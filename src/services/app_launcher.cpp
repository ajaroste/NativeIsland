#include "services/app_launcher.hpp"
#include "core/diagnostics.hpp"
#include "core/config.hpp"
#include "core/com_ptr.hpp"
#include <windows.h>
#include <shellapi.h>
#include <wincodec.h>
#include <fstream>
#include <algorithm>

namespace ni {
namespace {
std::string utf8(const std::wstring& value) {
  int bytes=WideCharToMultiByte(CP_UTF8,0,value.data(),static_cast<int>(value.size()),nullptr,0,nullptr,nullptr);
  std::string out(bytes,'\0');WideCharToMultiByte(CP_UTF8,0,value.data(),static_cast<int>(value.size()),out.data(),bytes,nullptr,nullptr);return out;
}
std::wstring wide(const std::string& value) {
  int chars=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),nullptr,0);
  std::wstring out(chars,L'\0');if(chars)MultiByteToWideChar(CP_UTF8,0,value.data(),static_cast<int>(value.size()),out.data(),chars);return out;
}
AppShortcut shortcut(std::wstring path) {
  path=std::filesystem::path(path).make_preferred().wstring();
  AppShortcut app;app.path=std::move(path);app.name=std::filesystem::path(app.path).stem().wstring();
  SHFILEINFOW info{};
  if(SHGetFileInfoW(app.path.c_str(),0,&info,sizeof(info),SHGFI_ICON|SHGFI_LARGEICON)) {
    ComPtr<IWICImagingFactory> factory;ComPtr<IWICBitmap> bitmap;ComPtr<IWICFormatConverter> converter;
    if(SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,__uuidof(IWICImagingFactory),reinterpret_cast<void**>(factory.put())))&&
       SUCCEEDED(factory->CreateBitmapFromHICON(info.hIcon,bitmap.put()))&&SUCCEEDED(factory->CreateFormatConverter(converter.put()))&&
       SUCCEEDED(converter->Initialize(bitmap.get(),GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom))) {
      converter->GetSize(&app.width,&app.height);
      if(app.width<=256&&app.height<=256){app.pixels.resize(app.width*app.height*4);if(FAILED(converter->CopyPixels(nullptr,app.width*4,static_cast<UINT>(app.pixels.size()),app.pixels.data())))app.pixels.clear();}
    }
    if(info.hIcon)DestroyIcon(info.hIcon);
  }
  return app;
}
}
AppLauncher::~AppLauncher(){stop();}
AppShortcut shell_icon(std::wstring path){return shortcut(std::move(path));}
void AppLauncher::start(std::function<void()> changed,bool persistence){changed_=std::move(changed);persistence_=persistence;worker_=std::jthread([this](std::stop_token stop){guarded_service("applications-worker-exception",[&]{run(stop);});});}
void AppLauncher::stop(){if(worker_.joinable()){worker_.request_stop();cv_.notify_all();worker_.join();}}
void AppLauncher::add(std::wstring path){std::scoped_lock lock(mutex_);additions_.push_back(std::move(path));cv_.notify_one();}
void AppLauncher::remove(size_t index){std::scoped_lock lock(mutex_);removals_.push_back(index);cv_.notify_one();}
void AppLauncher::launch(size_t index){std::scoped_lock lock(mutex_);if(index<apps_.size()){launches_.push_back(apps_[index].path);cv_.notify_one();}}
std::vector<AppShortcut> AppLauncher::snapshot()const{std::scoped_lock lock(mutex_);return apps_;}
void AppLauncher::run(std::stop_token stop) {
  HRESULT apartment=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
  auto file=Config::path().parent_path()/L"applications.txt";
  std::vector<AppShortcut> current;
  if(persistence_){std::ifstream in(file,std::ios::binary);std::string line;while(current.size()<32&&std::getline(in,line)){auto path=wide(line);if(!path.empty())current.push_back(shortcut(path));}}
  {std::scoped_lock lock(mutex_);apps_=current;}if(changed_)changed_();
  while(!stop.stop_requested()) {
    std::vector<std::wstring> additions,launches;std::vector<size_t> removals;
    {std::unique_lock lock(mutex_);cv_.wait(lock,stop,[this]{return !additions_.empty()||!removals_.empty()||!launches_.empty();});additions.swap(additions_);removals.swap(removals_);launches.swap(launches_);}
    bool changed=false;
    for(auto& path:additions){auto extension=std::filesystem::path(path).extension().wstring();std::transform(extension.begin(),extension.end(),extension.begin(),::towlower);
      if((extension==L".exe"||extension==L".lnk"||extension==L".appref-ms")&&current.size()<32&&std::none_of(current.begin(),current.end(),[&](auto& app){return app.path==path;})){current.push_back(shortcut(std::move(path)));changed=true;}}
    std::sort(removals.rbegin(),removals.rend());removals.erase(std::unique(removals.begin(),removals.end()),removals.end());
    for(auto i:removals)if(i<current.size()){current.erase(current.begin()+i);changed=true;}
    if(changed){
      {std::scoped_lock lock(mutex_);apps_=current;}
      if(persistence_){std::error_code error;std::filesystem::create_directories(file.parent_path(),error);auto temp=file;temp+=L".tmp";std::ofstream out(temp,std::ios::binary);for(auto& app:current)out<<utf8(app.path)<<'\n';out.flush();bool ok=out.good();out.close();if(ok)MoveFileExW(temp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);}
      if(changed_)changed_();
    }
    for(auto& path:launches)ShellExecuteW(nullptr,L"open",path.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
  }
  if(SUCCEEDED(apartment))CoUninitialize();
}
}
