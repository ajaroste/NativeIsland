#pragma once
#include <windows.h>
#include <dwmapi.h>
#include <functional>
#include <string>
namespace ni {
class TaskEntryWindow {
public:
  static constexpr int title_id=1001,add_id=1002;
  ~TaskEntryWindow(){close();}
  bool visible()const{return window_&&IsWindowVisible(window_);}
  bool translate(MSG& msg){if(!window_)return false;if(msg.message==WM_KEYDOWN&&msg.wParam==VK_RETURN&&msg.hwnd==GetDlgItem(window_,title_id))return true;return IsDialogMessageW(window_,&msg);}
  void close(){if(window_)DestroyWindow(window_);}
  void open(HINSTANCE instance,HWND owner,std::function<void(std::wstring)> add){
    if(window_){SetForegroundWindow(window_);return;}add_=std::move(add);WNDCLASSW c{};c.hInstance=instance;c.lpfnWndProc=proc;c.lpszClassName=L"NativeIslandTaskEntry";c.hCursor=LoadCursorW(nullptr,IDC_ARROW);c.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);RegisterClassW(&c);
    RECT r{};GetWindowRect(owner,&r);UINT dpi=GetDpiForWindow(owner);auto s=[&](int x){return MulDiv(x,dpi,96);};window_=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST,c.lpszClassName,L"Yeni görev",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,r.left+s(20),r.top+s(70),s(440),s(155),owner,nullptr,instance,this);if(!window_)return;
    int corners=2;DwmSetWindowAttribute(window_,33,&corners,sizeof(corners));
    HWND edit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,s(16),s(20),s(390),s(30),window_,reinterpret_cast<HMENU>(static_cast<INT_PTR>(title_id)),instance,nullptr);SendMessageW(edit,EM_SETLIMITTEXT,1024,0);
    HWND button=CreateWindowW(L"BUTTON",L"Ekle",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,s(306),s(65),s(100),s(30),window_,reinterpret_cast<HMENU>(static_cast<INT_PTR>(add_id)),instance,nullptr);
    for(auto child:{edit,button})SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);ShowWindow(window_,SW_SHOW);SetForegroundWindow(window_);SetFocus(edit);
  }
private:
  static LRESULT CALLBACK proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){auto* self=reinterpret_cast<TaskEntryWindow*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));if(msg==WM_NCCREATE){self=static_cast<TaskEntryWindow*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));self->window_=hwnd;}
    if(self&&msg==WM_COMMAND){if(LOWORD(wp)==IDCANCEL){self->close();return 0;}if(LOWORD(wp)==add_id&&HIWORD(wp)==BN_CLICKED&&reinterpret_cast<HWND>(lp)==GetDlgItem(hwnd,add_id)){wchar_t raw[1025]{};GetDlgItemTextW(hwnd,title_id,raw,1025);std::wstring title=raw;auto first=title.find_first_not_of(L" \t\r\n");if(first==std::wstring::npos){SetFocus(GetDlgItem(hwnd,title_id));return 0;}title=title.substr(first,title.find_last_not_of(L" \t\r\n")-first+1);auto add=self->add_;self->close();if(add)add(std::move(title));return 0;}}
    if(self&&msg==WM_DESTROY)self->window_=nullptr;return DefWindowProcW(hwnd,msg,wp,lp);
  }
  HWND window_{};std::function<void(std::wstring)> add_;
};
}
