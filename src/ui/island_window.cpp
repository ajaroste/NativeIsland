#include "ui/island_window.hpp"
#include "core/layout.hpp"
#include <windowsx.h>
#include <dwmapi.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <commctrl.h>
#include <shlobj.h>
#include <fstream>
#include <memory>
#include "ui/compact_layout.hpp"
#include "files/drag_source.hpp"
#include "services/clipboard_history.hpp"
#include "ui/island_shape.hpp"
#include "ui/notification_layout.hpp"

namespace ni {
namespace {
constexpr wchar_t kClassName[] = L"NativeIslandOverlay";
constexpr UINT kTelemetry = WM_APP + 1, kTray = WM_APP + 2, kCalendar=WM_APP+3, kAnimate = 1, kFullscreen = 2, kCollapseDelay = 3, kPageFade = 4, kAutoExit=5,kWaves=6,kNotice=7,kReminders=8,kReminderDelivery=9;
enum : UINT { CmdToggle = 100, CmdHover, CmdClipboard, CmdStartup, CmdClearFiles, CmdExit, CmdPlay, CmdPrev, CmdNext,CmdHide,CmdSettings,CmdReminders };
constexpr UINT kWindowsNotifications=10;
struct MonitorSelect { int requested{}; int non_primary{1}; HMONITOR result{}; };
BOOL CALLBACK monitor_at_index(HMONITOR monitor, HDC, LPRECT, LPARAM data) {
  auto* value = reinterpret_cast<MonitorSelect*>(data);
  MONITORINFO info{sizeof(info)}; GetMonitorInfoW(monitor, &info);
  if ((value->requested == 0 && (info.dwFlags & MONITORINFOF_PRIMARY)) ||
      (value->requested > 0 && !(info.dwFlags & MONITORINFOF_PRIMARY) && value->non_primary == value->requested)) {
    value->result = monitor; return FALSE;
  }
  if (!(info.dwFlags & MONITORINFOF_PRIMARY)) ++value->non_primary;
  return TRUE;
}
int dip_to_px(int dip, UINT dpi) { return layout::scale_dip(dip, dpi); }
}

IslandWindow::IslandWindow(HINSTANCE instance) : instance_(instance), config_(Config::load()) {
  view_.page=static_cast<Page>(config_.remember_page?config_.last_page:0);
}
IslandWindow::~IslandWindow() { reminder_window_.close();google_calendar_.stop();search_.close();audio_.stop();settings_.close();apps_.stop();monitor_.stop(); media_.stop(); remove_tray(); }

bool IslandWindow::create(bool verification) {
  verification_=verification;
  if(!verification)audio_favorites_.load();
  if(config_.start_with_windows&&!verification)set_startup(true);
  INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_BAR_CLASSES|ICC_WIN95_CLASSES};InitCommonControlsEx(&controls);
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  WNDCLASSEXW wc{sizeof(wc)}; wc.hInstance = instance_; wc.lpfnWndProc = window_proc; wc.lpszClassName = kClassName;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); wc.hbrBackground = nullptr;
  if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
  window_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_NOREDIRECTIONBITMAP, kClassName, L"Native Island",
    WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, instance_, this);
  if (!window_) return false;
  dpi_ = GetDpiForWindow(window_);
  width_.reset(static_cast<float>(dip_to_px(config_.collapsed_width, dpi_)));
  height_.reset(static_cast<float>(dip_to_px(config_.collapsed_height, dpi_)));
  position();
  RECT initial_client{};GetClientRect(window_,&initial_client);
  if (FAILED(renderer_.initialize(window_,initial_client.right,initial_client.bottom,static_cast<float>(dpi_))))return false;
  BOOL dark = TRUE; DwmSetWindowAttribute(window_, 20, &dark, sizeof(dark));
  drop_target_=new DropTarget;
  drop_target_->hovering=[this](bool active){view_.drop_active=active;if(active){KillTimer(window_,kCollapseDelay);view_.page=Page::files;view_.page_opacity=1;view_.scroll=0;KillTimer(window_,kPageFade);set_expanded(true);}else schedule_collapse();render();};
  drop_target_->dropped=[this](std::vector<std::wstring> paths){for(auto& path:paths)if(files_.size()<32&&std::find(files_.begin(),files_.end(),path)==files_.end())files_.push_back(std::move(path));view_.page=Page::files;view_.scroll=0;set_expanded(true,true);render();};
  if(FAILED(RegisterDragDrop(window_,drop_target_))){drop_target_->Release();drop_target_=nullptr;DragAcceptFiles(window_,TRUE);}
  add_tray();
  if(!verification&&config_.hotkey_key&&!RegisterHotKey(window_,1,config_.hotkey_modifiers|MOD_NOREPEAT,config_.hotkey_key))MessageBoxW(window_,L"Gizleme kısayolu başka bir uygulama tarafından kullanılıyor. Ayarlardan farklı bir kısayol seçin.",L"NativeIsland",MB_OK|MB_ICONWARNING);
  if (config_.clipboard_history&&!verification) AddClipboardFormatListener(window_);
  ShowWindow(window_, SW_SHOWNOACTIVATE); render();
  monitor_.start([hwnd = window_] { PostMessageW(hwnd, kTelemetry, 0, 0); });
  media_.start([hwnd = window_] { PostMessageW(hwnd, kTelemetry, 0, 0); });
  audio_.start([hwnd=window_]{PostMessageW(hwnd,kTelemetry,0,0);});
  apps_.start([hwnd=window_]{PostMessageW(hwnd,kTelemetry,0,0);},!verification);
  if(!verification)reminders_.load();
  google_calendar_.start(reminders_,[hwnd=window_]{PostMessageW(hwnd,kCalendar,0,0);},!verification);
  if(!verification){windows_notifications_.enable(config_.windows_notifications);SetTimer(window_,kWindowsNotifications,1000,nullptr);SetTimer(window_,kReminders,30000,nullptr);PostMessageW(window_,kCalendar,0,0);}
  if(verification){wchar_t executable[2048]{};GetModuleFileNameW(nullptr,executable,2048);apps_.add(executable);}
  SetTimer(window_, kFullscreen, 1000, nullptr);
  return true;
}

void IslandWindow::select_task_list(){auto g=google_calendar_.snapshot();HMENU menu=CreatePopupMenu();for(size_t i=0;i<g.task_lists.size();++i)AppendMenuW(menu,MF_STRING|(g.task_lists[i].id==g.task_list_id?MF_CHECKED:0),static_cast<UINT_PTR>(i+1),g.task_lists[i].name.c_str());POINT p{};GetCursorPos(&p);menu_active_=true;SetForegroundWindow(window_);UINT choice=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,p.x,p.y,0,window_,nullptr);DestroyMenu(menu);menu_active_=false;if(choice&&choice<=g.task_lists.size()){google_calendar_.select_task_list(g.task_lists[choice-1].id);view_.scroll=0;}schedule_collapse();}
int IslandWindow::run() {
  MSG message{};
  for(;;){
    HANDLE frame=animation_.active()?renderer_.frame_ready():nullptr;
    DWORD ready=MsgWaitForMultipleObjectsEx(frame?1:0,frame?&frame:nullptr,frame?100:INFINITE,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
    if(frame&&(ready==WAIT_OBJECT_0||ready==WAIT_TIMEOUT))animate(ready==WAIT_OBJECT_0);
    while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){
      if(message.message==WM_QUIT)return static_cast<int>(message.wParam);
      if(!settings_.translate(message)&&!reminder_window_.translate(message)&&!task_entry_.translate(message)){TranslateMessage(&message);DispatchMessageW(&message);}
    }
  }
}
int IslandWindow::run_for(unsigned milliseconds){SetTimer(window_,kAutoExit,milliseconds,nullptr);return run();}

