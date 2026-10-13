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
  const ni::WindowsNotice browser{3,103,L"Vivaldi",L"Bilnex Emir",L"Toplantı 15:00"};
  auto card=ni::windows_notice_presentation(browser,true);
  if(card.source!=L"Vivaldi"||card.title!=L"Bilnex Emir"||card.preview!=L"Toplantı 15:00")return 68;
  // A browser's text cannot be used to invent the identity of its web site.
  auto private_card=ni::windows_notice_presentation(browser,false);
  if(private_card.source!=L"Vivaldi"||private_card.title!=L"Yeni bildirim"||!private_card.preview.empty())return 69;
  auto unknown=ni::windows_notice_presentation({4,104,L"",L"",L"  Test\n\rmessage "},true);
  if(unknown.source!=L"Windows"||unknown.title!=L"Test message"||!unknown.preview.empty())return 70;
  auto long_card=ni::windows_notice_presentation({5,105,std::wstring(400,L'x'),std::wstring(400,L'y'),std::wstring(500,L'z')},true);
  if(long_card.source.size()>72||long_card.title.size()>160||long_card.preview.size()>240)return 71;
  auto config=ni::Config::parse("{\"windowsNotifications\":true,\"windowsNotificationContent\":false}");if(!config.windows_notifications||config.windows_notification_content||ni::Config{}.windows_notifications)return 67;
  return 0;
}
