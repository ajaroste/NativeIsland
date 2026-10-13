#pragma once
#include "app/admin_start.hpp"
#include <iostream>
inline int startup_tests(){
  auto fail=[](const char* stage){std::cerr<<"Startup test failed: "<<stage<<'\n';return 41;};
  if(!ni::guarded_service("test-success",[]{}))return fail("service success");
  if(ni::guarded_service("test-caught-standard",[]{throw std::runtime_error("fixture");}))return fail("standard worker containment");
  if(ni::guarded_service("test-caught-winrt",[]{throw winrt::hresult_error(E_ACCESSDENIED);}))return fail("WinRT worker containment");
  auto name=ni::startup_event_name();if(!ni::valid_startup_event(name)||ni::valid_startup_event(L"Local\\Other.Ready.123")||ni::valid_startup_event(name+L"extra"))return fail("ready-event validation");
  ni::StartupHandle ready(ni::create_startup_event(name));if(!ready.value)return fail("ready event creation and ACL");
  ni::StartupHandle sender(OpenEventW(EVENT_MODIFY_STATE,FALSE,name.c_str()));if(!sender.value||!SetEvent(sender.value)||WaitForSingleObject(ready.value,0)!=WAIT_OBJECT_0)return fail("ready handshake");
  ni::StartupHandle duplicate(ni::create_startup_event(name));if(duplicate.value||GetLastError()!=ERROR_ALREADY_EXISTS)return fail("event collision");
  ni::AdminHostArguments args;auto command=L"--admin-host --ui-pid 123 --ready-event "+name;
  if(!ni::parse_admin_host(command,args)||args.ui_pid!=123||args.ready_event!=name)return fail("host arguments");
  for(auto bad:{L"--admin-host",L"--admin-host --ui-pid -1 --ready-event ",L"--admin-host --ui-pid 4294967296 --ready-event ",L"--admin-host --ui-pid garbage --ready-event "})if(ni::parse_admin_host(std::wstring(bad)+name,args))return fail("invalid PID/arguments");
  if(ni::parse_admin_host(command+L" --extra",args)||ni::parse_admin_host(L"--admin-host --ui-pid 123 --ready-event Other",args))return fail("extra/untrusted event arguments");
  if(ni::valid_ui_process(GetCurrentProcess(),GetCurrentProcessId(),ni::startup_executable()))return fail("self-host rejection");
  auto image=ni::startup_executable();std::wstring quoted=L"\""+image+L"\" --startup-child";STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION child{};
  if(!CreateProcessW(image.c_str(),quoted.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&child))return fail("same-image child fixture");
  ni::StartupHandle process(child.hProcess),thread(child.hThread);
  bool valid=ni::valid_ui_process(process.value,child.dwProcessId,image);
  bool invalid=ni::valid_ui_process(process.value,child.dwProcessId,image+L".other");
  WaitForSingleObject(process.value,5000);
  if(!valid||invalid||ni::valid_ui_process(process.value,child.dwProcessId,image))return fail("same-image/session/liveness validation");
  std::cout<<"startup: restricted ready event, handshake, strict host arguments and process identity/liveness checks passed\n";return 0;
}
