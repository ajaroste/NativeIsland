#pragma once
#include <windows.h>
#include <shellapi.h>
#include <sddl.h>
#include <userenv.h>
#include <objbase.h>
#include <string>
#include <filesystem>
#include <format>
#include "core/diagnostics.hpp"

namespace ni {
struct StartupHandle {
  HANDLE value{};
  ~StartupHandle(){if(value)CloseHandle(value);}
  StartupHandle()=default;
  explicit StartupHandle(HANDLE handle):value(handle){}
  StartupHandle(const StartupHandle&)=delete;
  StartupHandle& operator=(const StartupHandle&)=delete;
};
inline bool elevated(){StartupHandle token;TOKEN_ELEVATION elevation{};DWORD size{};return OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token.value)&&GetTokenInformation(token.value,TokenElevation,&elevation,sizeof(elevation),&size)&&elevation.TokenIsElevated;}
inline std::wstring startup_executable(){wchar_t executable[32768]{};DWORD length=GetModuleFileNameW(nullptr,executable,32768);return length&&length<32768?std::wstring(executable,length):L"";}
inline void startup_error(const wchar_t* stage,DWORD error){diagnostic("startup-error",error);auto text=std::format(L"{}\nWindows hata kodu: {}\n\nAyarlar ve hatırlatıcılar korunur. Tanılama: %LOCALAPPDATA%\\NativeIsland\\startup.log",stage,error);MessageBoxW(nullptr,text.c_str(),L"NativeIsland",MB_OK|MB_ICONERROR);}
inline std::wstring startup_event_name(){GUID id{};if(FAILED(CoCreateGuid(&id)))return {};wchar_t guid[40]{};StringFromGUID2(id,guid,40);return L"Local\\NativeIsland.Ready."+std::wstring(guid);}
inline bool valid_startup_event(const std::wstring& name){constexpr wchar_t prefix[]=L"Local\\NativeIsland.Ready.";if(!name.starts_with(prefix))return false;auto suffix=name.substr(std::size(prefix)-1);GUID id{};return suffix.size()==38&&SUCCEEDED(CLSIDFromString(suffix.c_str(),&id));}
// Restricted to the original user, Administrators and SYSTEM. The event only
// acknowledges the waiting admin host; no privileged command interface exists.
inline HANDLE create_startup_event(const std::wstring& name){
  if(!valid_startup_event(name)){SetLastError(ERROR_INVALID_PARAMETER);return nullptr;}
  StartupHandle token;DWORD bytes{};if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token.value))return nullptr;
  GetTokenInformation(token.value,TokenUser,nullptr,0,&bytes);if(!bytes)return nullptr;
  std::string buffer(bytes,'\0');if(!GetTokenInformation(token.value,TokenUser,buffer.data(),bytes,&bytes))return nullptr;
  LPWSTR sid{};if(!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(buffer.data())->User.Sid,&sid))return nullptr;
  auto sddl=L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;"+std::wstring(sid)+L")";LocalFree(sid);
  // Match the caller's integrity label as well as its user ACL. Otherwise a
  // sandboxed low-integrity original process cannot signal its own event.
  GetTokenInformation(token.value,TokenIntegrityLevel,nullptr,0,&bytes);if(!bytes)return nullptr;
  buffer.resize(bytes);if(!GetTokenInformation(token.value,TokenIntegrityLevel,buffer.data(),bytes,&bytes))return nullptr;
  if(!ConvertSidToStringSidW(reinterpret_cast<TOKEN_MANDATORY_LABEL*>(buffer.data())->Label.Sid,&sid))return nullptr;
  sddl+=L"S:(ML;;NW;;;"+std::wstring(sid)+L")";LocalFree(sid);
  PSECURITY_DESCRIPTOR descriptor{};if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(),SDDL_REVISION_1,&descriptor,nullptr))return nullptr;
  SECURITY_ATTRIBUTES security{sizeof(security),descriptor,FALSE};HANDLE event=CreateEventW(&security,TRUE,FALSE,name.c_str());DWORD error=GetLastError();LocalFree(descriptor);
  if(event&&error==ERROR_ALREADY_EXISTS){CloseHandle(event);SetLastError(ERROR_ALREADY_EXISTS);return nullptr;}return event;
}
struct AdminHostArguments{DWORD ui_pid{};std::wstring ready_event;};
inline bool parse_admin_host(const std::wstring& arguments,AdminHostArguments& result){
  int count{};auto command=L"NativeIsland.exe "+arguments;LPWSTR* argv=CommandLineToArgvW(command.c_str(),&count);if(!argv)return false;
  bool valid=count==6&&std::wstring(argv[1])==L"--admin-host"&&std::wstring(argv[2])==L"--ui-pid"&&std::wstring(argv[4])==L"--ready-event";
  if(valid){wchar_t* end{};auto pid=wcstoull(argv[3],&end,10);valid=argv[3][0]!=L'-'&&end&&*end==0&&pid>0&&pid<=MAXDWORD&&valid_startup_event(argv[5]);if(valid){result.ui_pid=static_cast<DWORD>(pid);result.ready_event=argv[5];}}
  LocalFree(argv);return valid;
}
inline bool valid_ui_process(HANDLE process,DWORD pid,const std::wstring& executable){
  wchar_t image[32768]{};DWORD size=32768,ui_session{},our_session{},exit_code{};
  return pid!=GetCurrentProcessId()&&QueryFullProcessImageNameW(process,0,image,&size)&&_wcsicmp(image,executable.c_str())==0&&ProcessIdToSessionId(pid,&ui_session)&&ProcessIdToSessionId(GetCurrentProcessId(),&our_session)&&ui_session==our_session&&GetExitCodeProcess(process,&exit_code)&&exit_code==STILL_ACTIVE;
}
inline int monitor_ui_host(const AdminHostArguments& arguments){
  if(!elevated())return ERROR_ELEVATION_REQUIRED;
  StartupHandle process(OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,arguments.ui_pid));if(!process.value||!valid_ui_process(process.value,arguments.ui_pid,startup_executable()))return ERROR_INVALID_PARAMETER;
  StartupHandle mutex(CreateMutexW(nullptr,TRUE,L"Local\\NativeIsland.AdminHost"));DWORD error=GetLastError();if(!mutex.value||error==ERROR_ALREADY_EXISTS)return ERROR_ALREADY_EXISTS;
  StartupHandle ready(OpenEventW(EVENT_MODIFY_STATE,FALSE,arguments.ready_event.c_str()));if(!ready.value)return static_cast<int>(GetLastError());if(!SetEvent(ready.value))return static_cast<int>(GetLastError());
  WaitForSingleObject(process.value,INFINITE);return 0;
}
// -1 means keep THIS original unelevated process and start its UI. Normal
// startup no longer requires Explorer, shell COM, or another desktop UI process.
inline int request_admin_start(StartupHandle& starting){
  StartupHandle existing(OpenMutexW(SYNCHRONIZE,FALSE,L"Local\\NativeIsland.SingleInstance"));if(existing.value){MessageBoxW(nullptr,L"NativeIsland zaten açık. Sistem tepsisindeki simgesini kullanın.",L"NativeIsland",MB_OK|MB_ICONINFORMATION);return 0;}
  if(elevated()){
    diagnostic("already-elevated-direct-ui");
    MessageBoxW(nullptr,L"Ada yönetici yetkisiyle açılacak. Bu modda Windows, Explorer'dan dosya sürüklemeyi engelleyebilir. Tam sürükle-bırak için kısayolun ve EXE'nin 'Yönetici olarak çalıştır' uyumluluk seçeneğini kapatıp normal açın.",L"NativeIsland",MB_OK|MB_ICONWARNING);
    return -1;
  }
  auto fallback=[](DWORD error){diagnostic("admin-host-unavailable-ui-continues",error);if(error!=ERROR_CANCELLED)MessageBoxW(nullptr,L"Yönetici yardımcı süreci başlatılamadı. Ada normal kullanıcı yetkisiyle açılıyor; ayarlar ve hatırlatıcılar korunur.",L"NativeIsland",MB_OK|MB_ICONWARNING);return -1;};
  auto executable=startup_executable();if(executable.empty())return fallback(GetLastError());
  starting.value=CreateMutexW(nullptr,TRUE,L"Local\\NativeIsland.Starting");DWORD error=GetLastError();if(!starting.value||error==ERROR_ALREADY_EXISTS){MessageBoxW(nullptr,L"NativeIsland başlatılıyor. Yönetici izin ekranını kontrol edin.",L"NativeIsland",MB_OK|MB_ICONINFORMATION);return 0;}
  auto event_name=startup_event_name();StartupHandle ready(create_startup_event(event_name));if(!ready.value)return fallback(GetLastError());
  auto arguments=std::format(L"--admin-host --ui-pid {} --ready-event {}",GetCurrentProcessId(),event_name);
  SHELLEXECUTEINFOW info{sizeof(info)};info.fMask=SEE_MASK_NOCLOSEPROCESS;info.lpVerb=L"runas";info.lpFile=executable.c_str();info.lpParameters=arguments.c_str();info.nShow=SW_HIDE;
  if(!ShellExecuteExW(&info))return fallback(GetLastError());
  StartupHandle host(info.hProcess);if(!host.value)return fallback(ERROR_INVALID_HANDLE);
  HANDLE handles[]{ready.value,host.value};DWORD wait=WaitForMultipleObjects(2,handles,FALSE,15000);if(wait==WAIT_OBJECT_0)return -1;
  error=wait==WAIT_TIMEOUT?ERROR_TIMEOUT:GetLastError();if(wait==WAIT_OBJECT_0+1)GetExitCodeProcess(host.value,&error);return fallback(error);
}
}
