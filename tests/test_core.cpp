#include "animation/spring.hpp"
#include "core/layout.hpp"
#include "core/config.hpp"
#include "ui/theme.hpp"
#include "animation/animation_manager.hpp"
#include "ui/compact_layout.hpp"
#include "files/drag_source.hpp"
#include "services/app_launcher.hpp"
#include "services/clipboard_history.hpp"
#include "ui/notification_motion.hpp"
#include <filesystem>
#include <chrono>
#include <thread>
#include <cmath>
#include <iostream>
#include "test_reminders.hpp"
#include "test_windows_notifications.hpp"
#include "test_startup.hpp"
#include "test_config_reopen.hpp"
#include "test_tasks.hpp"

int main(int argc,char** argv) {
  if(argc==2&&std::string(argv[1])=="--startup-child"){Sleep(1500);return 0;}
  if(argc==2&&std::string(argv[1])=="--config-probe"){
    try{auto config=ni::Config::load();std::cout<<"Config load passed; horizontalPercent="<<config.horizontal_percent<<" lastPage="<<config.last_page<<'\n';return 0;}
    catch(const std::exception& error){std::cerr<<"Config load exception: "<<error.what()<<'\n';return 51;}
    catch(...){std::cerr<<"Unknown config load exception\n";return 52;}
  }
  winrt::init_apartment(winrt::apartment_type::single_threaded);
  if(int result=startup_tests())return result;
  if(int result=config_reopen_tests())return result;
  if(int result=reminder_tests())return result;
  if(int result=windows_notification_tests())return result;
  if(int result=tasks_tests())return result;
  if(ni::notification_shake(1,.1f,true)!=0||ni::notification_shake(1,.4f,false)!=0||std::abs(ni::notification_shake(1,.05f,false))>3)return 31;
  for(int style=0;style<4;++style){if(ni::AnimationManager::ease(0,style)!=0||ni::AnimationManager::ease(1,style)!=1)return 23;float previous=0;for(int i=0;i<=1000;++i){float value=ni::AnimationManager::ease(i/1000.f,style);if(!std::isfinite(value)||value<0||value>1.05f)return 24;if(style==0||style==2){if(value<previous||value>1)return 25;}previous=value;}}
  auto anim_config=ni::Config::parse("{\"animationStyle\":8,\"animationDuration\":900}");if(anim_config.animation_style!=3||anim_config.animation_duration_ms!=600)return 26;
  if(ni::page_height(ni::Page::media,0,0,false,false)>400 || ni::page_height(ni::Page::files,1,0,false,false)>=250)return 10;
  {std::vector<ni::ClipboardEntry> clips(3);clips[0].text=L"Hello world";clips[0].pinned=true;clips[0].created_at=0;clips[1].text=L"Another";clips[1].created_at=0;clips[2].text=L"HELLO again";clips[2].created_at=60000;
    auto matches=ni::clipboard_matches(clips,L"hello");if(matches!=std::vector<size_t>{0,2})return 27;
    if(!ni::expire_clipboard(clips,90000,1,false)||clips.size()!=2||!clips[0].pinned)return 28;
    if(!ni::expire_clipboard(clips,90000,1,true)||clips.size()!=1)return 29;
    if(ni::expire_clipboard(clips,999999,0,true))return 30;}
  if(ni::page_height(ni::Page::files,32,0,false,false)>330)return 11;
  const HRESULT apartment=OleInitialize(nullptr);
  {
    ni::ClipboardEntry image;BITMAPINFOHEADER header{};header.biSize=40;header.biWidth=2;header.biHeight=2;header.biPlanes=1;header.biBitCount=24;header.biSizeImage=16;
    image.dib.resize(56,0xff);memcpy(image.dib.data(),&header,40);
    if(!ni::decode_clip_image(image)||image.width!=2||image.height!=2||image.pixels.size()!=16)return 17;
    image.dib.resize(12);if(ni::decode_clip_image(image))return 18;
    ni::ClipboardEntry text;text.text.assign(32000,L'A');if(text.text.size()!=32000||text.image())return 19;
    if(ni::page_height(ni::Page::applications,4,0,false,false)!=238||ni::page_height(ni::Page::applications,8,0,false,false)!=350)return 20;
    auto config=ni::Config::parse("{\"horizontalPercent\":200,\"monitorIndex\":2,\"hotkeyKey\":75,\"hotkeyModifiers\":6}");
    if(config.horizontal_percent!=100||config.monitor_index!=2||config.hotkey_key!=75||config.hotkey_modifiers!=6)return 21;
  }
  {
    ni::ComPtr<IDataObject> data;
    const auto folder=std::filesystem::path(NI_SOURCE_DIR)/L"tests"/L"fixtures"/L"ShelfFolder";
    HRESULT drag_result=ni::file_data_object(folder.wstring(),data);
    if(FAILED(drag_result)){std::cerr<<"Shell drag data object failed: "<<std::hex<<static_cast<unsigned long>(drag_result)<<"\n";return 12;}
    FORMATETC format{CF_HDROP,nullptr,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};STGMEDIUM medium{};
    if(FAILED(data->GetData(&format,&medium)))return 13;
    wchar_t path[2048]{};DragQueryFileW(static_cast<HDROP>(medium.hGlobal),0,path,2048);
    const bool matches=std::filesystem::path(path)==folder;ReleaseStgMedium(&medium);if(!matches)return 14;
    ni::AppLauncher apps;apps.start([]{},false);wchar_t executable[2048]{};GetModuleFileNameW(nullptr,executable,2048);apps.add(executable);
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(4);
    while(apps.snapshot().empty()&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto shortcuts=apps.snapshot();if(shortcuts.size()!=1||shortcuts.front().pixels.empty()){std::cerr<<"Application icon loading failed\n";return 15;}
    apps.remove(0);deadline=std::chrono::steady_clock::now()+std::chrono::seconds(4);
    while(!apps.snapshot().empty()&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(10));
    if(!apps.snapshot().empty())return 16;
    apps.stop();
  }
  if(SUCCEEDED(apartment))OleUninitialize();
  const auto corrupt=ni::Config::parse("{\"theme\":999999999999999999999999999}");
  const auto malformed=ni::Config::parse("{\"theme\":4 garbage}");
  const auto configured=ni::Config::parse("{\"theme\":5,\"topGap\":70,\"opacity\":2,\"clipboardHistory\":true}");
  if(corrupt.theme_index!=0||malformed.theme_index!=0||configured.theme_index!=5||configured.top_gap!=40||configured.opacity_percent!=50||!configured.clipboard_history)return 6;
  const auto migrated=ni::Config::parse("{\"collapsedWidth\":196,\"expandedHeight\":154}");
  const auto behavior=ni::Config::parse("{\"pinOnClick\":false,\"startWithWindows\":true}");
  if(behavior.pin_on_click||!behavior.start_with_windows||!ni::Config::parse("{}").pin_on_click)return 22;
  if(migrated.collapsed_width!=228||migrated.expanded_height!=334)return 7;
  if(ni::themes[5].text!=0x1A2432||ni::theme_for(2,0x112233).accent!=0x112233)return 8;
  if(std::abs(ni::AnimationManager::ease(0))>.001f||std::abs(ni::AnimationManager::ease(1)-1)>.001f)return 9;
  static_assert(ni::layout::scale_dip(228,120)==285);
  static_assert(ni::layout::scale_dip(465,144)==698);
  static_assert(ni::layout::scale_dip(170, 96) == 170);
  static_assert(ni::layout::scale_dip(170, 120) == 213);
  static_assert(ni::layout::scale_dip(170, 144) == 255);
  static_assert(ni::layout::centered_left(0, 1920, 170) + 170 / 2 == 1920 / 2);
  static_assert(ni::layout::centered_left(0, 1920, 440) + 440 / 2 == 1920 / 2);
  static_assert(ni::layout::top_gap(-1) == 0 && ni::layout::top_gap(8) == 8 && ni::layout::top_gap(99) == 40);
  ni::Spring spring; spring.reset(100.0f); spring.target(400.0f);
  for (int i = 0; i < 600; ++i) spring.update(1.0f / 120.0f);
  if (!spring.settled() || std::abs(spring.value() - 400.0f) > 0.1f) {
    std::cerr << "spring convergence failed\n"; return 1;
  }
  spring.target(196.0f);
  for (int i = 0; i < 600; ++i) spring.update(1.0f / 144.0f);
  if (!spring.settled() || std::abs(spring.value() - 196.0f) > 0.1f) {
    std::cerr << "spring reverse convergence failed\n"; return 2;
  }
  std::cout << "core tests passed\n";
  return 0;
}