int IslandWindow::export_previews(const std::filesystem::path& directory) {
  std::error_code error;std::filesystem::create_directories(directory,error);if(error)return 2;
  KillTimer(window_,kAnimate);KillTimer(window_,kFullscreen);KillTimer(window_,kCollapseDelay);
  RemoveClipboardFormatListener(window_);
  config_=Config{};config_.hover_expand=false;config_.auto_hide_fullscreen=false;
  width_.reset(static_cast<float>(dip_to_px(228,dpi_)));height_.reset(static_cast<float>(dip_to_px(37,dpi_)));position();
  std::ofstream report(directory/L"verification.txt");
  report<<"INFO DXGI frame-latency wait handle: "<<(renderer_.frame_ready()?"available":"timer fallback")<<"\n";
  RECT collapsed{};GetWindowRect(window_,&collapsed);
  if(GetParent(window_)!=nullptr||GetWindow(window_,GW_OWNER)!=nullptr)return 10;
  auto pump=[&](int milliseconds){
    auto end=std::chrono::steady_clock::now()+std::chrono::milliseconds(milliseconds);
    while(std::chrono::steady_clock::now()<end){HANDLE frame=animation_.active()?renderer_.frame_ready():nullptr;DWORD ready=MsgWaitForMultipleObjectsEx(frame?1:0,frame?&frame:nullptr,10,QS_ALLINPUT,MWMO_INPUTAVAILABLE);if(frame&&ready==WAIT_OBJECT_0)animate(true);MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}}
  };
  const unsigned initial_allocations=renderer_.allocations();
  set_expanded(true,true);pump(500);RECT expanded{};GetWindowRect(window_,&expanded);
  if(std::abs((expanded.left+expanded.right)-(collapsed.left+collapsed.right))>1||expanded.top!=collapsed.top)return 11;
  if(expanded.right-expanded.left!=dip_to_px(500,dpi_)+2*dip_to_px(shadow_padding,dpi_))return 12;
  set_expanded(false);pump(500);RECT restored{};GetWindowRect(window_,&restored);
  if(!EqualRect(&collapsed,&restored))return 13;
  for(int style=0;style<4;++style){config_.animation_style=style;set_expanded(true,true);pump(420);set_expanded(false);pump(420);RECT final{};GetWindowRect(window_,&final);if(!EqualRect(&collapsed,&final))return 21;}
  config_.animation_style=0;set_expanded(true);pump(70);set_expanded(false);pump(420);RECT interrupted{};GetWindowRect(window_,&interrupted);if(!EqualRect(&collapsed,&interrupted))return 22;
  if(renderer_.allocations()!=initial_allocations)return 23;
  report<<"PASS four animation styles and interrupted reversal\nPASS zero back-buffer reallocations across animated cycles\n";
  report<<"PASS independent HWND (no parent/owner)\nPASS center invariant during expand/collapse\nPASS downward expansion, fixed top edge\nPASS collapse restores initial rectangle\n";
  auto above_island=[&](HWND candidate){for(HWND hwnd=GetTopWindow(nullptr);hwnd;hwnd=GetWindow(hwnd,GW_HWNDNEXT)){if(hwnd==candidate)return true;if(hwnd==window_)return false;}return false;};
  auto owned_window=[&](const wchar_t* cls){for(HWND hwnd=GetTopWindow(nullptr);hwnd;hwnd=GetWindow(hwnd,GW_HWNDNEXT)){wchar_t name[80]{};GetClassNameW(hwnd,name,80);if(wcscmp(name,cls)==0&&GetWindow(hwnd,GW_OWNER)==window_)return hwnd;}return static_cast<HWND>(nullptr);};
  int settings_changes=0;
  settings_.open(instance_,window_,config_,[&]{++settings_changes;},&google_calendar_);
  HWND settings_hwnd=owned_window(L"NativeIslandSettings");if(!settings_hwnd)return 29;
  Config settings_before=config_;
  for(int page=0;page<6;++page){int before=settings_changes;SendMessageW(settings_hwnd,WM_COMMAND,MAKEWPARAM(280+page,BN_CLICKED),0);pump(40);if(settings_changes!=before)return 40;
    if(!settings_.capture_preview(directory/(L"settings-"+std::to_wstring(page)+L".png")))return 40;
    if(IsWindowVisible(GetDlgItem(settings_hwnd,273))!=(page==3)||IsWindowVisible(GetDlgItem(settings_hwnd,200))!=(page==0))return 40;
  }
  if(!GetDlgItem(settings_hwnd,273)||!GetDlgItem(settings_hwnd,277)||IsWindowEnabled(GetDlgItem(settings_hwnd,274)))return 40;
  for(int id:{256,250,262,271,300}){
    SendMessageW(settings_hwnd,WM_COMMAND,MAKEWPARAM(id==300?280:id==262?282:281,BN_CLICKED),0);
    HWND combo=GetDlgItem(settings_hwnd,id);if(!combo)return 29;
    const int changes_before=settings_changes;
    SendMessageW(combo,CB_SHOWDROPDOWN,TRUE,0);pump(50);position();check_fullscreen();
    if(!SendMessageW(combo,CB_GETDROPPEDSTATE,0,0)||settings_changes!=changes_before)return 30;
    COMBOBOXINFO info{sizeof(info)};if(!GetComboBoxInfo(combo,&info)||!IsWindowVisible(info.hwndList)||!above_island(info.hwndList))return 31;
    SendMessageW(combo,CB_SHOWDROPDOWN,FALSE,0);
    if(settings_changes!=changes_before)return 32;
    int count=static_cast<int>(SendMessageW(combo,CB_GETCOUNT,0,0));if(count<1)return 33;
    for(int selection=0;selection<count;++selection){
      int before=settings_changes;SendMessageW(combo,CB_SETCURSEL,selection,0);
      SendMessageW(settings_hwnd,WM_COMMAND,MAKEWPARAM(id,CBN_SELCHANGE),reinterpret_cast<LPARAM>(combo));
      int expected=id==262?std::array{0,5,15,30,60,120,1440}[selection]:selection;
      int actual=id==300?config_.island_shape:id==256?config_.animation_style:id==250?config_.monitor_index:id==271?config_.reminder_morning_hour:config_.clipboard_ttl_minutes;
      if(actual!=expected||settings_changes!=before+1||SendMessageW(combo,CB_GETCURSEL,0,0)!=selection)return 34;
    }
  }
  config_=settings_before;
  report<<"PASS settings combos stay open without changes, list above island, every available selection updates config once\n";
  search_.open(instance_,window_,L"",[](std::wstring){});
  ClipboardEntry text_preview;text_preview.text=L"Owned preview regression test";
  show_clipboard_preview(instance_,window_,text_preview);
  pump(50);position();check_fullscreen();set_expanded(true);pump(420);
  for(auto cls:{L"NativeIslandSettings",L"NativeIslandPanoSearch",L"NativeIslandTextPreview"}){
    HWND hwnd=owned_window(cls);if(!hwnd||!above_island(hwnd))return 26;
  }
  DestroyWindow(owned_window(L"NativeIslandTextPreview"));search_.close();settings_.close();
  ReminderStore reminder_fixture(directory/(L"ui-reminders-"+std::to_wstring(GetTickCount64())+L".json"));
  const auto tomorrow=reminder_day_at_offset(reminder_now(),1);
  reminder_window_.open(instance_,window_,reminder_fixture,[]{},tomorrow);
  HWND reminder_dialog=owned_window(L"NativeIslandReminders");if(!reminder_dialog||!above_island(reminder_dialog))return 35;
  if(GetDlgItem(reminder_dialog,310)||GetDlgItem(reminder_dialog,300))return 36;
  SendMessageW(reminder_dialog,WM_COMMAND,MAKEWPARAM(305,BN_CLICKED),reinterpret_cast<LPARAM>(GetDlgItem(reminder_dialog,305)));
  if(!reminder_window_.visible()||!reminder_fixture.snapshot().empty())return 36;
  SetDlgItemTextW(reminder_dialog,301,L"UI reminder regression");
  SendMessageW(reminder_dialog,WM_COMMAND,MAKEWPARAM(305,BN_CLICKED),reinterpret_cast<LPARAM>(GetDlgItem(reminder_dialog,305)));
  if(reminder_fixture.snapshot().size()!=1||reminder_fixture.snapshot()[0].title!=L"UI reminder regression")return 37;
  if(reminder_day(reminder_fixture.snapshot()[0].start)!=reminder_day(tomorrow))return 55;
  if(reminder_window_.visible())return 38;
  if(!reminder_fixture.remove(reminder_fixture.snapshot()[0].id))return 39;
  reminder_window_.close();
  report<<"PASS compact owned reminder entry without Google/list controls; successful add closes entry\n";
  // Exercise a real native popup's nested message loop without selecting any command.
  static thread_local IslandWindow* menu_test{};static thread_local bool menu_above{};
  menu_test=this;menu_above=false;config_.auto_hide_fullscreen=true;
  SetTimer(window_,90,1200,[](HWND hwnd,UINT,UINT_PTR id,DWORD){
    KillTimer(hwnd,id);menu_test->position();menu_test->check_fullscreen();
    for(HWND item=GetTopWindow(nullptr);item;item=GetWindow(item,GW_HWNDNEXT)){
      if(item==hwnd)break;wchar_t cls[80]{};GetClassNameW(item,cls,80);
      DWORD pid{};GetWindowThreadProcessId(item,&pid);
      if(pid==GetCurrentProcessId()&&wcscmp(cls,L"#32768")==0){menu_above=true;break;}
    }
    EndMenu();
  });
  HMENU test_menu=CreatePopupMenu();AppendMenuW(test_menu,MF_STRING,1,L"Menu stacking regression test");
  POINT menu_point{anchor_center_x_,anchor_top_+20};SetForegroundWindow(window_);
  TrackPopupMenu(test_menu,TPM_RETURNCMD,menu_point.x,menu_point.y,0,window_,nullptr);
  KillTimer(window_,90);DestroyMenu(test_menu);menu_test=nullptr;config_.auto_hide_fullscreen=false;
  if(!menu_above||menu_active_||!(GetWindowLongPtrW(window_,GWL_EXSTYLE)&WS_EX_TOPMOST))return 27;
  set_expanded(false);pump(420);
  report<<"PASS settings/search/text preview remain above owner across animation and fullscreen polling\nPASS native popup remains above island during nested menu loop\nPASS island remains topmost after menu closes\n";
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
  while(!monitor_.snapshot().ready && std::chrono::steady_clock::now()<deadline) {
    MsgWaitForMultipleObjectsEx(0,nullptr,100,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
    MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
  }
  SystemSnapshot system=monitor_.snapshot();MediaSnapshot media=media_.snapshot();
  ViewModel view;view.files=files_;view.clipboard=clipboard_;view.applications=apps_.snapshot();
  view.audio=audio_.snapshot();
  Reminder reminder;reminder.id=L"fixture:1";reminder.title=L"Proje toplantısı";reminder.start=reminder_now();reminder.end=reminder.start+3600;
  view.reminders={reminder};reminder.id=L"fixture:2";reminder.title=L"Fatura ödeme";reminder.all_day=true;view.reminders.push_back(reminder);
  reminder.id=L"fixture:3";reminder.title=L"Google Takvim · Tasarım görüşmesi";reminder.all_day=false;reminder.google=true;view.reminders.push_back(reminder);view.calendar_status=L"Google Takvim bağlı değil · + ile yerel hatırlatıcı ekle";
  // Isolated visual fixtures, never added to the user's history or shortcuts.
  wchar_t system_dir[MAX_PATH]{};GetSystemDirectoryW(system_dir,MAX_PATH);
  for(auto name:{L"notepad.exe",L"cmd.exe",L"calc.exe"}){auto path=std::filesystem::path(system_dir)/name;if(std::filesystem::exists(path))view.applications.push_back(shell_icon(path.wstring()));}
  ClipboardEntry clip;clip.id=1;clip.pinned=true;clip.text=L"Uzun metin önizlemesi: satıra tıklayınca tam metin açılır.\r\nİkinci satır.";view.clipboard.push_back(clip);
  ClipboardEntry image;image.id=2;BITMAPINFOHEADER header{};header.biSize=40;header.biWidth=32;header.biHeight=32;header.biPlanes=1;header.biBitCount=24;header.biSizeImage=32*32*3;
  image.dib.resize(40+header.biSizeImage);memcpy(image.dib.data(),&header,40);for(size_t i=40;i<image.dib.size();i+=3){image.dib[i]=220;image.dib[i+1]=static_cast<unsigned char>((i/3)%255);image.dib[i+2]=80;}
  if(!decode_clip_image(image))return 14;
  show_clipboard_preview(instance_,window_,image);position();check_fullscreen();
  HWND image_preview=owned_window(L"NativeIslandImagePreview");if(!image_preview||!above_island(image_preview))return 28;
  DestroyWindow(image_preview);report<<"PASS image preview remains above island\n";
  view.clipboard.push_back(std::move(image));view.clipboard_enabled=true;
  const auto fixture=std::filesystem::current_path()/L"tests"/L"fixtures"/L"ShelfFolder";
  if(std::filesystem::exists(fixture))view.files.push_back(fixture.wstring());
  for(int theme=0;theme<6;++theme) {
    view.theme=theme_for(theme);view.expansion=0;
    const UINT cw=static_cast<UINT>(dip_to_px(228+2*shadow_padding,dpi_)),ch=static_cast<UINT>(dip_to_px(37+2*shadow_padding,dpi_));
    if(FAILED(renderer_.resize(cw,ch,static_cast<float>(dpi_))))return 3;
    renderer_.capture_next((directory/std::format(L"theme-{}-collapsed.png",theme)).wstring());
    if(FAILED(renderer_.render(system,media,view)))return 4;
    view.expansion=1;
    for(int page=0;page<8;++page) {
      view.page=static_cast<Page>(page);
      view.height=static_cast<float>(page_height(view.page,view.page==Page::reminders?view.reminders.size():view.page==Page::applications?view.applications.size():view.files.size(),view.clipboard.size(),true,!system.bluetooth_audio.empty()));
      view.wave_mix=media.playing?1.f:0.f;
      if(FAILED(renderer_.resize(dip_to_px(500+2*shadow_padding,dpi_),dip_to_px(static_cast<int>(view.height)+2*shadow_padding,dpi_),static_cast<float>(dpi_))))return 5;
      renderer_.capture_next((directory/std::format(L"theme-{}-page-{}.png",theme,page)).wstring());
      if(FAILED(renderer_.render(system,media,view)))return 6;
      if(renderer_.hit_targets().size()<5)return 7;
    }
  }
  report<<"PASS native render: six themes / eight compact pages\n";
  view.theme=theme_for(0);view.expansion=1;view.island_shape=1;view.page=Page::media;view.height=static_cast<float>(page_height(Page::media,0,0,false,!system.bluetooth_audio.empty()));
  MediaSnapshot music;music.available=true;music.title=L"Sen Bilmezsin";music.artist=L"Dedublüman";music.source=L"Spotify";music.playing=true;music.can_play=music.can_previous=music.can_next=music.can_seek=true;music.duration=240;music.position=94;
  renderer_.resize(dip_to_px(532,dpi_),dip_to_px(static_cast<int>(view.height)+32,dpi_),static_cast<float>(dpi_));renderer_.capture_next((directory/L"droplet-media.png").wstring());if(FAILED(renderer_.render(system,music,view)))return 75;
  if(std::any_of(renderer_.hit_targets().begin(),renderer_.hit_targets().end(),[](const auto& h){return h.action==Action::volume||h.action==Action::mute_output;}))return 76;
  view.page=Page::sound;view.audio.available=true;view.audio.output_name=L"Kulaklıklar (AirPods Pro)";view.audio.volume=.65f;view.audio.sessions={{L"fixture:spotify",L"Spotify",.85f,false},{L"fixture:chrome",L"Chrome",.4f,true},{L"fixture:system",L"Sistem sesleri",.5f,false}};view.height=static_cast<float>(page_height(Page::sound,3,0,false,false));
  view.audio.sessions[0].app_key=L"fixture:spotify";view.audio.sessions[0].favorite=true;view.audio.sessions[1].app_key=L"fixture:chrome";view.audio.sessions[2].app_key=L"fixture:system";
  renderer_.resize(dip_to_px(532,dpi_),dip_to_px(static_cast<int>(view.height)+32,dpi_),static_cast<float>(dpi_));renderer_.capture_next((directory/L"sound-mixer.png").wstring());if(FAILED(renderer_.render(system,music,view)))return 77;
  if(std::count_if(renderer_.hit_targets().begin(),renderer_.hit_targets().end(),[](const auto& h){return h.action==Action::session_volume&&!h.key.empty();})!=3)return 78;
  if(std::count_if(renderer_.hit_targets().begin(),renderer_.hit_targets().end(),[](const auto& h){return h.action==Action::favorite_audio&&!h.key.empty();})!=3)return 99;
  if(owned_window(L"NativeIslandSatellite"))return 100;
  view.page=Page::tasks;view.google.tasks_enabled=true;view.google.task_list_id=L"fixture-list";view.google.task_lists={{L"fixture-list",L"İşlerim"}};view.google.tasks_message=L"Google Tasks güncel";view.google.tasks={{L"fixture1",L"Tasarım toplantısına hazırlan",L"",L"2026-10-14",false},{L"fixture2",L"Raporu gönder",L"Son kontrolü yap",L"",false},{L"fixture3",L"E-postaları yanıtla",L"",L"",true}};view.height=static_cast<float>(page_height(Page::tasks,3,0,false,false));
  renderer_.resize(dip_to_px(532,dpi_),dip_to_px(static_cast<int>(view.height)+32,dpi_),static_cast<float>(dpi_));renderer_.capture_next((directory/L"tasks-island.png").wstring());if(FAILED(renderer_.render(system,music,view)))return 79;
  if(std::count_if(renderer_.hit_targets().begin(),renderer_.hit_targets().end(),[](const auto& h){return h.action==Action::complete_task;})!=2)return 80;
  if(std::count_if(renderer_.hit_targets().begin(),renderer_.hit_targets().end(),[](const auto& h){return h.action==Action::delete_task&&!h.key.empty();})!=2||std::any_of(renderer_.hit_targets().begin(),renderer_.hit_targets().end(),[](const auto& h){return h.key==L"fixture3";}))return 101;
  if(std::count_if(renderer_.hit_targets().begin(),renderer_.hit_targets().end(),[](const auto& h){return h.action==Action::edit_task&&!h.key.empty();})!=2)return 108;
  for(const auto& edit_target:renderer_.hit_targets())if(edit_target.action==Action::edit_task){auto remove=std::find_if(renderer_.hit_targets().begin(),renderer_.hit_targets().end(),[&](const auto& h){return h.action==Action::delete_task&&h.key==edit_target.key;});if(remove==renderer_.hit_targets().end()||edit_target.rect.right>remove->rect.left)return 109;}
  view.island_shape=0;
  const auto saved_page=view_.page;RECT role_before{};GetWindowRect(window_,&role_before);activate({{},Action::tab,7},0);if(view_.page!=Page::tasks)return 81;RECT role_after{};GetWindowRect(window_,&role_after);if(role_before.left+role_before.right!=role_after.left+role_after.right||role_before.top!=role_after.top)return 82;activate({{},Action::tab,2},0);if(view_.page!=Page::media)return 83;view_.page=saved_page;
  view.google.connected=true;
  for(int width:{465,500,650}){view.content_width=static_cast<float>(width);renderer_.resize(dip_to_px(width+32,dpi_),dip_to_px(static_cast<int>(view.height)+32,dpi_),static_cast<float>(dpi_));renderer_.capture_next((directory/std::format(L"tasks-navigation-{}.png",width)).wstring());if(FAILED(renderer_.render(system,music,view)))return 87;const auto& hits=renderer_.hit_targets();if(hits.size()<9||hits[5].index!=5||hits[6].index!=7||hits[7].index!=6||hits[8].action!=Action::settings)return 84;for(size_t i=1;i<9;++i)if(hits[i-1].rect.right>hits[i].rect.left)return 85;}
  view.content_width=500;renderer_.resize(dip_to_px(532,dpi_),dip_to_px(static_cast<int>(view.height)+32,dpi_),static_cast<float>(dpi_));
  std::wstring entered;int submissions=0;auto add_fixture=[&](std::wstring text){++submissions;entered=std::move(text);};task_entry_.open(instance_,window_,add_fixture);auto entry=owned_window(L"NativeIslandTaskEntry");if(!entry||!above_island(entry))return 88;
  auto edit=GetDlgItem(entry,TaskEntryWindow::title_id),add=GetDlgItem(entry,TaskEntryWindow::add_id);SetDlgItemTextW(entry,TaskEntryWindow::title_id,L"  ");SendMessageW(add,BM_CLICK,0,0);if(submissions||!task_entry_.visible())return 102;
  for(auto title:{L"Y",L"Ye",L"Yeni",L"  Yeni görev  "})SetDlgItemTextW(entry,TaskEntryWindow::title_id,title);
  SendMessageW(entry,WM_COMMAND,MAKEWPARAM(TaskEntryWindow::title_id,EN_CHANGE),reinterpret_cast<LPARAM>(edit));SendMessageW(entry,WM_COMMAND,MAKEWPARAM(1,EN_CHANGE),reinterpret_cast<LPARAM>(edit));SendMessageW(entry,WM_COMMAND,IDOK,0);
  MSG enter{};enter.hwnd=edit;enter.message=WM_KEYDOWN;enter.wParam=VK_RETURN;if(!task_entry_.translate(enter)||submissions||!task_entry_.visible())return 103;
  SendMessageW(entry,WM_COMMAND,MAKEWPARAM(TaskEntryWindow::add_id,BN_SETFOCUS),reinterpret_cast<LPARAM>(add));if(submissions||!task_entry_.visible())return 104;
  SendMessageW(add,BM_CLICK,0,0);if(entered!=L"Yeni görev"||task_entry_.visible()||submissions!=1)return 89;
  task_entry_.open(instance_,window_,add_fixture);entry=owned_window(L"NativeIslandTaskEntry");SetDlgItemTextW(entry,TaskEntryWindow::title_id,L"İptal edilen görev");task_entry_.close();if(submissions!=1)return 105;
  int edit_attempts=0;bool accept_edit=false;std::wstring edited_title;
  task_entry_.open_edit(instance_,window_,L"edit-fixture",L"Eski başlık",[&](std::wstring title){++edit_attempts;edited_title=std::move(title);auto dialog=owned_window(L"NativeIslandTaskEntry");SendMessageW(dialog,WM_COMMAND,MAKEWPARAM(TaskEntryWindow::add_id,BN_CLICKED),reinterpret_cast<LPARAM>(GetDlgItem(dialog,TaskEntryWindow::add_id)));return accept_edit;});
  entry=owned_window(L"NativeIslandTaskEntry");wchar_t edit_text[1025]{};GetDlgItemTextW(entry,TaskEntryWindow::title_id,edit_text,1025);if(std::wstring(edit_text)!=L"Eski başlık"||task_entry_.task_key()!=L"edit-fixture")return 110;
  SetDlgItemTextW(entry,TaskEntryWindow::title_id,L"  Yeni başlık  ");task_entry_.open_edit(instance_,window_,L"edit-fixture",L"Eski başlık",[](std::wstring){return true;});task_entry_.open_edit(instance_,window_,L"other-fixture",L"Başka başlık",[](std::wstring){return true;});GetDlgItemTextW(entry,TaskEntryWindow::title_id,edit_text,1025);if(entry!=owned_window(L"NativeIslandTaskEntry")||task_entry_.task_key()!=L"edit-fixture"||std::wstring(edit_text)!=L"  Yeni başlık  "||edit_attempts)return 111;
  SendMessageW(GetDlgItem(entry,TaskEntryWindow::add_id),BM_CLICK,0,0);if(edit_attempts!=1||!task_entry_.visible()||edited_title!=L"Yeni başlık")return 112;
  accept_edit=true;SendMessageW(GetDlgItem(entry,TaskEntryWindow::add_id),BM_CLICK,0,0);if(edit_attempts!=2||task_entry_.visible())return 113;
  task_entry_.open_edit(instance_,window_,L"unchanged-fixture",L"Aynı başlık",[&](std::wstring){++edit_attempts;return true;});entry=owned_window(L"NativeIslandTaskEntry");SendMessageW(GetDlgItem(entry,TaskEntryWindow::add_id),BM_CLICK,0,0);if(edit_attempts!=2||task_entry_.visible())return 114;
  task_entry_.open_edit(instance_,window_,L"cancel-fixture",L"Başlık",[&](std::wstring){++edit_attempts;return true;});task_entry_.close();if(edit_attempts!=2)return 115;
  report<<"PASS task edit: prefilled title, single form, stable task identity/input, reentrant save suppressed, rejected save retained, unchanged/cancel do not submit\n";
  report<<"PASS droplet media without volume controls, session mixer, Tasks fixtures and header Tasks navigation at fixed anchor\nPASS no satellite window and no header overlap at 465/500/650 DIP\nPASS completed tasks hidden; delete targets preserve task IDs\nPASS task entry ignores typing, focus, legacy IDOK and Enter; only Ekle submits once; blank/cancel do not submit\n";
  for(int alignment:{0,50,100}){config_.horizontal_percent=alignment;position();RECT before{};GetWindowRect(window_,&before);config_.top_gap=10;position();RECT after{};GetWindowRect(window_,&after);if(before.left!=after.left)return 15;config_.top_gap=7;}
  toggle_hidden();if(IsWindowVisible(window_))return 16;check_fullscreen();if(IsWindowVisible(window_))return 17;toggle_hidden();if(!IsWindowVisible(window_))return 18;
  report<<"PASS horizontal alignment preserves x during top-gap changes\nPASS manual hide survives fullscreen polling and restores\nPASS clipboard image decoding / grid fixtures rendered\n";
  view.page=Page::reminders;view.theme=theme_for(1);
  {
    ViewModel summary_view=view;summary_view.expansion=0;summary_view.notice.clear();
    Reminder item;item.id=L"summary-fixture";item.title=L"Test";item.start=reminder_now()-3600;item.end=reminder_now()+3600;
    for(int scenario=0;scenario<3;++scenario){
      item.done=scenario==1;if(scenario==2)item.end=reminder_now()-1;
      summary_view.reminders={item};summary_view.pending_today_reminders=reminders_pending_today(summary_view.reminders,reminder_now());
      if(summary_view.pending_today_reminders.size()!=(scenario==0?1:0))return 56;
      renderer_.resize(dip_to_px(260,dpi_),dip_to_px(69,dpi_),static_cast<float>(dpi_));
      renderer_.capture_next((directory/std::format(L"reminder-summary-{}.png",scenario)).wstring());
      if(FAILED(renderer_.render(system,MediaSnapshot{},summary_view)))return 57;
    }
  }
  const auto day_now=reminder_now();
  for(int offset=-1;offset<=1;++offset){
    view.reminder_offset=offset;view.reminder_date=reminder_day_at_offset(day_now,offset);view.has_today_reminders=true;
    Reminder item;item.id=L"day-fixture";item.title=offset<0?L"Dünkü toplantı":offset>0?L"Yarınki plan":L"Bugünkü hatırlatıcı";
    item.start=view.reminder_date;item.end=item.start+3600;item.done=offset<0;view.reminders={item};
    view.height=static_cast<float>(page_height(Page::reminders,1,0,false,false));
    if(FAILED(renderer_.resize(dip_to_px(532,dpi_),dip_to_px(static_cast<int>(view.height)+32,dpi_),static_cast<float>(dpi_))))return 50;
    renderer_.capture_next((directory/std::format(L"reminders-day-{}.png",offset+1)).wstring());
    if(FAILED(renderer_.render(system,media,view)))return 51;
    if(std::count_if(renderer_.hit_targets().begin(),renderer_.hit_targets().end(),[](const HitTarget& hit){return hit.action==Action::reminder_day;})!=3)return 52;
    view_.scroll=3;activate({{},Action::reminder_day,static_cast<size_t>(offset+1)},0);
    if(view_.reminder_offset!=offset||view_.scroll!=0)return 53;
  }
  view_.reminder_offset=0;view_.page_opacity=1;KillTimer(window_,kPageFade);
  view.reminder_offset=0;view.reminder_date=day_now;view.reminders.clear();
  view.height=static_cast<float>(page_height(Page::reminders,0,0,false,false));
  renderer_.resize(dip_to_px(532,dpi_),dip_to_px(static_cast<int>(view.height)+32,dpi_),static_cast<float>(dpi_));
  renderer_.capture_next((directory/L"reminders-days-empty.png").wstring());if(FAILED(renderer_.render(system,media,view)))return 54;
  report<<"PASS reminder day navigation and selected-date entry\n";
  view.page=Page::clipboard;view.clipboard_query=L"uzun";view.height=238;renderer_.resize(dip_to_px(532,dpi_),dip_to_px(270,dpi_),static_cast<float>(dpi_));renderer_.capture_next((directory/L"clipboard-search.png").wstring());if(FAILED(renderer_.render(system,media,view)))return 24;
  view.expansion=0;view.notice=L"Vivaldi · Yeni yorum";view.notice_app=L"Vivaldi";view.notice_title=L"Yeni yorum";view.notice_body=L"Tasarım dokümanındaki değişiklikleri incele.";view.notice_kind=8;view.notice_age=1;
  renderer_.resize(dip_to_px(520,dpi_),dip_to_px(136,dpi_),static_cast<float>(dpi_));renderer_.capture_next((directory/L"notice-windows.png").wstring());if(FAILED(renderer_.render(system,media,view)))return 58;
  auto sample_image=[](unsigned b,unsigned g,unsigned r){auto pixels=std::make_shared<std::vector<std::uint8_t>>(12+64*48*4);(*pixels)[0]='N';(*pixels)[1]='I';(*pixels)[2]='R';(*pixels)[3]='G';(*pixels)[4]=64;(*pixels)[8]=48;for(size_t i=12;i<pixels->size();i+=4){(*pixels)[i]=static_cast<std::uint8_t>(b);(*pixels)[i+1]=static_cast<std::uint8_t>(g);(*pixels)[i+2]=static_cast<std::uint8_t>(r);(*pixels)[i+3]=255;}return pixels;};
  for(int scenario=0;scenario<6;++scenario){view.island_shape=scenario==5?1:0;view.notice_logo=scenario==0?nullptr:sample_image(32,48,220);view.notice_portrait=scenario==2?sample_image(190,155,70):scenario==3?std::make_shared<const std::vector<std::uint8_t>>(3,std::uint8_t{}):nullptr;view.notice_title=L"Örnek bildirim";view.notice_body=scenario>=4?L"Bu uzun bir test mesajıdır. Uygulama adı, başlık ve mesaj aynı sütundan başlar; satırlar düzenli aralıklarla devam eder. Uzun içerik güvenli kenar boşlukları içinde önizlenir.":L"Test verisi · kişi fotoğrafı ve uygulama ikonu ayrı alanlardır.";auto size=compact_notice_size(view.notice_app,view.notice_title,view.notice_body,scenario==2);renderer_.resize(dip_to_px(size.width+32,dpi_),dip_to_px(size.height+32,dpi_),static_cast<float>(dpi_));renderer_.capture_next((directory/std::format(L"notice-identity-{}.png",scenario)).wstring());if(FAILED(renderer_.render(system,media,view)))return 106;}
  view.notice_portrait.reset();view.notice_logo.reset();view.island_shape=0;
  report<<"PASS notification image fixtures: no portrait, app icon only, circular independent portrait, malformed portrait hidden\n";
  const bool motion_before=config_.reduced_motion;config_.reduced_motion=true;
  view_.notice=L"Windows notification regression";view_.notice_app=L"Vivaldi";view_.notice_title=L"Yeni yorum";
  view_.notice_body=L"Test";view_.notice_kind=8;view_.notice_portrait=sample_image(190,155,70);set_expanded(false);
  const auto expected_notice=compact_notice_size(view_.notice_app,view_.notice_title,view_.notice_body,true);
  if(static_cast<int>(width_.value())!=dip_to_px(expected_notice.width,dpi_)||
     static_cast<int>(height_.value())!=dip_to_px(expected_notice.height,dpi_))return 59;
  notice_deadline_=std::chrono::steady_clock::now()-std::chrono::seconds(1);handle(WM_TIMER,kNotice,0);
  if(view_.notice_portrait||view_.notice_logo)return 107;
  if(!view_.notice.empty()||static_cast<int>(width_.value())!=dip_to_px(config_.collapsed_width,dpi_)||static_cast<int>(height_.value())!=dip_to_px(config_.collapsed_height,dpi_))return 59;
  config_.reduced_motion=motion_before;
  view.expansion=0;view.notice=L"Mikrofon sessize alındı";view.notice_kind=1;view.notice_age=.1f;renderer_.resize(dip_to_px(260,dpi_),dip_to_px(69,dpi_),static_cast<float>(dpi_));renderer_.capture_next((directory/L"notice-microphone.png").wstring());if(FAILED(renderer_.render(system,media,view)))return 25;
  report<<"PASS clipboard search/pin and mute notification visual fixtures\nINFO audio outputs: "<<view.audio.outputs.size()<<"\nINFO output switching interface: "<<view.audio.output_switch_supported<<"\n";
  config_.reduced_motion=true;config_.pin_on_click=false;set_expanded(true,true);if(state_==State::pinned)return 19;
  config_.pin_on_click=true;set_expanded(true,true);if(state_!=State::pinned)return 20;
  report<<"PASS click-to-pin setting gates pinned state\n";
  DestroyWindow(window_);return 0;
}

