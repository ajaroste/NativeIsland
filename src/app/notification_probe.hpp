#pragma once
#include "core/config.hpp"
#include "core/diagnostics.hpp"
#include "services/windows_notifications.hpp"
#include <windows.h>
#include <appmodel.h>
#include <objbase.h>
#include <fstream>
namespace ni {
inline int notification_probe(){
  std::error_code error;std::filesystem::create_directories(Config::path().parent_path(),error);
  std::ofstream report(Config::path().parent_path()/L"windows-notification-probe.txt");
  UINT32 length{};LONG identity=GetCurrentPackageFullName(&length,nullptr);
  diagnostic("windows-notification-package-identity",static_cast<unsigned long>(identity));
  report<<"Package identity: "<<(identity==ERROR_INSUFFICIENT_BUFFER?"present":"absent")<<'\n';
  HRESULT initialized=OleInitialize(nullptr);if(FAILED(initialized)){report<<"COM unavailable\n";return 1;}
  int result=0;
  try{
    auto status=winrt::Windows::UI::Notifications::Management::UserNotificationListener::Current().GetAccessStatus();
    diagnostic("windows-notification-access-status",static_cast<unsigned long>(status));
    report<<"Listener access: "<<static_cast<int>(status)<<" (0=unspecified, 1=allowed, 2=denied)\n";
  }catch(const winrt::hresult_error& failure){diagnostic("windows-notification-probe-error",static_cast<unsigned long>(failure.code().value));report<<"Listener HRESULT: "<<std::hex<<failure.code().value<<'\n';result=2;}
  OleUninitialize();return result;
}
}
