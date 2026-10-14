#pragma once
#include "services/windows_notifications.hpp"
#include "ui/notification_layout.hpp"
#include "services/notification_icon_resolver.hpp"
#include <windows.h>
#include <filesystem>
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
  const auto short_notice=ni::compact_notice_size(L"LanDeck.Services.exe",L"LanDeck",L"Test");
  if(short_notice.width>360||short_notice.height!=96)return 72;
  const auto hidden=ni::compact_notice_size(L"WhatsApp",L"Yeni bildirim",L"");
  if(hidden.height!=76||hidden.width<336)return 73;
  const auto long_notice=ni::compact_notice_size(L"WhatsApp",L"Arkadaş",std::wstring(220,L'x'));
  if(long_notice.height!=142||long_notice.width>500)return 74;
  const auto two_lines=ni::compact_notice_size(L"Vivaldi",L"Bir mesaj",std::wstring(62,L'x'),true);
  if(two_lines.height!=120)return 75;
  auto text_only=ni::notice_image_layout(false,false),app_only=ni::notice_image_layout(false,true),with_portrait=ni::notice_image_layout(true,true);
  if(text_only.text_left!=56||text_only.source_left!=text_only.text_left||text_only.portrait_visible||app_only.text_left!=56||app_only.source_left!=app_only.text_left||app_only.portrait_visible||with_portrait.text_left!=76||with_portrait.source_left!=with_portrait.text_left||!with_portrait.portrait_visible)return 82;
  auto no_photo=ni::compact_notice_size(L"Vivaldi",L"Bir mesaj",std::wstring(62,L'x'));if(no_photo.width>=two_lines.width)return 83;
  ni::WindowsNotice portrait_notice=fresh;portrait_notice.portrait=std::make_shared<const std::vector<std::uint8_t>>(16,std::uint8_t{});
  if(ni::windows_notice_presentation(portrait_notice,false).portrait||ni::windows_notice_presentation(portrait_notice,true).portrait!=portrait_notice.portrait)return 84;
  portrait_notice.portrait=std::make_shared<const std::vector<std::uint8_t>>(512*1024+1,std::uint8_t{});if(ni::windows_notice_presentation(portrait_notice,true).portrait)return 85;
  if(ni::notification_icon_key(L"Foo.AUMID",L"Vivaldi")!=L"Foo.AUMID")return 76;
  if(ni::notification_icon_key(L"",L"Vivaldi")!=L"@desktop:Vivaldi")return 77;
  if(!ni::notification_icon_key(L"",L"").empty())return 78;
  // Integration smoke test: extract and decode the icon of this very process.
  // This executes the real Win32 process and shell fallback on the CI machine.
  wchar_t self[MAX_PATH]{};
  if(!GetModuleFileNameW(nullptr,self,MAX_PATH))return 79;
  auto image=ni::notification_shell_icon(L"",std::filesystem::path(self).filename().wstring());
  if(image.size()<12||image[0]!='N'||image[1]!='I'||image[2]!='R'||image[3]!='G')return 80;
  auto u32=[&](size_t at)->std::uint32_t {
    return std::uint32_t(image[at])|(std::uint32_t(image[at+1])<<8)|
           (std::uint32_t(image[at+2])<<16)|(std::uint32_t(image[at+3])<<24);
  };
  auto w=u32(4),h=u32(8);
  if(w==0||h==0||w>128||h>128||image.size()!=12+static_cast<size_t>(w)*h*4)return 81;
  auto config=ni::Config::parse("{\"windowsNotifications\":true,\"windowsNotificationContent\":false}");if(!config.windows_notifications||config.windows_notification_content||ni::Config{}.windows_notifications)return 67;
  return 0;
}