LRESULT CALLBACK IslandWindow::window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  IslandWindow* self = reinterpret_cast<IslandWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (message == WM_NCCREATE) {
    self = static_cast<IslandWindow*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
    self->window_ = hwnd; SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  }
  return self ? self->handle(message, wparam, lparam) : DefWindowProcW(hwnd, message, wparam, lparam);
}

LRESULT IslandWindow::handle(UINT message, WPARAM wparam, LPARAM lparam) {
  switch (message) {
  case WM_ENTERMENULOOP:menu_active_=true;KillTimer(window_,kCollapseDelay);return 0;
  case WM_EXITMENULOOP:menu_active_=false;schedule_collapse();return 0;
  case WM_HOTKEY:if(wparam==1)toggle_hidden();return 0;
  case WM_ERASEBKGND: return 1;
  case WM_PAINT: { PAINTSTRUCT ps{}; BeginPaint(window_, &ps); EndPaint(window_, &ps); render(); return 0; }
  case kTelemetry:smart_events();resize_for_content();update_media_animation();render(); return 0;
  case kCalendar:reminder_events();resize_for_content();render();return 0;
  case WM_TIMER:
    if(wparam==kWindowsNotifications){windows_notification_events();return 0;}
    if(wparam==kReminders){reminder_events();render();return 0;}
    if(wparam==kReminderDelivery){KillTimer(window_,kReminderDelivery);show_reminder_notice();return 0;}
    if(wparam==kNotice){auto now=std::chrono::steady_clock::now();float remaining=std::chrono::duration<float>(notice_deadline_-now).count();view_.notice_age=remaining<.25f?2.6f-remaining:std::min(2.35f,std::chrono::duration<float>(now-notice_start_).count());if(remaining<=0){if(view_.notice_kind==8)clear_windows_notice();else{view_.notice.clear();KillTimer(window_,kNotice);}resize_for_content();if(reminder_summary_open_){reminder_summary_open_=false;schedule_collapse();}}render();return 0;}
    if(wparam==kAutoExit){DestroyWindow(window_);return 0;}
    if(wparam==kWaves) {
      auto now=std::chrono::steady_clock::now();float dt=std::chrono::duration<float>(now-wave_frame_).count();wave_frame_=now;
      view_.wave_phase=std::chrono::duration<float>(now-wave_start_).count();
      view_.wave_mix+=(wave_target_-view_.wave_mix)*std::min(1.f,dt*14.f);
      if(wave_target_==0&&view_.wave_mix<.01f){view_.wave_mix=0;waves_running_=false;KillTimer(window_,kWaves);}
      render();return 0;
    }
    if (wparam == kAnimate) animate();
    else if (wparam == kFullscreen){check_fullscreen();smart_events();if(expire_clipboard(clipboard_,GetTickCount64(),config_.clipboard_ttl_minutes,config_.expire_pinned_clips)){++clipboard_revision_;view_.scroll=0;resize_for_content();render();}}
    else if (wparam == kPageFade) {
      view_.page_opacity=std::clamp(std::chrono::duration<float>(std::chrono::steady_clock::now()-page_transition_).count()/.2f,0.f,1.f);
      render();if(view_.page_opacity>=1)KillTimer(window_,kPageFade);
    }
    else if (wparam == kCollapseDelay) {
      KillTimer(window_, kCollapseDelay);
      if (!pointer_inside() && !outgoing_drag_ && !menu_active_ && !reminder_summary_open_ && !view_.drop_active && state_ != State::pinned) set_expanded(false);
    }
    return 0;
  case WM_DPICHANGED: {
    animation_.cancel();KillTimer(window_,kAnimate);
    dpi_ = HIWORD(wparam);
    const bool open = expanded_;
    width_.reset(static_cast<float>(dip_to_px(open ? config_.expanded_width : config_.collapsed_width, dpi_)));
    height_.reset(static_cast<float>(dip_to_px(open ? expanded_height() : config_.collapsed_height, dpi_)));
    if(open)width_.reset(static_cast<float>(dip_to_px(std::max(500,config_.expanded_width),dpi_)));
    position();
    RECT dpi_client{};GetClientRect(window_,&dpi_client);
    renderer_.resize(dpi_client.right,dpi_client.bottom,static_cast<float>(dpi_));render();return 0;
  }
  case WM_DISPLAYCHANGE: position(); return 0;
  case WM_SETTINGCHANGE: apply_settings(); return 0;
  case WM_NCHITTEST: {
    POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)}; ScreenToClient(window_, &point);
    return point_inside_capsule(point) ? HTCLIENT : HTTRANSPARENT;
  }
  case WM_MOUSEMOVE: {
    if(background_press_){POINT now{};GetCursorPos(&now);if(moving_||std::abs(now.x-drag_origin_.x)>=GetSystemMetrics(SM_CXDRAG)){
      moving_=true;animation_.cancel();KillTimer(window_,kAnimate);width_.reset(static_cast<float>(dip_to_px(expanded_?std::max(500,config_.expanded_width):config_.collapsed_width,dpi_)));height_.reset(static_cast<float>(dip_to_px(expanded_?expanded_height():config_.collapsed_height,dpi_)));MonitorSelect select{std::max(0,config_.monitor_index)};EnumDisplayMonitors(nullptr,nullptr,monitor_at_index,reinterpret_cast<LPARAM>(&select));MONITORINFO info{sizeof(info)};GetMonitorInfoW(select.result?select.result:MonitorFromWindow(window_,MONITOR_DEFAULTTONEAREST),&info);
      int half=dip_to_px(std::max(500,config_.expanded_width),dpi_)/2+8;int low=info.rcMonitor.left+half,high=std::max(low,static_cast<int>(info.rcMonitor.right)-half);anchor_center_x_=std::clamp(drag_center_+static_cast<int>(now.x-drag_origin_.x),low,high);
      config_.horizontal_percent=high>low?MulDiv(anchor_center_x_-low,100,high-low):50;animate();return 0;}}
    if(pressed_hit_>=0 && !outgoing_drag_ && static_cast<size_t>(pressed_hit_)<renderer_.hit_targets().size()) {
      auto target=pressed_target_.value_or(renderer_.hit_targets()[pressed_hit_]);POINT now{};GetCursorPos(&now);
      if(target.action==Action::volume){float x=GET_X_LPARAM(lparam)*96.f/dpi_-shadow_padding;audio_.volume(std::clamp((x-target.rect.left)/(target.rect.right-target.rect.left),0.f,1.f));return 0;}
      if(target.action==Action::session_volume){float x=GET_X_LPARAM(lparam)*96.f/dpi_-shadow_padding;audio_.session_volume(target.key,std::clamp((x-target.rect.left)/(target.rect.right-target.rect.left),0.f,1.f));return 0;}
      if(target.action==Action::drag_file&&target.index<files_.size()&&
         (std::abs(now.x-drag_origin_.x)>=GetSystemMetrics(SM_CXDRAG)||std::abs(now.y-drag_origin_.y)>=GetSystemMetrics(SM_CYDRAG))) {
        const auto path=files_[target.index];pressed_hit_=-1;ReleaseCapture();outgoing_drag_=true;KillTimer(window_,kCollapseDelay);
        drag_file_copy(window_,path);outgoing_drag_=false;schedule_collapse();render();return 0;
      }
    }
    pointer_over_ = true; KillTimer(window_, kCollapseDelay);
    POINT p{GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)};int hover=hit_at(p);
    if(hover!=view_.hovered){view_.hovered=hover;render();}
    if (!outgoing_drag_ && config_.hover_expand && (state_ == State::collapsed || state_ == State::collapsing)) set_expanded(true);
    TRACKMOUSEEVENT t{sizeof(t), TME_LEAVE, window_, 0}; TrackMouseEvent(&t); return 0;
  }
  case WM_MOUSELEAVE:
    pointer_over_ = false; view_.hovered=-1;render();schedule_collapse(); return 0;
  case WM_MOUSEWHEEL: {
    size_t count=view_.page==Page::sound?view_.audio.sessions.size():view_.page==Page::tasks?view_.google.tasks.size():view_.page==Page::files?files_.size():view_.page==Page::clipboard?clipboard_matches(clipboard_,view_.clipboard_query).size():view_.page==Page::applications?apps_.snapshot().size():view_.page==Page::reminders?view_.reminders.size():0;
    size_t step=view_.page==Page::applications?4:1;
    if(GET_WHEEL_DELTA_WPARAM(wparam)<0 && view_.scroll+step<count)view_.scroll+=step;
    else if(GET_WHEEL_DELTA_WPARAM(wparam)>0)view_.scroll=view_.scroll>=step?view_.scroll-step:0;
    render();return 0;
  }
  case WM_LBUTTONDOWN:
    GetCursorPos(&drag_origin_);
    drag_center_=anchor_center_x_;moving_=false;
    pressed_hit_=hit_at(POINT{GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)});
    pressed_target_.reset();if(pressed_hit_>=0){pressed_target_=renderer_.hit_targets()[pressed_hit_];KillTimer(window_,kCollapseDelay);SetCapture(window_);return 0;}
    background_press_=true;KillTimer(window_,kCollapseDelay);SetCapture(window_);return 0;
  case WM_LBUTTONUP:
    if(pressed_hit_>=0) {
      const int hit=hit_at(POINT{GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)});const int pressed=pressed_hit_;pressed_hit_=-1;ReleaseCapture();
      if(hit==pressed && static_cast<size_t>(hit)<renderer_.hit_targets().size()) {
        const auto target=pressed_target_.value_or(renderer_.hit_targets()[hit]);pressed_target_.reset();activate(target,GET_X_LPARAM(lparam)*96.f/dpi_-shadow_padding);
      }
      return 0;
    }
    if (background_press_) {
      POINT now{}; GetCursorPos(&now); ReleaseCapture(); background_press_ = false;
      if(moving_){moving_=false;config_writer_.enqueue(config_);schedule_collapse();return 0;}
      if (abs(now.x - drag_origin_.x) < 4 && abs(now.y - drag_origin_.y) < 4) {
        if (state_ == State::pinned) { state_ = State::expanded; schedule_collapse(); }
        else set_expanded(true, true);
      }
    } return 0;
  case WM_CAPTURECHANGED:if(moving_)config_writer_.enqueue(config_);background_press_=false;return 0;
  case WM_RBUTTONUP: { POINT p{}; GetCursorPos(&p); show_menu(p); return 0; }
  case WM_DROPFILES: {
    const auto drop = reinterpret_cast<HDROP>(wparam); const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
    for (UINT i = 0; i < count && files_.size() < 32; ++i) {
      const UINT length=DragQueryFileW(drop,i,nullptr,0);if(length>32767)continue;
      std::wstring path(length+1,L'\0');DragQueryFileW(drop,i,path.data(),length+1);path.resize(length);
      if(std::find(files_.begin(),files_.end(),path)==files_.end())files_.push_back(std::move(path));
    }
    DragFinish(drop); view_.page=Page::files;view_.scroll=0;set_expanded(true,true); render(); return 0;
  }
  case WM_CLIPBOARDUPDATE: update_clipboard(); return 0;
  case WM_COMMAND:
    switch (LOWORD(wparam)) {
    case CmdHide:toggle_hidden();break;
    case CmdReminders:open_reminders();break;
    case CmdSettings:settings_.open(instance_,window_,config_,[this]{apply_settings();},&google_calendar_,&windows_notifications_);break;
    case CmdToggle:
      if (state_ == State::pinned) { state_ = State::expanded; set_expanded(false); }
      else set_expanded(true, true);
      break;
    case CmdHover:
      config_.hover_expand = !config_.hover_expand; config_writer_.enqueue(config_);
      if (!config_.hover_expand && state_ != State::pinned) set_expanded(false);
      break;
    case CmdClipboard:
      config_.clipboard_history = !config_.clipboard_history;
      if (config_.clipboard_history) AddClipboardFormatListener(window_); else { RemoveClipboardFormatListener(window_); clipboard_.clear();++clipboard_revision_;view_.clipboard_query.clear();search_.close(); }
      config_writer_.enqueue(config_); render(); break;
    case CmdStartup: config_.start_with_windows = !config_.start_with_windows; if (!set_startup(config_.start_with_windows)) config_.start_with_windows = !config_.start_with_windows; config_writer_.enqueue(config_); break;
    case CmdClearFiles: files_.clear();resize_for_content();render(); break;
    case CmdPlay: media_.play_pause(); break; case CmdPrev: media_.previous(); break; case CmdNext: media_.next(); break;
    case CmdExit: DestroyWindow(window_); break;
    } return 0;
  case kTray:
    if(LOWORD(lparam)==NIN_BALLOONUSERCLICK){if(manually_hidden_)toggle_hidden();view_.page=Page::reminders;view_.scroll=0;set_expanded(true,true);render();return 0;}
    if (LOWORD(lparam) == WM_LBUTTONUP) {
      if(manually_hidden_){toggle_hidden();return 0;}
      if (state_ == State::pinned) { state_ = State::expanded; set_expanded(false); }
      else set_expanded(true, true);
    }
    else if (LOWORD(lparam) == WM_RBUTTONUP) { POINT p{}; GetCursorPos(&p); show_menu(p); }
    return 0;
  case WM_DESTROY:
    task_entry_.close();
    reminder_window_.close();google_calendar_.stop();
    search_.close();audio_.stop();
    UnregisterHotKey(window_,1);
    if(drop_target_){RevokeDragDrop(window_);drop_target_->Release();drop_target_=nullptr;}
    settings_.close();apps_.stop();monitor_.stop();media_.stop();remove_tray();PostQuitMessage(0);return 0;
  }
  return DefWindowProcW(window_, message, wparam, lparam);
}

