#include "ui/settings_window.hpp"
#include "ui/theme.hpp"
#include <commctrl.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <string>
#include <algorithm>
#include <format>
#include "core/com_ptr.hpp"
#include <wincodec.h>
#include <shellapi.h>

namespace ni {
namespace {constexpr int ThemeBase=200,Color=220,Follow=221,Hover=222,Collapse=223,Remember=224,Startup=225,PinClick=226,Gap=230,Opacity=231,Speed=232,Width=233,Shadow=234,Save=240,Reset=241,Monitor=250,Left=251,Center=252,Right=253,Hotkey=254,WinKey=255,AnimationStyle=256;
BOOL CALLBACK count_monitors(HMONITOR,HDC,LPRECT,LPARAM data){++*reinterpret_cast<int*>(data);return TRUE;}}
namespace {constexpr int MorningEnabled=270,MorningHour=271,GoogleHeader=272,Connect=273,Sync=274,Disconnect=275,GoogleStatus=276,Calendars=277,ApplyCalendars=278;}
void SettingsWindow::open(HINSTANCE instance,HWND owner,Config& config,std::function<void()> changed,GoogleCalendarService* google,WindowsNotifications* notifications) {
  if(window_){ShowWindow(window_,SW_SHOWNORMAL);SetForegroundWindow(window_);return;}
  config_=&config;changed_=std::move(changed);google_=google;notifications_=notifications;calendar_signature_.clear();
  WNDCLASSW wc{};wc.hInstance=instance;wc.lpfnWndProc=proc;wc.lpszClassName=L"NativeIslandSettings";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);
  window_=CreateWindowExW(WS_EX_TOOLWINDOW,wc.lpszClassName,L"NativeIsland · Ayarlar",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_VSCROLL,
    CW_USEDEFAULT,CW_USEDEFAULT,560,896,owner,nullptr,instance,this);
  dpi_=GetDpiForWindow(window_);font_=CreateFontW(-MulDiv(13,dpi_,96),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
  heading_font_=CreateFontW(-MulDiv(25,dpi_,96),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
  background_=CreateSolidBrush(RGB(16,20,27));surface_=CreateSolidBrush(RGB(25,30,39));BOOL dark=TRUE;DwmSetWindowAttribute(window_,20,&dark,sizeof(dark));DWORD corner=2;DwmSetWindowAttribute(window_,33,&corner,sizeof(corner));
  RECT desired{0,0,MulDiv(744,dpi_,96),MulDiv(606,dpi_,96)};AdjustWindowRectExForDpi(&desired,GetWindowLongW(window_,GWL_STYLE),FALSE,WS_EX_TOOLWINDOW,dpi_);
  MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(owner,MONITOR_DEFAULTTONEAREST),&monitor);
  int width=desired.right-desired.left,height=std::min(desired.bottom-desired.top,monitor.rcWork.bottom-monitor.rcWork.top-24);
  scroll_=0;page_=0;SetWindowPos(window_,nullptr,monitor.rcWork.left+(monitor.rcWork.right-monitor.rcWork.left-width)/2,monitor.rcWork.top+12,width,height,SWP_NOZORDER);
  populate();refresh();refresh_google(true);SetTimer(window_,1,1000,nullptr);ShowWindow(window_,SW_SHOW);
}
void SettingsWindow::close(){if(window_)DestroyWindow(window_);}
bool SettingsWindow::capture_preview(const std::filesystem::path& path){
  if(!window_)return false;RECT r{};GetWindowRect(window_,&r);int width=r.right-r.left,height=r.bottom-r.top;
  HDC screen=GetDC(window_),memory=CreateCompatibleDC(screen);HBITMAP bitmap=CreateCompatibleBitmap(screen,width,height);auto old=SelectObject(memory,bitmap);
  RedrawWindow(window_,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);
  BOOL painted=TRUE;SendMessageW(window_,WM_PRINT,reinterpret_cast<WPARAM>(memory),PRF_CLIENT|PRF_NONCLIENT|PRF_CHILDREN|PRF_ERASEBKGND);
  SelectObject(memory,old);DeleteDC(memory);ReleaseDC(window_,screen);
  ComPtr<IWICImagingFactory> factory;ComPtr<IWICBitmap> image;ComPtr<IWICStream> stream;ComPtr<IWICBitmapEncoder> encoder;ComPtr<IWICBitmapFrameEncode> frame;
  bool ok=painted&&SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,__uuidof(IWICImagingFactory),reinterpret_cast<void**>(factory.put())))&&SUCCEEDED(factory->CreateBitmapFromHBITMAP(bitmap,nullptr,WICBitmapIgnoreAlpha,image.put()))&&SUCCEEDED(factory->CreateStream(stream.put()))&&SUCCEEDED(stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE))&&SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,encoder.put()))&&SUCCEEDED(encoder->Initialize(stream.get(),WICBitmapEncoderNoCache))&&SUCCEEDED(encoder->CreateNewFrame(frame.put(),nullptr))&&SUCCEEDED(frame->Initialize(nullptr))&&SUCCEEDED(frame->SetSize(width,height));
  WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGR;
  if(ok)ok=SUCCEEDED(frame->SetPixelFormat(&format))&&SUCCEEDED(frame->WriteSource(image.get(),nullptr))&&SUCCEEDED(frame->Commit())&&SUCCEEDED(encoder->Commit());
  DeleteObject(bitmap);return ok;
}
LRESULT CALLBACK SettingsWindow::proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
  auto* self=reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
  if(msg==WM_NCCREATE){self=static_cast<SettingsWindow*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);self->window_=hwnd;SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
  return self?self->handle(msg,wp,lp):DefWindowProcW(hwnd,msg,wp,lp);
}
void SettingsWindow::populate() {
  page_controls_.clear();int group=-1;
  auto control=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int id,int x,int y,int w,int h){
    if(wcscmp(cls,L"COMBOBOX")==0)style|=CBS_OWNERDRAWFIXED|CBS_HASSTRINGS;
    if(wcscmp(cls,L"BUTTON")==0&&(style&BS_TYPEMASK)==BS_PUSHBUTTON)style=(style&~BS_TYPEMASK)|BS_OWNERDRAW;
    HWND c=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,MulDiv(x,dpi_,96),MulDiv(y,dpi_,96),MulDiv(w,dpi_,96),MulDiv(h,dpi_,96),window_,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),nullptr,nullptr);
    SendMessageW(c,WM_SETFONT,reinterpret_cast<WPARAM>(font_),TRUE);
    if(group>=0)page_controls_.push_back({c,group});
    return c;
  };
  const wchar_t* pages[]{L"Görünüm",L"Davranış",L"Pano",L"Google Takvim",L"Bildirimler",L"Google Tasks"};
  for(int i=0;i<6;++i)control(L"BUTTON",pages[i],BS_OWNERDRAW|WS_TABSTOP,280+i,20,116+i*54,158,44);
  control(L"STATIC",L"Değişiklikler anında uygulanır.",0,0,212,562,280,25);
  control(L"BUTTON",L"Sıfırla",BS_PUSHBUTTON|WS_TABSTOP,Reset,500,554,88,36);
  control(L"BUTTON",L"Tamam",BS_PUSHBUTTON|WS_TABSTOP,Save,602,554,120,36);
  group=0;
  for(int i=0;i<6;++i)control(L"BUTTON",themes[i].name,BS_OWNERDRAW|WS_TABSTOP,ThemeBase+i,224+(i%3)*161,156+(i/3)*70,147,58);
  control(L"BUTTON",L"Özel vurgu rengi",BS_PUSHBUTTON|WS_TABSTOP,Color,224,302,180,32);
  control(L"BUTTON",L"Windows temasını takip et",BS_AUTOCHECKBOX|WS_TABSTOP,Follow,422,304,275,28);
  control(L"STATIC",L"Ada biçimi",0,0,224,336,170,24);
  HWND shape=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,300,460,332,236,140);for(auto name:{L"Kapsül",L"Damla · ekranın üstüne bağlı"})SendMessageW(shape,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name));
  const wchar_t* labels[]{L"Üst boşluk",L"Opaklık",L"Animasyon süresi",L"Panel genişliği",L"Gölge"};
  int minimum[]{0,50,120,465,0},maximum[]{40,100,600,650,100};
  for(int i=0;i<5;++i){control(L"STATIC",labels[i],0,0,224,376+i*32,180,24);HWND c=control(TRACKBAR_CLASSW,L"",TBS_HORZ|TBS_NOTICKS|WS_TABSTOP,Gap+i,400,370+i*32,250,28);SendMessageW(c,TBM_SETRANGE,TRUE,MAKELPARAM(minimum[i],maximum[i]));control(L"STATIC",L"",SS_RIGHT,290+i,650,376+i*32,50,24);}
  group=1;
  control(L"BUTTON",L"Fare üzerine gelince aç",BS_AUTOCHECKBOX|WS_TABSTOP,Hover,224,156,230,28);
  control(L"BUTTON",L"Fare ayrılınca küçült",BS_AUTOCHECKBOX|WS_TABSTOP,Collapse,476,156,230,28);
  control(L"BUTTON",L"Son sekmeyi hatırla",BS_AUTOCHECKBOX|WS_TABSTOP,Remember,224,190,230,28);
  control(L"BUTTON",L"Tıklayınca açık tut",BS_AUTOCHECKBOX|WS_TABSTOP,PinClick,476,190,230,28);
  control(L"BUTTON",L"Windows başladığında aç",BS_AUTOCHECKBOX|WS_TABSTOP,Startup,224,224,230,28);
  control(L"BUTTON",L"Hareketi azalt",BS_AUTOCHECKBOX|WS_TABSTOP,261,476,224,230,28);
  control(L"STATIC",L"Açılış animasyonu",0,257,224,270,205,26);
  HWND styles=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,AnimationStyle,460,262,236,170);
  for(auto name:{L"Yumuşak",L"Yaylı",L"Hızlı",L"Animasyonsuz"})SendMessageW(styles,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name));
  control(L"STATIC",L"Monitör",0,0,224,311,180,24);
  HWND monitors=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,Monitor,460,303,236,180);
  int count=0;EnumDisplayMonitors(nullptr,nullptr,count_monitors,reinterpret_cast<LPARAM>(&count));
  for(int i=0;i<count;++i){std::wstring label=i?L"Ek monitör "+std::to_wstring(i):L"Ana monitör";SendMessageW(monitors,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));}
  control(L"STATIC",L"Yatay konum",0,0,224,357,205,24);
  control(L"BUTTON",L"Sol",BS_PUSHBUTTON|WS_TABSTOP,Left,460,345,72,32);control(L"BUTTON",L"Ortala",BS_PUSHBUTTON|WS_TABSTOP,Center,542,345,72,32);control(L"BUTTON",L"Sağ",BS_PUSHBUTTON|WS_TABSTOP,Right,624,345,72,32);
  control(L"STATIC",L"Gizle / göster kısayolu",0,0,224,398,210,24);
  control(HOTKEY_CLASSW,L"",WS_TABSTOP,Hotkey,460,389,128,30);control(L"BUTTON",L"Win tuşu",BS_AUTOCHECKBOX|WS_TABSTOP,WinKey,601,389,102,30);
  control(L"BUTTON",L"Akıllı bildirimler",BS_AUTOCHECKBOX|WS_TABSTOP,260,224,441,230,28);
  control(L"BUTTON",L"Ada içinde çan bildirimi",BS_AUTOCHECKBOX|WS_TABSTOP,MorningEnabled,476,441,230,28);
  control(L"STATIC",L"Gün başlangıcı",0,0,224,493,205,24);
  HWND hours=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,MorningHour,460,485,236,160);
  for(int hour=0;hour<24;++hour){auto label=std::format(L"{:02}:00",hour);SendMessageW(hours,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));}
  group=2;
  control(L"STATIC",L"Kopyalanan metin ve görseller için saklama süresi.",0,0,224,158,470,26);
  control(L"STATIC",L"Otomatik temizleme",0,0,224,208,210,26);
  HWND ttl=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,262,460,200,236,180);
  for(auto name:{L"Kapalı",L"5 dakika",L"15 dakika",L"30 dakika",L"1 saat",L"2 saat",L"24 saat"})SendMessageW(ttl,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name));
  control(L"BUTTON",L"Sabit kayıtları da süre sonunda temizle",BS_AUTOCHECKBOX|WS_TABSTOP,263,224,257,470,28);
  control(L"STATIC",L"Pano geçmişi bellekte tutulur.\nUygulama kapatıldığında geçmiş temizlenir.\nSabitlenen kayıtlar, yukarıdaki seçenek kapalıysa süreden etkilenmez.",0,0,224,318,472,110);
  group=3;
  control(L"STATIC",L"Takvimlerini bağla, bugünün etkinliklerini adada gör.",0,GoogleHeader,224,158,470,26);
  control(L"BUTTON",L"Google bağla (JSON)",BS_PUSHBUTTON|WS_TABSTOP,Connect,224,200,186,36);
  control(L"BUTTON",L"Eşitle",BS_PUSHBUTTON|WS_TABSTOP,Sync,422,200,100,36);
  control(L"BUTTON",L"Bağlantıyı kaldır",BS_PUSHBUTTON|WS_TABSTOP,Disconnect,534,200,162,36);
  control(L"STATIC",L"",0,GoogleStatus,224,254,472,48);
  control(L"STATIC",L"Gösterilecek takvimler",0,0,224,318,470,24);
  control(L"LISTBOX",L"",LBS_EXTENDEDSEL|LBS_NOTIFY|WS_BORDER|WS_VSCROLL|WS_TABSTOP,Calendars,224,348,472,126);
  control(L"STATIC",L"Ctrl ile birden fazla takvim seçebilirsin.",0,0,224,493,268,28);
  control(L"BUTTON",L"Seçimi uygula",BS_PUSHBUTTON|WS_TABSTOP,ApplyCalendars,526,488,170,34);
  group=4;
  control(L"BUTTON",L"Windows bildirimlerini adada göster",BS_AUTOCHECKBOX|WS_TABSTOP,295,224,158,472,28);
  control(L"BUTTON",L"Mesaj başlığını ve içeriğini göster",BS_AUTOCHECKBOX|WS_TABSTOP,296,224,200,472,28);
  control(L"BUTTON",L"Erişim izni ver",BS_PUSHBUTTON|WS_TABSTOP,297,224,252,200,36);
  control(L"BUTTON",L"Windows bildirim ayarları",BS_PUSHBUTTON|WS_TABSTOP,298,438,252,258,36);
  control(L"STATIC",L"",0,299,224,310,472,64);
  control(L"STATIC",L"Tüm uygulamaların yeni Windows bildirimleri 8 saniye görünür.\nEski bildirimler tekrar gösterilmez. İçerikler dosyaya kaydedilmez.\nİçeriği gizleyerek yalnızca uygulama adını gösterebilirsin.\nWindows'un kendi bildirimleri silinmez veya kapatılmaz.",0,0,224,390,472,118);
  group=5;
  control(L"STATIC",L"Çan yanındaki görev ikonundan görevlerini gör, ekle ve tamamla.",0,0,224,158,472,26);
  control(L"BUTTON",L"Tasks izni ver",BS_PUSHBUTTON|WS_TABSTOP,301,224,200,220,36);
  control(L"BUTTON",L"JSON ile bağla",BS_PUSHBUTTON|WS_TABSTOP,302,460,200,236,36);
  control(L"STATIC",L"",0,303,224,254,472,58);
  control(L"STATIC",L"Görev listesi",0,0,224,328,200,24);
  control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP|WS_VSCROLL,304,224,360,472,160);
  control(L"BUTTON",L"Şimdi eşitle",BS_PUSHBUTTON|WS_TABSTOP,305,224,412,160,34);
  control(L"STATIC",L"Google Cloud projesinde Tasks API'yi etkinleştirin.\nTasks için ek izin tarayıcıda onaylanır; Takvim salt okunur kalır.\nİzinler ve liste seçimi güncellemelerde korunur.\nBağlantı kaldırma: Google Takvim sayfası (iki servisi de ayırır).",0,0,224,458,472,72);
  select_page(page_);
}
void SettingsWindow::select_page(int page){
  page_=std::clamp(page,0,5);
  for(auto& item:page_controls_){if(!IsWindow(item.window))continue;ShowWindow(item.window,item.page==page_?SW_SHOW:SW_HIDE);}
  for(int i=0;i<6;++i)InvalidateRect(GetDlgItem(window_,280+i),nullptr,TRUE);
  InvalidateRect(window_,nullptr,TRUE);
}
void SettingsWindow::paint(HDC dc){
  auto scale=[&](int v){return MulDiv(v,dpi_,96);};
  RECT all{};GetClientRect(window_,&all);FillRect(dc,&all,background_);
  auto card=[&](int x,int y,int w,int h,COLORREF color){auto brush=CreateSolidBrush(color);auto pen=CreatePen(PS_SOLID,1,RGB(43,49,60));auto oldbrush=SelectObject(dc,brush);auto oldpen=SelectObject(dc,pen);RoundRect(dc,scale(x),scale(y),scale(x+w),scale(y+h),scale(18),scale(18));SelectObject(dc,oldbrush);SelectObject(dc,oldpen);DeleteObject(brush);DeleteObject(pen);};
  card(204,108,518,430,RGB(25,30,39));
  auto text=[&](const wchar_t* value,int x,int y,int w,int h,HFONT font,COLORREF color){auto old=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,color);RECT r{scale(x),scale(y),scale(x+w),scale(y+h)};DrawTextW(dc,value,-1,&r,DT_LEFT|DT_VCENTER|DT_SINGLELINE);SelectObject(dc,old);};
  text(L"NativeIsland",24,18,400,34,heading_font_,RGB(242,245,250));
  text(L"Adanı kendine göre düzenle.",24,56,500,26,font_,RGB(147,160,181));
  const wchar_t* names[]{L"Tema ve görünüm",L"Davranış ve konum",L"Pano ve gizlilik",L"Google Takvim",L"Windows bildirimleri",L"Google Tasks · Yapılacaklar"};
  text(names[page_],224,116,470,30,font_,RGB(236,240,248));
}
void SettingsWindow::refresh() {
  auto check=[&](int id,bool value){SendDlgItemMessageW(window_,id,BM_SETCHECK,value?BST_CHECKED:BST_UNCHECKED,0);};
  check(Follow,config_->follow_system_theme);check(Hover,config_->hover_expand);check(Collapse,config_->auto_collapse);check(Remember,config_->remember_page);
  check(Startup,config_->start_with_windows);check(PinClick,config_->pin_on_click);
  check(295,config_->windows_notifications);check(296,config_->windows_notification_content);EnableWindow(GetDlgItem(window_,297),notifications_&&config_->windows_notifications);
  SetDlgItemTextW(window_,299,notifications_?notifications_->status().c_str():L"Kapalı · Bildirim içeriği kaydedilmez.");
  check(MorningEnabled,config_->reminder_notifications);SendDlgItemMessageW(window_,MorningHour,CB_SETCURSEL,config_->reminder_morning_hour,0);
  check(260,config_->smart_notifications);check(261,config_->reduced_motion);check(263,config_->expire_pinned_clips);
  int ttl_values[]{0,5,15,30,60,120,1440};int ttl_selection=3;for(int i=0;i<7;++i)if(ttl_values[i]==config_->clipboard_ttl_minutes)ttl_selection=i;SendDlgItemMessageW(window_,262,CB_SETCURSEL,ttl_selection,0);
  int values[]{config_->top_gap,config_->opacity_percent,config_->animation_duration_ms,config_->expanded_width,config_->shadow_percent};
  for(int i=0;i<5;++i)SendDlgItemMessageW(window_,Gap+i,TBM_SETPOS,TRUE,values[i]);
  for(int i=0;i<5;++i){auto label=std::to_wstring(values[i])+(i==1||i==4?L"%":i==2?L" ms":L"");SetDlgItemTextW(window_,290+i,label.c_str());}
  SendDlgItemMessageW(window_,Monitor,CB_SETCURSEL,config_->monitor_index,0);
  SendDlgItemMessageW(window_,AnimationStyle,CB_SETCURSEL,config_->animation_style,0);
  SendDlgItemMessageW(window_,300,CB_SETCURSEL,config_->island_shape,0);
  WORD mods=0;if(config_->hotkey_modifiers&MOD_CONTROL)mods|=HOTKEYF_CONTROL;if(config_->hotkey_modifiers&MOD_ALT)mods|=HOTKEYF_ALT;if(config_->hotkey_modifiers&MOD_SHIFT)mods|=HOTKEYF_SHIFT;
  SendDlgItemMessageW(window_,Hotkey,HKM_SETHOTKEY,MAKEWORD(config_->hotkey_key,mods),0);check(WinKey,(config_->hotkey_modifiers&MOD_WIN)!=0);
  InvalidateRect(window_,nullptr,TRUE);
}
void SettingsWindow::refresh_google(bool force){
  GoogleCalendarStatus state;if(google_)state=google_->snapshot();else state.message=L"Google servisi kullanılamıyor";
  SetDlgItemTextW(window_,GoogleStatus,state.message.c_str());
  SetDlgItemTextW(window_,303,state.busy?L"Google ile bağlantı işlemi sürüyor…":state.tasks_message.c_str());for(int id:{301,302,304,305})EnableWindow(GetDlgItem(window_,id),google_&&!state.busy&&(id==302||(id==301?state.connected:state.tasks_enabled)));
  std::wstring task_signature=state.task_list_id;for(auto& list:state.task_lists)task_signature+=L"|"+list.id+L"|"+list.name;
  if(force||task_signature!=task_signature_){task_signature_=task_signature;task_lists_=state.task_lists;SendDlgItemMessageW(window_,304,CB_RESETCONTENT,0,0);for(size_t i=0;i<task_lists_.size();++i){SendDlgItemMessageW(window_,304,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(task_lists_[i].name.c_str()));if(task_lists_[i].id==state.task_list_id)SendDlgItemMessageW(window_,304,CB_SETCURSEL,i,0);}}
  for(int id:{Connect,Sync,Disconnect,ApplyCalendars})EnableWindow(GetDlgItem(window_,id),google_&&!state.busy&&(id==Connect||state.connected));
  std::wstring signature;for(auto& calendar:state.calendars)signature+=calendar.id+calendar.name+(calendar.selected?L"1":L"0");
  if(force||signature!=calendar_signature_){calendar_signature_=std::move(signature);calendars_=std::move(state.calendars);SendDlgItemMessageW(window_,Calendars,LB_RESETCONTENT,0,0);
    for(size_t i=0;i<calendars_.size();++i){SendDlgItemMessageW(window_,Calendars,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(calendars_[i].name.c_str()));SendDlgItemMessageW(window_,Calendars,LB_SETSEL,calendars_[i].selected,i);}}
}
LRESULT SettingsWindow::handle(UINT msg,WPARAM wp,LPARAM lp) {
  switch(msg){
  case WM_SIZE:{SCROLLINFO info{sizeof(info),SIF_RANGE|SIF_PAGE};info.nMax=MulDiv(605,dpi_,96);info.nPage=HIWORD(lp);SetScrollInfo(window_,SB_VERT,&info,TRUE);return 0;}
  case WM_PAINT:{PAINTSTRUCT ps{};HDC dc=BeginPaint(window_,&ps);paint(dc);EndPaint(window_,&ps);return 0;}
  case WM_PRINTCLIENT:paint(reinterpret_cast<HDC>(wp));return 0;
  case WM_TIMER:refresh_google();if(notifications_)SetDlgItemTextW(window_,299,notifications_->status().c_str());return 0;
  case WM_MOUSEWHEEL:SendMessageW(window_,WM_VSCROLL,static_cast<short>(HIWORD(wp))<0?SB_LINEDOWN:SB_LINEUP,0);return 0;
  case WM_VSCROLL:{SCROLLINFO info{sizeof(info),SIF_ALL};GetScrollInfo(window_,SB_VERT,&info);int next=scroll_;switch(LOWORD(wp)){case SB_LINEUP:next-=32;break;case SB_LINEDOWN:next+=32;break;case SB_PAGEUP:next-=info.nPage;break;case SB_PAGEDOWN:next+=info.nPage;break;case SB_THUMBTRACK:next=info.nTrackPos;break;}next=std::clamp(next,0,std::max(0,info.nMax-static_cast<int>(info.nPage)+1));ScrollWindowEx(window_,0,scroll_-next,nullptr,nullptr,nullptr,nullptr,SW_SCROLLCHILDREN|SW_INVALIDATE|SW_ERASE);scroll_=next;SetScrollPos(window_,SB_VERT,next,TRUE);return 0;}
  case WM_DPICHANGED: {
    dpi_=HIWORD(wp);const auto* suggested=reinterpret_cast<RECT*>(lp);
    scroll_=0;SetWindowPos(window_,nullptr,suggested->left,suggested->top,suggested->right-suggested->left,suggested->bottom-suggested->top,SWP_NOZORDER);
    for(HWND child=GetWindow(window_,GW_CHILD);child;){HWND next=GetWindow(child,GW_HWNDNEXT);DestroyWindow(child);child=next;}
    if(font_)DeleteObject(font_);font_=CreateFontW(-MulDiv(13,dpi_,96),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    if(heading_font_)DeleteObject(heading_font_);heading_font_=CreateFontW(-MulDiv(25,dpi_,96),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    populate();refresh();refresh_google(true);return 0;
  }
  case WM_ERASEBKGND: {RECT r{};GetClientRect(window_,&r);if(background_)FillRect(reinterpret_cast<HDC>(wp),&r,background_);return 1;}
  case WM_CTLCOLORSTATIC: case WM_CTLCOLORBTN: case WM_CTLCOLORLISTBOX: case WM_CTLCOLOREDIT:{
    bool card=std::any_of(page_controls_.begin(),page_controls_.end(),[&](auto& item){return item.window==reinterpret_cast<HWND>(lp);});
    if(msg==WM_CTLCOLORLISTBOX||msg==WM_CTLCOLOREDIT)card=true;
    SetTextColor(reinterpret_cast<HDC>(wp),RGB(225,232,241));SetBkColor(reinterpret_cast<HDC>(wp),card?RGB(25,30,39):RGB(16,20,27));return reinterpret_cast<LRESULT>(card?surface_:background_);
  }
  case WM_MEASUREITEM:{auto* item=reinterpret_cast<MEASUREITEMSTRUCT*>(lp);if(item->CtlType==ODT_COMBOBOX){item->itemHeight=MulDiv(26,dpi_,96);return TRUE;}break;}
  case WM_NOTIFY:{auto* draw=reinterpret_cast<NMCUSTOMDRAW*>(lp);int id=static_cast<int>(draw->hdr.idFrom);
    if(draw->hdr.code!=NM_CUSTOMDRAW||id<Gap||id>Shadow)break;
    if(draw->dwDrawStage==CDDS_PREPAINT)return CDRF_NOTIFYITEMDRAW;
    if(draw->dwDrawStage!=CDDS_ITEMPREPAINT)break;
    auto t=theme_for(config_->theme_index,config_->custom_accent);auto accent=RGB((t.accent>>16)&255,(t.accent>>8)&255,t.accent&255);
    auto rounded=[&](RECT r,COLORREF color){auto b=CreateSolidBrush(color);auto old=SelectObject(draw->hdc,b);auto pen=SelectObject(draw->hdc,GetStockObject(NULL_PEN));RoundRect(draw->hdc,r.left,r.top,r.right,r.bottom,MulDiv(8,dpi_,96),MulDiv(8,dpi_,96));SelectObject(draw->hdc,old);SelectObject(draw->hdc,pen);DeleteObject(b);};
    if(draw->dwItemSpec==TBCD_CHANNEL){RECT r=draw->rc;int middle=(r.top+r.bottom)/2;r.top=middle-MulDiv(2,dpi_,96);r.bottom=middle+MulDiv(2,dpi_,96);rounded(r,RGB(65,75,91));int low=static_cast<int>(SendMessageW(draw->hdr.hwndFrom,TBM_GETRANGEMIN,0,0)),high=static_cast<int>(SendMessageW(draw->hdr.hwndFrom,TBM_GETRANGEMAX,0,0)),value=static_cast<int>(SendMessageW(draw->hdr.hwndFrom,TBM_GETPOS,0,0));r.right=r.left+MulDiv(r.right-r.left,value-low,std::max(1,high-low));if(r.right>r.left)rounded(r,accent);return CDRF_SKIPDEFAULT;}
    if(draw->dwItemSpec==TBCD_THUMB){rounded(draw->rc,accent);return CDRF_SKIPDEFAULT;}
    return CDRF_DODEFAULT;
  }
  case WM_DRAWITEM: {
    auto* d=reinterpret_cast<DRAWITEMSTRUCT*>(lp);
    if(d->CtlType==ODT_COMBOBOX){FillRect(d->hDC,&d->rcItem,surface_);if(d->itemID!=static_cast<UINT>(-1)){wchar_t label[256]{};SendMessageW(d->hwndItem,CB_GETLBTEXT,d->itemID,reinterpret_cast<LPARAM>(label));RECT r=d->rcItem;r.left+=MulDiv(10,dpi_,96);SetBkMode(d->hDC,TRANSPARENT);SetTextColor(d->hDC,RGB(225,232,241));auto old=SelectObject(d->hDC,font_);DrawTextW(d->hDC,label,-1,&r,DT_LEFT|DT_VCENTER|DT_SINGLELINE);SelectObject(d->hDC,old);if(d->itemState&ODS_FOCUS)DrawFocusRect(d->hDC,&d->rcItem);}return TRUE;}
    int i=static_cast<int>(d->CtlID)-ThemeBase;
    if(i<0||i>=6){
      auto t=theme_for(config_->theme_index,config_->custom_accent);auto rgb=[](unsigned color){return RGB((color>>16)&255,(color>>8)&255,color&255);};
      bool selected=d->CtlID>=280&&d->CtlID<=285&&static_cast<int>(d->CtlID)-280==page_;
      bool primary=d->CtlID==Save;COLORREF fill=primary?rgb(t.accent):selected?RGB(36,47,62):RGB(32,38,49);
      if(d->itemState&ODS_SELECTED)fill=RGB(48,57,72);
      bool focused=(d->itemState&ODS_FOCUS)&&!(d->itemState&ODS_NOFOCUSRECT);
      auto brush=CreateSolidBrush(fill);auto pen=CreatePen(PS_SOLID,focused?2:1,selected||focused?rgb(t.accent):RGB(49,58,71));auto oldbrush=SelectObject(d->hDC,brush);auto oldpen=SelectObject(d->hDC,pen);
      RoundRect(d->hDC,d->rcItem.left,d->rcItem.top,d->rcItem.right,d->rcItem.bottom,MulDiv(12,dpi_,96),MulDiv(12,dpi_,96));SelectObject(d->hDC,oldbrush);SelectObject(d->hDC,oldpen);DeleteObject(brush);DeleteObject(pen);
      unsigned luminance=((t.accent>>16)&255)*299+((t.accent>>8)&255)*587+(t.accent&255)*114;
      wchar_t label[256]{};GetWindowTextW(d->hwndItem,label,256);RECT r=d->rcItem;r.left+=MulDiv(12,dpi_,96);SetBkMode(d->hDC,TRANSPARENT);SetTextColor(d->hDC,d->itemState&ODS_DISABLED?RGB(107,119,139):primary?(luminance>150000?RGB(12,19,28):RGB(255,255,255)):selected?rgb(t.accent):RGB(225,232,241));auto oldfont=SelectObject(d->hDC,font_);DrawTextW(d->hDC,label,-1,&r,DT_VCENTER|DT_SINGLELINE|(d->CtlID>=280&&d->CtlID<=285?DT_LEFT:DT_CENTER));SelectObject(d->hDC,oldfont);
      return TRUE;
    }
    auto t=themes[i];auto rgb=[](unsigned c){return RGB((c>>16)&255,(c>>8)&255,c&255);};
    auto brush=CreateSolidBrush(rgb(t.background));FillRect(d->hDC,&d->rcItem,brush);DeleteObject(brush);
    auto pen=CreatePen(PS_SOLID,config_->theme_index==i?2:1,rgb(config_->theme_index==i?t.accent:t.border));auto oldpen=SelectObject(d->hDC,pen);auto oldbrush=SelectObject(d->hDC,GetStockObject(NULL_BRUSH));
    RoundRect(d->hDC,d->rcItem.left+1,d->rcItem.top+1,d->rcItem.right-1,d->rcItem.bottom-1,12,12);SelectObject(d->hDC,oldpen);SelectObject(d->hDC,oldbrush);DeleteObject(pen);
    RECT r=d->rcItem;r.top+=MulDiv(25,dpi_,96);SetBkMode(d->hDC,TRANSPARENT);SetTextColor(d->hDC,rgb(t.text));DrawTextW(d->hDC,t.name,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    brush=CreateSolidBrush(rgb(t.accent));RECT dot{(d->rcItem.left+d->rcItem.right)/2-14,d->rcItem.top+8,(d->rcItem.left+d->rcItem.right)/2+14,d->rcItem.top+18};FillRect(d->hDC,&dot,brush);DeleteObject(brush);return TRUE;
  }
  case WM_HSCROLL: {
    int id=GetDlgCtrlID(reinterpret_cast<HWND>(lp));int value=static_cast<int>(SendMessageW(reinterpret_cast<HWND>(lp),TBM_GETPOS,0,0));
    if(id==Gap)config_->top_gap=value;if(id==Opacity)config_->opacity_percent=value;if(id==Speed)config_->animation_duration_ms=value;if(id==Width)config_->expanded_width=value;
    if(id==Shadow)config_->shadow_percent=value;
    if(id>=Gap&&id<=Shadow){auto label=std::to_wstring(value)+(id==Opacity||id==Shadow?L"%":id==Speed?L" ms":L"");SetDlgItemTextW(window_,290+id-Gap,label.c_str());}
    changed_();return 0;
  }
  case WM_COMMAND: {
    int id=LOWORD(wp);
    if(id==IDOK)id=Save;if(id==IDCANCEL){close();return 0;}
    if(id>=280&&id<=285){if(HIWORD(wp)==BN_CLICKED){for(auto& item:page_controls_)SendMessageW(item.window,CB_SHOWDROPDOWN,FALSE,0);select_page(id-280);SetFocus(GetDlgItem(window_,id));}return 0;}
    if(id==301||id==302||id==305){if(HIWORD(wp)!=BN_CLICKED||!google_)return 0;if(id==301)google_->enable_tasks();if(id==305)google_->refresh();if(id==302){wchar_t path[32768]{};OPENFILENAMEW dialog{sizeof(dialog)};dialog.hwndOwner=window_;dialog.lpstrFilter=L"Google OAuth Desktop JSON\0*.json\0";dialog.lpstrFile=path;dialog.nMaxFile=32768;dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;if(GetOpenFileNameW(&dialog))google_->connect(path,true);}return 0;}
    if(id==304){if(HIWORD(wp)==CBN_SELCHANGE&&google_){auto selection=SendDlgItemMessageW(window_,304,CB_GETCURSEL,0,0);if(selection>=0&&static_cast<size_t>(selection)<task_lists_.size())google_->select_task_list(task_lists_[selection].id);}return 0;}
    if(id==297){if(HIWORD(wp)==BN_CLICKED&&notifications_)notifications_->request_access();return 0;}
    if(id==298){if(HIWORD(wp)==BN_CLICKED)ShellExecuteW(window_,L"open",L"ms-settings:notifications",nullptr,nullptr,SW_SHOWNORMAL);return 0;}
    if(id==Calendars)return 0;
    if(id==Connect||id==Sync||id==Disconnect||id==ApplyCalendars){
      if(HIWORD(wp)!=BN_CLICKED||!google_)return 0;
      if(id==Connect){wchar_t path[32768]{};OPENFILENAMEW dialog{sizeof(dialog)};dialog.hwndOwner=window_;dialog.lpstrFilter=L"Google OAuth Desktop JSON\0*.json\0";dialog.lpstrFile=path;dialog.nMaxFile=32768;dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;dialog.lpstrTitle=L"Google Cloud Masaüstü OAuth JSON dosyasını seç";if(GetOpenFileNameW(&dialog))google_->connect(path);}
      if(id==Sync)google_->refresh();if(id==Disconnect)google_->disconnect();
      if(id==ApplyCalendars){std::vector<std::wstring> ids;for(size_t i=0;i<calendars_.size();++i)if(SendDlgItemMessageW(window_,Calendars,LB_GETSEL,i,0)>0)ids.push_back(calendars_[i].id);google_->select(std::move(ids));}
      refresh_google();return 0;
    }
    if(id==MorningHour){if(HIWORD(wp)==CBN_SELCHANGE){auto hour=SendDlgItemMessageW(window_,id,CB_GETCURSEL,0,0);if(hour>=0&&hour<24){config_->reminder_morning_hour=static_cast<int>(hour);changed_();}}return 0;}
    if(id==AnimationStyle||id==Monitor||id==262||id==300){
      // Opening/focusing/closing a combo is not a settings change. In particular,
      // CB_SETCURSEL in refresh() during CBN_DROPDOWN immediately closes its list.
      if(HIWORD(wp)!=CBN_SELCHANGE)return 0;
      LRESULT selection=SendDlgItemMessageW(window_,id,CB_GETCURSEL,0,0);
      LRESULT count=SendDlgItemMessageW(window_,id,CB_GETCOUNT,0,0);
      if(selection<0||selection>=count)return 0;
      if(id==AnimationStyle)config_->animation_style=static_cast<int>(selection);
      if(id==300)config_->island_shape=static_cast<int>(selection);
      if(id==Monitor)config_->monitor_index=static_cast<int>(selection);
      if(id==262){constexpr int values[]{0,5,15,30,60,120,1440};if(selection>=7)return 0;config_->clipboard_ttl_minutes=values[selection];}
      // Keep the native selection lifecycle intact; the control already displays it.
      changed_();return 0;
    }
    if(id==Hotkey||id==WinKey){WORD value=static_cast<WORD>(SendDlgItemMessageW(window_,Hotkey,HKM_GETHOTKEY,0,0));config_->hotkey_key=LOBYTE(value);config_->hotkey_modifiers=0;BYTE mods=HIBYTE(value);if(mods&HOTKEYF_CONTROL)config_->hotkey_modifiers|=MOD_CONTROL;if(mods&HOTKEYF_ALT)config_->hotkey_modifiers|=MOD_ALT;if(mods&HOTKEYF_SHIFT)config_->hotkey_modifiers|=MOD_SHIFT;if(SendDlgItemMessageW(window_,WinKey,BM_GETCHECK,0,0)==BST_CHECKED)config_->hotkey_modifiers|=MOD_WIN;if(config_->hotkey_key&&config_->hotkey_modifiers==0){config_->hotkey_modifiers=MOD_CONTROL|MOD_ALT;}changed_();return 0;}
    if(id==Left)config_->horizontal_percent=0;if(id==Center)config_->horizontal_percent=50;if(id==Right)config_->horizontal_percent=100;
    if(id>=ThemeBase&&id<ThemeBase+6){config_->theme_index=id-ThemeBase;config_->custom_accent=-1;config_->follow_system_theme=false;}
    if(id==Color){static COLORREF custom[16]{};auto t=theme_for(config_->theme_index,config_->custom_accent);CHOOSECOLORW color{sizeof(color)};color.hwndOwner=window_;color.lpCustColors=custom;color.Flags=CC_FULLOPEN|CC_RGBINIT;color.rgbResult=RGB((t.accent>>16)&255,(t.accent>>8)&255,t.accent&255);if(ChooseColorW(&color))config_->custom_accent=(GetRValue(color.rgbResult)<<16)|(GetGValue(color.rgbResult)<<8)|GetBValue(color.rgbResult);}
    auto checked=[&](int control){return SendDlgItemMessageW(window_,control,BM_GETCHECK,0,0)==BST_CHECKED;};
    if(id==MorningEnabled)config_->reminder_notifications=checked(MorningEnabled);
    if(id==295)config_->windows_notifications=checked(295);
    if(id==296)config_->windows_notification_content=checked(296);
    if(id==260)config_->smart_notifications=checked(260);if(id==261)config_->reduced_motion=checked(261);if(id==263)config_->expire_pinned_clips=checked(263);
    if(id==Startup){bool enabled=checked(Startup);if(set_startup(enabled))config_->start_with_windows=enabled;else MessageBoxW(window_,L"Windows başlangıç kaydı güncellenemedi. Önceki ayar korundu.",L"NativeIsland",MB_OK|MB_ICONERROR);}
    if(id==PinClick)config_->pin_on_click=checked(PinClick);
    if(id==Follow)config_->follow_system_theme=checked(Follow);if(id==Hover)config_->hover_expand=checked(Hover);if(id==Collapse)config_->auto_collapse=checked(Collapse);if(id==Remember)config_->remember_page=checked(Remember);
    if(id==Reset){bool clipboard=config_->clipboard_history,startup=config_->start_with_windows;*config_=Config{};config_->clipboard_history=clipboard;config_->start_with_windows=startup;}
    changed_();refresh();if(id==Save){close();changed_();}return 0;
  }
  case WM_CLOSE:close();changed_();return 0;
  case WM_DESTROY:KillTimer(window_,1);window_=nullptr;page_controls_.clear();if(font_){DeleteObject(font_);font_=nullptr;}if(heading_font_){DeleteObject(heading_font_);heading_font_=nullptr;}if(surface_){DeleteObject(surface_);surface_=nullptr;}if(background_){DeleteObject(background_);background_=nullptr;}return 0;
  }
  return DefWindowProcW(window_,msg,wp,lp);
}
}
