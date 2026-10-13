#pragma once
#include "services/windows_notifications.hpp"
inline int windows_notification_tests(){
  ni::NotificationDelta delta;ni::WindowsNotice old{1,100,L"WhatsApp",L"Eski",L"Mesaj"},fresh{2,101,L"WhatsApp",L"Emir",L"Merhaba"};
  if(!delta.update({old}).empty())return 60;
  auto added=delta.update({old,fresh});if(added.size()!=1||added[0].id!=2||!delta.update({old,fresh}).empty())return 61;
  delta.update({old});if(delta.contains(2))return 62;
  old.created=102;if(delta.update({old}).size()!=1)return 63;
  delta.reset();if(!delta.update({fresh}).empty())return 64;
  if(ni::windows_notice_text(fresh,false)!=L"WhatsApp · Yeni bildirim"||ni::windows_notice_text(fresh,true).find(L"Merhaba")==std::wstring::npos)return 65;
  fresh.body=std::wstring(500,L'x');if(ni::windows_notice_text(fresh,true).size()>240)return 66;
  auto config=ni::Config::parse("{\"windowsNotifications\":true,\"windowsNotificationContent\":false}");if(!config.windows_notifications||config.windows_notification_content||ni::Config{}.windows_notifications)return 67;
  return 0;
}