void IslandWindow::render() {
  if(animation_.active()&&!drawing_animation_)return;
  const float collapsed = static_cast<float>(dip_to_px(config_.collapsed_height, dpi_));
  const float expanded = static_cast<float>(dip_to_px(expanded_height(), dpi_));
  const float progress = expanded > collapsed ? std::clamp((height_.value() - collapsed) / (expanded - collapsed), 0.0f, 1.0f) : 0.0f;
  // A taller compact Windows notice must not expose the expanded island pages.
  view_.expansion=(!expanded_&&view_.notice_kind==8&&!view_.notice.empty())?0.f:progress;view_.theme=theme_for(config_.theme_index,config_.custom_accent);
  if(config_.follow_system_theme) {
    DWORD light=0,size=sizeof(light);RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&light,&size);
    view_.theme=theme_for(light?5:0,config_.custom_accent);
  }
  view_.opacity=config_.opacity_percent/100.f;view_.clipboard_enabled=config_.clipboard_history;view_.files=files_;
  if(view_clipboard_revision_!=clipboard_revision_){view_.clipboard=clipboard_;view_clipboard_revision_=clipboard_revision_;}
  view_.audio=audio_.snapshot();view_.reduced_motion=config_.reduced_motion;
  audio_favorites_.apply(view_.audio);
  view_.google=google_calendar_.snapshot();view_.island_shape=config_.island_shape;
  auto entries=reminders_.snapshot();auto now=reminder_now();view_.reminder_date=reminder_day_at_offset(now,view_.reminder_offset);view_.reminders=reminders_for_day(entries,view_.reminder_date,true);view_.pending_today_reminders=reminders_pending_today(entries,now);view_.has_today_reminders=!view_.pending_today_reminders.empty();view_.calendar_status=reminders_.error().empty()?google_calendar_.snapshot().message:reminders_.error();
  view_.shadow=config_.shadow_percent/100.f;
  view_.height=static_cast<float>(expanded_height());view_.content_width=static_cast<float>(std::max(500,config_.expanded_width));view_.applications=apps_.snapshot();
  const auto hr = renderer_.render(monitor_.snapshot(), media_.snapshot(), view_,drawing_animation_);
  if (hr == D2DERR_RECREATE_TARGET || hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
    RECT rect{}; GetClientRect(window_, &rect); renderer_.discard_device();
    renderer_.initialize(window_, rect.right, rect.bottom, static_cast<float>(dpi_));
  }
}

