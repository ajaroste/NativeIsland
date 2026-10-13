#include "ui/reminder_window.hpp"
#include <commctrl.h>
#include <algorithm>
namespace ni {
namespace {enum{Title=301,Date=302,Time=303,AllDay=304,Add=305,Status=306};}
void ReminderWindow::open(HINSTANCE instance,HWND owner,ReminderStore& store,std::function<void()> changed,int64_t selected_day){
  if(window_){ShowWindow(window_,SW_SHOWNORMAL);SetForegroundWindow(window_);return;}
  instance_=instance;store_=&store;changed_=std::move(changed);
  INITCOMMONCONTROLSEX cc{sizeof(cc),ICC_DATE_CLASSES};InitCommonControlsEx(&cc);
  WNDCLASSW wc{};wc.hInstance=instance;wc.lpfnWndProc=proc;wc.lpszClassName=L"NativeIslandReminders";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);RegisterClassW(&wc);
  window_=CreateWindowExW(WS_EX_TOOLWINDOW,wc.lpszClassName,L"Hatırlatıcı ekle",WS_POPUP|WS_CAPTION|WS_SYSMENU,0,0,420,210,owner,nullptr,instance,this);
  if(!window_)return;dpi_=GetDpiForWindow(window_);font_=CreateFontW(-MulDiv(13,dpi_,96),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
  RECT rect{0,0,MulDiv(400,dpi_,96),MulDiv(176,dpi_,96)};AdjustWindowRectExForDpi(&rect,GetWindowLongW(window_,GWL_STYLE),FALSE,WS_EX_TOOLWINDOW,dpi_);
  MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(owner,MONITOR_DEFAULTTONEAREST),&monitor);RECT island{};GetWindowRect(owner,&island);
  int width=rect.right-rect.left,height=rect.bottom-rect.top;
  int x=std::clamp(static_cast<int>((island.left+island.right-width)/2),static_cast<int>(monitor.rcWork.left),std::max(static_cast<int>(monitor.rcWork.left),static_cast<int>(monitor.rcWork.right)-width));
  int y=std::clamp(static_cast<int>(island.top)+MulDiv(70,dpi_,96),static_cast<int>(monitor.rcWork.top),std::max(static_cast<int>(monitor.rcWork.top),static_cast<int>(monitor.rcWork.bottom)-height));
  SetWindowPos(window_,nullptr,x,y,width,height,SWP_NOZORDER);populate();if(selected_day){auto date=reminder_local(selected_day);DateTime_SetSystemtime(GetDlgItem(window_,Date),GDT_VALID,&date);}ShowWindow(window_,SW_SHOW);SetForegroundWindow(window_);SetFocus(GetDlgItem(window_,Title));
}
void ReminderWindow::populate(){
  auto c=[&](const wchar_t* cls,const wchar_t* title,DWORD style,int id,int x,int y,int w,int h){HWND hwnd=CreateWindowExW(0,cls,title,WS_CHILD|WS_VISIBLE|style,MulDiv(x,dpi_,96),MulDiv(y,dpi_,96),MulDiv(w,dpi_,96),MulDiv(h,dpi_,96),window_,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance_,nullptr);SendMessageW(hwnd,WM_SETFONT,reinterpret_cast<WPARAM>(font_),TRUE);return hwnd;};
  HWND title=c(L"EDIT",L"",WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,Title,16,16,368,30);SendMessageW(title,EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(L"Ne hatırlatalım?"));SendMessageW(title,EM_SETLIMITTEXT,2048,0);
  c(DATETIMEPICK_CLASSW,L"",DTS_SHORTDATEFORMAT|WS_TABSTOP,Date,16,58,154,28);c(DATETIMEPICK_CLASSW,L"",DTS_TIMEFORMAT|WS_TABSTOP,Time,180,58,100,28);
  c(L"BUTTON",L"Tüm gün",BS_AUTOCHECKBOX|WS_TABSTOP,AllDay,290,58,94,28);
  c(L"STATIC",L"",0,Status,16,94,368,30);
  c(L"BUTTON",L"İptal",BS_PUSHBUTTON|WS_TABSTOP,IDCANCEL,174,134,100,28);c(L"BUTTON",L"Ekle",BS_DEFPUSHBUTTON|WS_TABSTOP,Add,284,134,100,28);
}
LRESULT CALLBACK ReminderWindow::proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){auto* self=reinterpret_cast<ReminderWindow*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));if(msg==WM_NCCREATE){self=static_cast<ReminderWindow*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);self->window_=hwnd;SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}return self?self->handle(msg,wp,lp):DefWindowProcW(hwnd,msg,wp,lp);}
LRESULT ReminderWindow::handle(UINT msg,WPARAM wp,LPARAM lp){switch(msg){
  case WM_COMMAND:{int id=LOWORD(wp);if(HIWORD(wp)!=BN_CLICKED)return 0;
    if(id==IDCANCEL){close();return 0;}
    if(id==AllDay){EnableWindow(GetDlgItem(window_,Time),SendDlgItemMessageW(window_,AllDay,BM_GETCHECK,0,0)!=BST_CHECKED);return 0;}
    if(id==Add||id==IDOK){wchar_t title[2049]{};GetDlgItemTextW(window_,Title,title,2049);
      if(std::wstring(title).find_first_not_of(L" \t\r\n")==std::wstring::npos){SetDlgItemTextW(window_,Status,L"Bir hatırlatıcı başlığı yazın.");SetFocus(GetDlgItem(window_,Title));return 0;}
      SYSTEMTIME date{},time{};if(DateTime_GetSystemtime(GetDlgItem(window_,Date),&date)!=GDT_VALID||DateTime_GetSystemtime(GetDlgItem(window_,Time),&time)!=GDT_VALID){SetDlgItemTextW(window_,Status,L"Geçerli bir tarih ve saat seçin.");return 0;}
      bool all=SendDlgItemMessageW(window_,AllDay,BM_GETCHECK,0,0)==BST_CHECKED;date.wHour=all?0:time.wHour;date.wMinute=all?0:time.wMinute;date.wSecond=date.wMilliseconds=0;
      Reminder r;r.title=title;r.start=reminder_utc(date);r.all_day=all;r.end=all?reminder_day_bounds(r.start).second:r.start+3600;
      if(store_->add(std::move(r))){close();if(changed_)changed_();}else SetDlgItemTextW(window_,Status,L"Kaydedilemedi; tarih ve veri klasörü izinlerini kontrol edin.");return 0;
    }return 0;
  }
  case WM_CLOSE:close();return 0;
  case WM_DESTROY:window_=nullptr;if(font_){DeleteObject(font_);font_=nullptr;}return 0;
}return DefWindowProcW(window_,msg,wp,lp);}
}
