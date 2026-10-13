#include "services/notification_icon_resolver.hpp"
#include "core/com_ptr.hpp"
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <wincodec.h>
#include <tlhelp32.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cwchar>
#include <cwctype>
#include <cstring>

namespace ni {
namespace {
std::wstring registered_icon(const std::wstring& app_id){
  if(app_id.empty()||app_id.size()>256||app_id.find_first_of(L"\\/\r\n")!=std::wstring::npos)return {};
  std::wstring key=L"AppUserModelId\\"+app_id;
  wchar_t value[4096]{};
  DWORD bytes=sizeof(value);
  if(RegGetValueW(HKEY_CLASSES_ROOT,key.c_str(),L"IconUri",RRF_RT_REG_SZ|RRF_RT_REG_EXPAND_SZ,
                  nullptr,value,&bytes)!=ERROR_SUCCESS)return {};
  return value;
}

std::vector<std::uint8_t> icon_file_bytes(const std::wstring& location){
  if(location.empty()||location.size()>4095)return {};
  // Win32 toast registrations normally use a local PNG/ICO file path.
  std::filesystem::path path(location);
  auto extension=path.extension().wstring();
  std::transform(extension.begin(),extension.end(),extension.begin(),::towlower);
  if(extension!=L".png"&&extension!=L".ico"&&extension!=L".jpg"&&
     extension!=L".jpeg"&&extension!=L".bmp")return {};
  std::ifstream in(path,std::ios::binary|std::ios::ate);
  if(!in)return {};
  const auto size=in.tellg();
  if(size<=0||size>512*1024)return {};
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
  in.seekg(0);
  if(!in.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())))return {};
  return bytes;
}

std::vector<std::uint8_t> icon_pixels(HICON handle){
  if(!handle)return {};
  ComPtr<IWICImagingFactory> factory;
  ComPtr<IWICBitmap> bitmap;
  ComPtr<IWICFormatConverter> converter;
  if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,
            __uuidof(IWICImagingFactory),reinterpret_cast<void**>(factory.put())))||
     FAILED(factory->CreateBitmapFromHICON(handle,bitmap.put()))||
     FAILED(factory->CreateFormatConverter(converter.put()))||
     FAILED(converter->Initialize(bitmap.get(),GUID_WICPixelFormat32bppPBGRA,
             WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))return {};
  UINT width{},height{};
  if(FAILED(converter->GetSize(&width,&height))||!width||!height||
     width>128||height>128)return {};
  const auto length=static_cast<std::size_t>(width)*height*4;
  // NIRG + little-endian width/height + premultiplied 32bpp BGRA pixels.
  std::vector<std::uint8_t> result(12+length);
  result[0]='N';result[1]='I';result[2]='R';result[3]='G';
  std::memcpy(result.data()+4,&width,4);
  std::memcpy(result.data()+8,&height,4);
  if(FAILED(converter->CopyPixels(nullptr,width*4,static_cast<UINT>(length),result.data()+12)))return {};
  return result;
}

std::vector<std::uint8_t> file_shell_icon(const std::wstring& path){
  if(path.empty()||path.size()>4095)return {};
  SHFILEINFOW info{};
  if(!SHGetFileInfoW(path.c_str(),0,&info,sizeof(info),SHGFI_ICON|SHGFI_LARGEICON))return {};
  auto pixels=icon_pixels(info.hIcon);
  if(info.hIcon)DestroyIcon(info.hIcon);
  return pixels;
}

std::vector<std::uint8_t> apps_folder_icon(const std::wstring& app_id){
  if(app_id.empty()||app_id.size()>256||app_id.find_first_of(L"\\/\r\n")!=std::wstring::npos)return {};
  const std::wstring name=L"shell:AppsFolder\\"+app_id;
  PIDLIST_ABSOLUTE pidl{};
  if(FAILED(SHParseDisplayName(name.c_str(),nullptr,&pidl,0,nullptr))||!pidl)return {};
  SHFILEINFOW info{};
  const auto got=SHGetFileInfoW(reinterpret_cast<LPCWSTR>(pidl),0,&info,sizeof(info),
                               SHGFI_PIDL|SHGFI_ICON|SHGFI_LARGEICON);
  CoTaskMemFree(pidl);
  if(!got)return {};
  auto pixels=icon_pixels(info.hIcon);
  if(info.hIcon)DestroyIcon(info.hIcon);
  return pixels;
}

std::wstring matching_process_executable(std::wstring display_name){
  if(display_name.empty()||display_name.size()>125||
     display_name.find_first_of(L"\\/:*?\"<>|\r\n")!=std::wstring::npos)return {};
  if(display_name.size()<4||_wcsicmp(display_name.c_str()+display_name.size()-4,L".exe"))
    display_name+=L".exe";
  HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
  if(snapshot==INVALID_HANDLE_VALUE)return {};
  PROCESSENTRY32W entry{sizeof(entry)};
  std::wstring result;
  if(Process32FirstW(snapshot,&entry))do{
    if(_wcsicmp(entry.szExeFile,display_name.c_str()))continue;
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,entry.th32ProcessID);
    if(!process)continue;
    wchar_t filename[4096]{};
    DWORD length=4096;
    if(QueryFullProcessImageNameW(process,0,filename,&length))result.assign(filename,length);
    CloseHandle(process);
    if(!result.empty())break;
  }while(Process32NextW(snapshot,&entry));
  CloseHandle(snapshot);
  return result;
}
}

std::vector<std::uint8_t> notification_shell_icon(const std::wstring& app_id,
                                                    const std::wstring& display_name){
  // Prefer the identity Windows uses for the toast's own attribution area.
  const auto location=registered_icon(app_id);
  if(auto data=icon_file_bytes(location);!data.empty())return data;
  // Some Win32 apps register an icon resource in their executable instead.
  if(!location.empty()){
    if(auto data=file_shell_icon(location);!data.empty())return data;
  }
  if(auto data=apps_folder_icon(app_id);!data.empty())return data;
  // Last resort for a running classic EXE: exact executable-name match only.
  auto process_path=matching_process_executable(display_name);
  if(!process_path.empty())return file_shell_icon(process_path);
  return {};
}
}