void IslandWindow::set_expanded(bool expanded, bool pin) {
  pin=pin&&config_.pin_on_click;
  KillTimer(window_, kCollapseDelay);
  expanded_ = expanded;
  const bool toast=!view_.notice.empty()&&view_.notice_kind==8;
  const auto notice_size=compact_notice_size(view_.notice_app,view_.notice_title,view_.notice_body,static_cast<bool>(view_.notice_portrait));
  planned_height_=expanded?expanded_height():toast?notice_size.height:config_.collapsed_height;
  planned_width_=expanded?std::max(500,config_.expanded_width):toast?notice_size.width:config_.collapsed_width;
  state_ = expanded ? (pin ? State::pinned : State::expanding) : State::collapsing;
  const float target_width = static_cast<float>(dip_to_px(planned_width_,dpi_));
  const float target_height = static_cast<float>(dip_to_px(planned_height_, dpi_));
  float seconds=config_.animation_duration_ms/1000.f;
  if(config_.animation_style==2)seconds*=.65f;
  animation_.begin(width_.value(),height_.value(),target_width,target_height,seconds,config_.animation_style);
  if (config_.reduced_motion||config_.animation_style==3) { animation_.cancel();width_.reset(target_width); height_.reset(target_height); animate(); }
  else if(!renderer_.frame_ready())SetTimer(window_, kAnimate, 16, nullptr);
  update_media_animation();
}

