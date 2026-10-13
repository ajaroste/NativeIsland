#include "ui/island_window.hpp"
#include <windows.h>
#include <objbase.h>
#include "app/admin_start.hpp"
#include <fstream>
#include "app/notification_probe.hpp"

int run_application(HINSTANCE instance, PWSTR command) {
  const std::wstring arguments(command);
  if(arguments==L"--windows-notification-probe")return ni::notification_probe();
  const bool handshake_test=arguments==L"--startup-handshake-test";
  const bool testing=arguments==L"--render-previews"||arguments==L"--smoke-test"||handshake_test;
  ni::StartupHandle starting;
  if(arguments.starts_with(L"--admin-host ")){ni::AdminHostArguments host;if(!ni::parse_admin_host(arguments,host))return ERROR_INVALID_PARAMETER;return ni::monitor_ui_host(host);}
  const bool runtime_test=arguments==L"--runtime-smoke-test";
  if(!testing&&!runtime_test&&arguments!=L"--desktop-ui"&&arguments!=L"--quit"){int start=ni::request_admin_start(starting);if(start!=-1)return start;}
  ni::diagnostic("ui-initializing");
  if(handshake_test){
    if(ni::elevated())return ERROR_BAD_TOKEN_TYPE;
    auto name=ni::startup_event_name();ni::StartupHandle ready(ni::create_startup_event(name));if(!ready.value)return 42;
    auto path=std::filesystem::current_path()/L"out"/L"startup-handshake.txt";std::error_code ec;std::filesystem::create_directories(path.parent_path(),ec);
    std::ofstream report(path);report<<GetCurrentProcessId()<<'\n';for(wchar_t character:name)report<<static_cast<char>(character);report<<'\n';report.close();
    if(WaitForSingleObject(ready.value,60000)!=WAIT_OBJECT_0)return 43;
  }
  HANDLE mutex = CreateMutexW(nullptr, TRUE, (testing||runtime_test)?L"Local\\NativeIsland.Verification":L"Local\\NativeIsland.SingleInstance");
  if (!mutex || GetLastError() == ERROR_ALREADY_EXISTS) {
    if(mutex)CloseHandle(mutex);
    if(std::wstring(command).find(L"--quit")!=std::wstring::npos){HWND window=FindWindowW(L"NativeIslandOverlay",L"Native Island");return window&&PostMessageW(window,WM_CLOSE,0,0)?0:2;}
    if(!testing)MessageBoxW(nullptr,L"NativeIsland zaten açık. Güncel sürümü açmadan önce sistem tepsisinden Çıkış seçin.",L"NativeIsland",MB_OK|MB_ICONINFORMATION);
    return 0;
  }
  const HRESULT com = OleInitialize(nullptr);
  if(FAILED(com)){ni::startup_error(L"Windows arayüz oturumu hazırlanamadı.",static_cast<DWORD>(com));CloseHandle(mutex);return 1;}
  int result;
  const bool previews=std::wstring(command).find(L"--render-previews")!=std::wstring::npos;
  { ni::diagnostic("ui-constructing");ni::IslandWindow app(instance);ni::diagnostic("ui-config-loaded");
    if(!app.create(testing)){result=1;if(!testing)MessageBoxW(nullptr,L"Arayüz oluşturulamadı. Windows grafik sürücüsü ve kullanıcı oturumunu kontrol edin. Kayıtlarınız korunur.",L"NativeIsland",MB_OK|MB_ICONERROR);}
    else if(previews)result=app.export_previews(std::filesystem::current_path()/L"out"/L"previews");
    else if(handshake_test)result=app.run_for(6000);
    else if(arguments==L"--smoke-test")result=app.run_for(30000);
    else if(runtime_test){ni::diagnostic("full-services-ui-ready");result=app.run_for(30000);}
    else result=app.run();
  }
  if (SUCCEEDED(com)) OleUninitialize();
  CloseHandle(mutex);
  ni::diagnostic("ui-exited",result);
  return result;
}

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR command,int) {
  ni::diagnostic("NativeIsland-0.7.8-start",ni::elevated()?1:0);
  std::set_terminate([]{
    unsigned long code=ERROR_UNHANDLED_EXCEPTION;
    try{if(auto error=std::current_exception())std::rethrow_exception(error);}
    catch(const winrt::hresult_error& error){code=static_cast<unsigned long>(error.code().value);}catch(...){}
    ni::diagnostic("fatal-terminate",code);
    MessageBoxW(nullptr,L"Beklenmeyen bir hata nedeniyle NativeIsland kapandı. Ayarlar ve hatırlatıcılar korunur. Ayrıntı: %LOCALAPPDATA%\\NativeIsland\\startup.log",L"NativeIsland",MB_OK|MB_ICONERROR);
    ExitProcess(code);
  });
  try{return run_application(instance,command);}
  catch(const winrt::hresult_error& error){ni::startup_error(L"Windows arayüz işlemi başarısız oldu.",static_cast<DWORD>(error.code().value));}
  catch(const std::exception& error){ni::diagnostic(typeid(error).name(),ERROR_UNHANDLED_EXCEPTION);ni::startup_error(L"NativeIsland beklenmeyen bir hatayla karşılaştı.",ERROR_UNHANDLED_EXCEPTION);}
  catch(...){ni::startup_error(L"NativeIsland beklenmeyen bir hatayla karşılaştı.",ERROR_UNHANDLED_EXCEPTION);}
  return 1;
}