void IslandWindow::animate(bool frame_ready) {
  if(animation_.active()){auto frame=animation_.frame();width_.reset(frame.width);height_.reset(frame.height);}
  const int width = std::max(1, static_cast<int>(std::round(width_.value())));
  const int height = std::max(1, static_cast<int>(std::round(height_.value())));
  const int pad=dip_to_px(shadow_padding,dpi_);
  SetWindowPos(window_,nullptr,anchor_center_x_-width/2-pad,anchor_top_-pad,width+2*pad,height+2*pad,SWP_NOACTIVATE|SWP_NOZORDER);
  renderer_.resize(width+2*pad,height+2*pad,static_cast<float>(dpi_));drawing_animation_=true;
  // Only mark the frame as consumed when it came from the DXGI wait handle.
  if(!frame_ready&&renderer_.frame_ready())WaitForSingleObjectEx(renderer_.frame_ready(),50,FALSE);
  render();drawing_animation_=false;
  if (!animation_.active()) {
    KillTimer(window_, kAnimate);
    if (state_ == State::expanding) state_ = State::expanded;
    else if (state_ == State::collapsing) state_ = State::collapsed;
  }
}

void IslandWindow::position() {
  MonitorSelect selection{std::max(0, config_.monitor_index)};
  EnumDisplayMonitors(nullptr, nullptr, monitor_at_index, reinterpret_cast<LPARAM>(&selection));
  HMONITOR monitor = selection.result;
  if (!monitor) monitor = MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
  MONITORINFO info{sizeof(info)}; GetMonitorInfoW(monitor, &info);
  const int half=dip_to_px(std::max(500,config_.expanded_width),dpi_)/2+8;
  const int span=std::max(0,static_cast<int>(info.rcMonitor.right-info.rcMonitor.left)-2*half);
  anchor_center_x_=span?info.rcMonitor.left+half+MulDiv(span,config_.horizontal_percent,100):info.rcMonitor.left+(info.rcMonitor.right-info.rcMonitor.left)/2;
  anchor_top_ = info.rcMonitor.top + (config_.island_shape==1?0:dip_to_px(std::clamp(config_.top_gap, 0, 40), dpi_));
  const int width = std::max(1, static_cast<int>(std::round(width_.value())));
  const int height = std::max(1, static_cast<int>(std::round(height_.value())));
  const int left = anchor_center_x_-width/2;
  const int pad=dip_to_px(shadow_padding,dpi_);
  SetWindowPos(window_,nullptr,left-pad,anchor_top_-pad,width+2*pad,height+2*pad,SWP_NOACTIVATE|SWP_NOZORDER);
}

void IslandWindow::schedule_collapse() {
  if (config_.auto_collapse && state_ != State::pinned && !outgoing_drag_ && !menu_active_ && !reminder_summary_open_ && !background_press_ && !view_.drop_active && pressed_hit_<0 && !settings_.visible()&&!search_.visible()&&!reminder_window_.visible()&&!task_entry_.visible()) SetTimer(window_, kCollapseDelay, 430, nullptr);
}

bool IslandWindow::point_inside_capsule(POINT point) const {
  RECT client{}; GetClientRect(window_, &client);
  const int pad=dip_to_px(shadow_padding,dpi_);point.x-=pad;point.y-=pad;client.right-=2*pad;client.bottom-=2*pad;
  const float width = static_cast<float>(client.right), height = static_cast<float>(client.bottom);
  const float radius = height/2+(30.f*dpi_/96.f-height/2)*view_.expansion;
  const float x = static_cast<float>(point.x), y = static_cast<float>(point.y);
  return island_contains(x*96/dpi_,y*96/dpi_,width*96/dpi_,height*96/dpi_,radius*96/dpi_,config_.island_shape==1);
}

bool IslandWindow::pointer_inside() const {
  POINT point{}; GetCursorPos(&point); ScreenToClient(window_, &point);
  return point_inside_capsule(point);
}

int IslandWindow::expanded_height()const {
  const bool bluetooth=!monitor_.snapshot().bluetooth_audio.empty();
  const size_t count=view_.page==Page::sound?audio_.snapshot().sessions.size():view_.page==Page::tasks?google_calendar_.snapshot().tasks.size():view_.page==Page::reminders?reminders_for_day(reminders_.snapshot(),reminder_day_at_offset(reminder_now(),view_.reminder_offset),true).size():view_.page==Page::applications?apps_.snapshot().size():files_.size();
  return page_height(view_.page,count,clipboard_matches(clipboard_,view_.clipboard_query).size(),config_.clipboard_history,bluetooth)+(view_.notice.empty()?0:34);
}
void IslandWindow::resize_for_content(){
  if(expanded_){if(planned_height_!=expanded_height())set_expanded(true,state_==State::pinned);return;}
  const bool toast=!view_.notice.empty()&&view_.notice_kind==8;
  const auto size=compact_notice_size(view_.notice_app,view_.notice_title,view_.notice_body,static_cast<bool>(view_.notice_portrait));
  const int h=toast?size.height:config_.collapsed_height;
  const int w=toast?size.width:config_.collapsed_width;
  if(planned_height_!=h||planned_width_!=w)set_expanded(false);
}
void IslandWindow::update_media_animation() {
  auto snapshot=media_.snapshot();
  const bool visible=!manually_hidden_&&!hidden_for_fullscreen_&&(!expanded_||view_.page==Page::media);
  wave_target_=snapshot.available&&snapshot.playing?1.f:0.f;
  if(!visible||config_.reduced_motion){KillTimer(window_,kWaves);waves_running_=false;view_.wave_mix=wave_target_;return;}
  if(wave_target_>0||view_.wave_mix>.01f) {
    if(!waves_running_){wave_start_=wave_frame_=std::chrono::steady_clock::now();waves_running_=true;SetTimer(window_,kWaves,33,nullptr);}
  }else{KillTimer(window_,kWaves);waves_running_=false;view_.wave_mix=0;}
}
void IslandWindow::add_application() {
  ComPtr<IFileOpenDialog> dialog;
  if(FAILED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,__uuidof(IFileOpenDialog),reinterpret_cast<void**>(dialog.put()))))return;
  COMDLG_FILTERSPEC filters[]{{L"Uygulamalar ve kısayollar",L"*.exe;*.lnk;*.appref-ms"}};
  dialog->SetFileTypes(1,filters);dialog->SetTitle(L"Uygulama ekle");
  DWORD options{};dialog->GetOptions(&options);dialog->SetOptions(options|FOS_FILEMUSTEXIST|FOS_FORCEFILESYSTEM|FOS_ALLOWMULTISELECT|FOS_NOCHANGEDIR|FOS_NODEREFERENCELINKS);
  outgoing_drag_=true;KillTimer(window_,kCollapseDelay);
  if(SUCCEEDED(dialog->Show(window_))) {
    ComPtr<IShellItemArray> results;if(SUCCEEDED(dialog->GetResults(results.put()))) {
      DWORD count{};results->GetCount(&count);for(DWORD i=0;i<count&&i<32;++i){ComPtr<IShellItem> item;PWSTR path{};if(SUCCEEDED(results->GetItemAt(i,item.put()))&&SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))){apps_.add(path);CoTaskMemFree(path);}}
    }
  }
  outgoing_drag_=false;schedule_collapse();
}

void IslandWindow::show_menu(POINT point) {
  HMENU menu = CreatePopupMenu();
  AppendMenuW(menu, MF_STRING, CmdToggle, expanded_ ? L"Küçült" : L"Genişlet");
  AppendMenuW(menu,MF_STRING,CmdHide,manually_hidden_?L"Adayı göster":L"Adayı tamamen gizle");
  AppendMenuW(menu,MF_STRING,CmdSettings,L"Ayarlar / Konum / Kısayol");
  AppendMenuW(menu,MF_STRING,CmdReminders,L"Hatırlatıcı ekle…");
  AppendMenuW(menu, MF_STRING | (config_.hover_expand ? MF_CHECKED : 0), CmdHover, L"Fareyle genişlet");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, CmdPrev, L"Önceki parça"); AppendMenuW(menu, MF_STRING, CmdPlay, L"Oynat / Duraklat"); AppendMenuW(menu, MF_STRING, CmdNext, L"Sonraki parça");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING | (config_.clipboard_history ? MF_CHECKED : 0), CmdClipboard, L"Pano geçmişi (yalnızca bellek)");
  AppendMenuW(menu, MF_STRING | (config_.start_with_windows ? MF_CHECKED : 0), CmdStartup, L"Windows ile başlat");
  AppendMenuW(menu, MF_STRING | (files_.empty() ? MF_GRAYED : 0), CmdClearFiles, L"Dosya rafını temizle");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr); AppendMenuW(menu, MF_STRING, CmdExit, L"Çıkış");
  SetForegroundWindow(window_); TrackPopupMenu(menu, TPM_RIGHTBUTTON, point.x, point.y, 0, window_, nullptr); DestroyMenu(menu);
}

int IslandWindow::hit_at(POINT point) const {
  const float x=point.x*96.f/dpi_-shadow_padding,y=point.y*96.f/dpi_-shadow_padding;
  const auto& targets=renderer_.hit_targets();
  for(size_t i=0;i<targets.size();++i){const auto& r=targets[i].rect;if(targets[i].enabled&&x>=r.left&&x<=r.right&&y>=r.top&&y<=r.bottom)return static_cast<int>(i);}
  return -1;
}

void IslandWindow::activate(const HitTarget& target,float x) {
  if(!target.enabled)return;
  switch(target.action) {
  case Action::tab:
    view_.page=static_cast<Page>(target.index);view_.scroll=0;view_.hovered=-1;
    config_.last_page=static_cast<int>(view_.page);config_writer_.enqueue(config_);resize_for_content();update_media_animation();
    view_.page_opacity=0;page_transition_=std::chrono::steady_clock::now();SetTimer(window_,kPageFade,16,nullptr);break;
  case Action::settings:settings_.open(instance_,window_,config_,[this]{apply_settings();},&google_calendar_,&windows_notifications_);break;
  case Action::manage_reminders:open_reminders();break;
  case Action::complete_reminder:if(target.index<view_.reminders.size()){if(reminders_.complete(view_.reminders[target.index].id)){if(view_.notice_kind==7){view_.notice.clear();KillTimer(window_,kNotice);reminder_summary_open_=false;}reminder_events();}resize_for_content();render();}break;
  case Action::reminder_day:
    if(target.index<3){
      const int offset=static_cast<int>(target.index)-1;
      if(view_.reminder_offset==offset)break;
      view_.reminder_offset=offset;view_.scroll=0;view_.hovered=-1;
      view_.page_opacity=config_.reduced_motion?1.f:0.f;
      page_transition_=std::chrono::steady_clock::now();
      if(!config_.reduced_motion)SetTimer(window_,kPageFade,16,nullptr);
      resize_for_content();render();
    }break;
  case Action::previous:media_.previous();break;
  case Action::play:media_.play_pause();break;
  case Action::next:media_.next();break;
  case Action::seek:{auto snapshot=media_.snapshot();media_.seek(snapshot.duration*std::clamp((x-target.rect.left)/(target.rect.right-target.rect.left),0.f,1.f));break;}
  case Action::remove_file:if(target.index<files_.size())files_.erase(files_.begin()+target.index);view_.scroll=0;break;
  case Action::reveal_file:
    if(target.index<files_.size()) {
      auto* path=new(std::nothrow)std::wstring(files_[target.index]);
      if(path && !TrySubmitThreadpoolCallback([](PTP_CALLBACK_INSTANCE,void* context){
        std::unique_ptr<std::wstring> file(static_cast<std::wstring*>(context));
        HRESULT apartment=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
        PIDLIST_ABSOLUTE item=ILCreateFromPathW(file->c_str());
        if(item){SHOpenFolderAndSelectItems(item,0,nullptr,0);CoTaskMemFree(item);}
        if(SUCCEEDED(apartment))CoUninitialize();
      },path,nullptr))delete path;
    }break;
  case Action::toggle_clip:
    SendMessageW(window_,WM_COMMAND,CmdClipboard,0);break;
  case Action::remove_clip:if(target.index<clipboard_.size()){clipboard_.erase(clipboard_.begin()+target.index);++clipboard_revision_;}view_.scroll=0;break;
  case Action::clear_clip:std::erase_if(clipboard_,[](auto& clip){return !clip.pinned;});++clipboard_revision_;view_.scroll=0;break;
  case Action::pin_clip:if(target.index<clipboard_.size()){clipboard_[target.index].pinned=!clipboard_[target.index].pinned;++clipboard_revision_;view_.scroll=0;}break;
  case Action::search_clips:search_.open(instance_,window_,view_.clipboard_query,[this](std::wstring query){view_.clipboard_query=std::move(query);view_.scroll=0;resize_for_content();render();});break;
  case Action::volume:audio_.volume(std::clamp((x-target.rect.left)/(target.rect.right-target.rect.left),0.f,1.f));break;
  case Action::session_volume:audio_.session_volume(target.key,std::clamp((x-target.rect.left)/(target.rect.right-target.rect.left),0.f,1.f));break;
  case Action::session_mute:{auto a=audio_.snapshot();for(auto& s:a.sessions)if(s.id==target.key)audio_.session_mute(s.id,!s.muted);break;}
  case Action::favorite_audio:if(audio_favorites_.toggle(target.key))view_.scroll=0;else notify(L"Ses favorisi kaydedilemedi",5);break;
  case Action::select_task_list:select_task_list();break;
  case Action::refresh_tasks:google_calendar_.refresh();break;
  case Action::add_task:{auto g=google_calendar_.snapshot();auto list=g.task_list_id;set_expanded(true,true);task_entry_.open(instance_,window_,[this,list](std::wstring title){google_calendar_.add_task(list,std::move(title));schedule_collapse();});break;}
  case Action::edit_task:{auto g=google_calendar_.snapshot();if(g.busy||!tasks_connected(g)||g.task_list_id!=view_.google.task_list_id)break;auto task=std::find_if(g.tasks.begin(),g.tasks.end(),[&](const auto& t){return t.id==target.key&&!t.completed;});if(task==g.tasks.end())break;auto list=g.task_list_id,id=task->id;set_expanded(true,true);task_entry_.open_edit(instance_,window_,id,task->title,[this,list,id](std::wstring title){return google_calendar_.update_task(list,id,std::move(title));});break;}
  case Action::complete_task:{for(auto& t:view_.google.tasks)if(t.id==target.key)google_calendar_.complete_task(view_.google.task_list_id,t.id,!t.completed);break;}
  case Action::delete_task:{auto g=google_calendar_.snapshot();if(g.busy||g.task_list_id!=view_.google.task_list_id)break;auto task=std::find_if(g.tasks.begin(),g.tasks.end(),[&](const auto& t){return t.id==target.key;});if(task==g.tasks.end())break;auto id=task->id,list=g.task_list_id;std::wstring text=L"“"+task->title+L"” görevi Google Tasks hesabından silinecek.\n\nSilmek istiyor musun?";menu_active_=true;int result=MessageBoxW(window_,text.c_str(),L"Görevi sil",MB_YESNO|MB_ICONWARNING|MB_DEFBUTTON2);menu_active_=false;if(result==IDYES){google_calendar_.delete_task(std::move(list),std::move(id));view_.scroll=0;}schedule_collapse();break;}
  case Action::mute_output:{auto a=audio_.snapshot();audio_.mute(false,!a.muted);break;}
  case Action::mute_mic:{auto a=audio_.snapshot();audio_.mute(true,!a.mic_muted);break;}
  case Action::select_output:select_audio_output();break;
  case Action::sound_settings:ShellExecuteW(nullptr,L"open",L"ms-settings:sound",nullptr,nullptr,SW_SHOWNORMAL);break;
  case Action::drag_file:break;
  case Action::add_app:add_application();break;
  case Action::launch_app:apps_.launch(target.index);break;
  case Action::remove_app:apps_.remove(target.index);view_.scroll=0;break;
  case Action::preview_clip:if(target.index<clipboard_.size())show_clipboard_preview(instance_,window_,clipboard_[target.index]);break;
  case Action::copy_clip:
    if(target.index<clipboard_.size()){auto entry=clipboard_[target.index];copy_clipboard(window_,entry);}break;
  default:break;
  }
  resize_for_content();render();
}

void IslandWindow::apply_settings() {
  if(!verification_)windows_notifications_.enable(config_.windows_notifications);
  if(!config_.windows_notifications){windows_notice_queue_.clear();clear_windows_notice();}
  else if(!config_.windows_notification_content)clear_windows_notice();
  if(!config_.smart_notifications){clear_windows_notice();view_.notice.clear();KillTimer(window_,kNotice);}
  if(expire_clipboard(clipboard_,GetTickCount64(),config_.clipboard_ttl_minutes,config_.expire_pinned_clips)){++clipboard_revision_;view_.scroll=0;}
  if(!config_.pin_on_click&&state_==State::pinned)state_=State::expanded;
  if(!verification_){UnregisterHotKey(window_,1);if(config_.hotkey_key&&!RegisterHotKey(window_,1,config_.hotkey_modifiers|MOD_NOREPEAT,config_.hotkey_key))MessageBoxW(window_,L"Kısayol kullanımda. Başka bir kombinasyon seçin.",L"NativeIsland",MB_OK|MB_ICONWARNING);}
  config_writer_.enqueue(config_);position();
  if(expanded_)set_expanded(true,state_==State::pinned);
  if(expanded_)schedule_collapse();
  render();
}

void IslandWindow::update_clipboard() {
  if(!config_.clipboard_history)return;auto entry=read_clipboard(window_);
  if((entry.image()||!entry.text.empty())&&(clipboard_.empty()||!(clipboard_.back()==entry))){clipboard_.push_back(std::move(entry));++clipboard_revision_;
    auto bytes=[&]{size_t total=0;for(auto& clip:clipboard_)total+=clip.dib.size()+clip.pixels.size()+clip.text.size()*sizeof(wchar_t);return total;};
    while(clipboard_.size()>100||bytes()>64*1024*1024){auto old=std::find_if(clipboard_.begin(),clipboard_.end(),[](auto& clip){return !clip.pinned;});if(old==clipboard_.end())break;clipboard_.erase(old);}}
  resize_for_content();render();
}
void IslandWindow::clear_windows_notice(){
  if(view_.notice_kind!=8)return;
  view_.notice.clear();view_.notice_app.clear();view_.notice_title.clear();view_.notice_body.clear();
  view_.notice_logo.reset();view_.notice_portrait.reset();windows_notice_app_id_.clear();
  windows_notice_id_=0;KillTimer(window_,kNotice);
}
void IslandWindow::windows_notification_events(){
  if(verification_||!config_.windows_notifications)return;
  // Never expose message content on a locked/noninteractive desktop.
  bool interactive=false;HDESK desktop=OpenInputDesktop(0,FALSE,DESKTOP_READOBJECTS);
  if(desktop){wchar_t name[128]{};DWORD bytes{};interactive=GetUserObjectInformationW(desktop,UOI_NAME,name,sizeof(name),&bytes)&&_wcsicmp(name,L"Default")==0;CloseDesktop(desktop);}
  if(!interactive){windows_notifications_.enable(false);windows_notice_queue_.clear();if(view_.notice_kind==8){clear_windows_notice();resize_for_content();render();}return;}
  windows_notifications_.enable(true);windows_notifications_.poll();
  auto incoming=windows_notifications_.take();
  if(manually_hidden_||hidden_for_fullscreen_){windows_notice_queue_.clear();return;}
  for(auto& notice:incoming){if(windows_notice_queue_.size()==20)windows_notice_queue_.pop_front();windows_notice_queue_.push_back(std::move(notice));}
  std::erase_if(windows_notice_queue_,[this](const auto& n){return !windows_notifications_.contains(n.id);});
  if(view_.notice_kind==8&&!view_.notice.empty()&&!windows_notifications_.contains(windows_notice_id_)){clear_windows_notice();resize_for_content();render();}
  if(view_.notice_kind==8&&!view_.notice.empty()){
    auto logo=windows_notifications_.logo_for(windows_notice_app_id_);
    if(logo!=view_.notice_logo){view_.notice_logo=std::move(logo);render();}
  }
  if(!view_.notice.empty()||windows_notice_queue_.empty())return;
  auto item=std::move(windows_notice_queue_.front());windows_notice_queue_.pop_front();
  windows_notice_id_=item.id;windows_notice_app_id_=item.app_id;
  view_.notice_logo=windows_notifications_.logo_for(windows_notice_app_id_);
  const auto presentation=windows_notice_presentation(item,config_.windows_notification_content);
  view_.notice_app=presentation.source;view_.notice_title=presentation.title;view_.notice_body=presentation.preview;
  view_.notice_portrait=presentation.portrait;
  view_.notice=view_.notice_app+L" · "+view_.notice_title;view_.notice_kind=8;view_.notice_age=0;
  notice_start_=std::chrono::steady_clock::now();notice_deadline_=notice_start_+std::chrono::seconds(8);
  SetTimer(window_,kNotice,config_.reduced_motion?8000:16,nullptr);resize_for_content();render();
}
void IslandWindow::open_reminders(){reminder_window_.open(instance_,window_,reminders_,[this]{reminder_events();resize_for_content();render();},reminder_day_at_offset(reminder_now(),view_.page==Page::reminders?view_.reminder_offset:0));}
void IslandWindow::reminder_events(){
  if(verification_)return;
  if(reminders_pending_today(reminders_.snapshot(),reminder_now()).empty()){
    reminder_notices_.clear();KillTimer(window_,kReminderDelivery);
    if(view_.notice_kind==7&&!view_.notice.empty()){view_.notice.clear();KillTimer(window_,kNotice);reminder_summary_open_=false;resize_for_content();schedule_collapse();}
  }
  if(!config_.reminder_notifications){reminder_notices_.clear();KillTimer(window_,kReminderDelivery);return;}
  bool idle=reminder_notices_.empty();for(auto& notice:reminders_.tick(reminder_now(),true,config_.reminder_morning_hour))reminder_notices_.push_back(std::move(notice));
  if(idle&&!reminder_notices_.empty())show_reminder_notice();
  resize_for_content();
}
void IslandWindow::show_reminder_notice(){
  if(reminder_notices_.empty())return;auto notice=std::move(reminder_notices_.front());reminder_notices_.pop_front();
  if(!manually_hidden_&&!hidden_for_fullscreen_){
    clear_windows_notice();
    view_.notice=notice.title;view_.notice_kind=7;view_.notice_age=0;notice_start_=std::chrono::steady_clock::now();notice_deadline_=notice_start_+std::chrono::seconds(8);SetTimer(window_,kNotice,config_.reduced_motion?8000:16,nullptr);
    if(notice.summary&&!expanded_){view_.page=Page::reminders;view_.reminder_offset=0;view_.scroll=0;reminder_summary_open_=true;set_expanded(true);}
    resize_for_content();render();
  }
  if(!reminder_notices_.empty())SetTimer(window_,kReminderDelivery,8000,nullptr);
}
void IslandWindow::notify(std::wstring text,int kind,float value){if(verification_||!config_.smart_notifications||manually_hidden_||hidden_for_fullscreen_)return;clear_windows_notice();auto now=std::chrono::steady_clock::now();if(kind==0&&!view_.notice.empty()&&view_.notice_kind!=0&&std::chrono::duration<float>(now-notice_start_).count()<.7f)return;bool merge=kind==0&&!view_.notice.empty()&&view_.notice_kind==0;view_.notice=std::move(text);view_.notice_kind=kind;view_.notice_value=value;if(!merge){view_.notice_age=0;notice_start_=now;}notice_deadline_=now+std::chrono::milliseconds(2600);SetTimer(window_,kNotice,config_.reduced_motion?2600:16,nullptr);resize_for_content();render();}
void IslandWindow::smart_events(){
  auto a=audio_.snapshot();
  if(audio_baseline_){
    if(!a.error.empty()&&a.error!=previous_audio_.error)notify(a.error,5);
    else if(a.mic_available&&previous_audio_.mic_available&&a.mic_muted!=previous_audio_.mic_muted)notify(a.mic_muted?L"Mikrofon sessize alındı":L"Mikrofon yeniden açık",a.mic_muted?1:6);
    else if(a.output_id!=previous_audio_.output_id)notify(a.available?L"Ses çıkışı: "+a.output_name:L"Ses çıkışı bağlantısı kesildi",2);
    else if(a.outputs!=previous_audio_.outputs){auto added=std::find_if(a.outputs.begin(),a.outputs.end(),[&](auto& device){return std::none_of(previous_audio_.outputs.begin(),previous_audio_.outputs.end(),[&](auto& old){return old.id==device.id;});});notify(added!=a.outputs.end()?L"Ses aygıtı bağlandı: "+added->name:L"Ses aygıtı bağlantısı kesildi",2);}
    else if(a.available&&a.muted!=previous_audio_.muted)notify(a.muted?L"Ses sessize alındı":L"Ses yeniden açık",4);
    else if(a.available&&std::lround(a.volume*100)!=std::lround(previous_audio_.volume*100))notify(std::format(L"Ses seviyesi · {:.0f}%",a.volume*100),0,a.volume);
  }
  if(a.available||a.mic_available)audio_baseline_=true;previous_audio_=a;
  auto bluetooth=monitor_.snapshot().bluetooth_audio;
  if(bluetooth_baseline_&&bluetooth!=previous_bluetooth_)notify(bluetooth.empty()?L"Bluetooth ses bağlantısı kesildi":L"Bluetooth: "+bluetooth.front(),2);
  bluetooth_baseline_=true;previous_bluetooth_=std::move(bluetooth);
  SYSTEM_POWER_STATUS power{};if(GetSystemPowerStatus(&power)&&!(power.BatteryFlag&128)&&power.BatteryLifePercent!=255){if(power.ACLineStatus==1||power.BatteryLifePercent>25)battery_warning_=0;else{int level=power.BatteryLifePercent<=10?10:power.BatteryLifePercent<=20?20:0;if(level&&(battery_warning_==0||level<battery_warning_)){battery_warning_=level;notify(std::format(L"Düşük pil · %{}",power.BatteryLifePercent),3);}}}
}
void IslandWindow::select_audio_output(){auto a=audio_.snapshot();if(!a.output_switch_supported){notify(L"Doğrudan çıkış seçimi desteklenmiyor",5);ShellExecuteW(nullptr,L"open",L"ms-settings:sound",nullptr,nullptr,SW_SHOWNORMAL);return;}HMENU menu=CreatePopupMenu();for(size_t i=0;i<a.outputs.size();++i)AppendMenuW(menu,MF_STRING|(a.outputs[i].id==a.output_id?MF_CHECKED:0),static_cast<UINT_PTR>(i+1),a.outputs[i].name.c_str());POINT point{};GetCursorPos(&point);outgoing_drag_=true;KillTimer(window_,kCollapseDelay);SetForegroundWindow(window_);UINT choice=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,point.x,point.y,0,window_,nullptr);DestroyMenu(menu);if(choice&&choice<=a.outputs.size())audio_.output(a.outputs[choice-1].id);outgoing_drag_=false;schedule_collapse();}
void IslandWindow::toggle_hidden(){manually_hidden_=!manually_hidden_;KillTimer(window_,kCollapseDelay);ShowWindow(window_,manually_hidden_||hidden_for_fullscreen_?SW_HIDE:SW_SHOWNOACTIVATE);monitor_.set_visible(!manually_hidden_&&!hidden_for_fullscreen_);media_.set_visible(!manually_hidden_&&!hidden_for_fullscreen_);update_media_animation();}

void IslandWindow::add_tray() {
  tray_.cbSize = sizeof(tray_); tray_.hWnd = window_; tray_.uID = 1; tray_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
  tray_.uCallbackMessage = kTray; tray_.hIcon = LoadIconW(nullptr, IDI_APPLICATION); wcscpy_s(tray_.szTip, L"Native Island");
  Shell_NotifyIconW(NIM_ADD, &tray_); tray_.uVersion = NOTIFYICON_VERSION_4; Shell_NotifyIconW(NIM_SETVERSION, &tray_);
}
void IslandWindow::remove_tray() { if (tray_.hWnd) { Shell_NotifyIconW(NIM_DELETE, &tray_); tray_.hWnd = nullptr; } }

void IslandWindow::check_fullscreen() {
  if(manually_hidden_)return;
  if (!config_.auto_hide_fullscreen) {
    if(hidden_for_fullscreen_){hidden_for_fullscreen_=false;state_=state_before_hidden_;ShowWindow(window_,SW_SHOWNOACTIVATE);position();monitor_.set_visible(true);media_.set_visible(true);}
    return;
  }
  HWND foreground = GetForegroundWindow(); bool fullscreen = false;
  if (foreground && foreground != window_ && !IsIconic(foreground)) {
    wchar_t class_name[80]{};GetClassNameW(foreground,class_name,80);
    RECT wr{}; GetWindowRect(foreground, &wr); HMONITOR monitor = MonitorFromWindow(foreground, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{sizeof(mi)}; GetMonitorInfoW(monitor, &mi);
    fullscreen = wr.left <= mi.rcMonitor.left && wr.top <= mi.rcMonitor.top && wr.right >= mi.rcMonitor.right && wr.bottom >= mi.rcMonitor.bottom;
    if(foreground==GetShellWindow() || wcscmp(class_name,L"Progman")==0 || wcscmp(class_name,L"WorkerW")==0 ||
       wcscmp(class_name,L"Shell_TrayWnd")==0 || wcscmp(class_name,L"SHELLDLL_DefView")==0 || wcscmp(class_name,L"SysListView32")==0 ||
       wcscmp(class_name,L"NativeIslandSettings")==0 || monitor!=MonitorFromWindow(window_,MONITOR_DEFAULTTONEAREST))fullscreen=false;
    DWORD process{},shell_process{};GetWindowThreadProcessId(foreground,&process);GetWindowThreadProcessId(GetShellWindow(),&shell_process);
    if(process==GetCurrentProcessId()||(shell_process&&process==shell_process))fullscreen=false;
  }
  if (fullscreen != hidden_for_fullscreen_) {
    hidden_for_fullscreen_ = fullscreen;
    if (fullscreen) { state_before_hidden_ = state_; state_ = State::hidden; }
    else { state_ = state_before_hidden_; position(); }
    ShowWindow(window_, fullscreen ? SW_HIDE : SW_SHOWNOACTIVATE);
    monitor_.set_visible(!fullscreen);
    media_.set_visible(!fullscreen);
    update_media_animation();
  }else if(!fullscreen) {
    // Topmost is persistent. Re-inserting every second raises us over native menus.
    if(!menu_active_&&!(GetWindowLongPtrW(window_,GWL_EXSTYLE)&WS_EX_TOPMOST))
      SetWindowPos(window_,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
    if(!IsWindowVisible(window_))ShowWindow(window_,SW_SHOWNOACTIVATE);
  }
}
}

