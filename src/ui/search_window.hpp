#pragma once
#include <windows.h>
#include <string>
#include <functional>
namespace ni {
class SearchWindow {
public:
  ~SearchWindow(){close();}
  bool visible()const{return window_&&IsWindowVisible(window_);}
  void close(){if(window_)DestroyWindow(window_);}
  void open(HINSTANCE instance,HWND owner,const std::wstring& query,std::function<void(std::wstring)> changed){
    changed_=std::move(changed);if(window_){SetForegroundWindow(window_);return;}
    WNDCLASSW wc{};wc.hInstance=instance;wc.lpfnWndProc=proc;wc.lpszClassName=L"NativeIslandPanoSearch";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);RegisterClassW(&wc);
    window_=CreateWindowExW(WS_EX_TOOLWINDOW,wc.lpszClassName,L"Pano içinde ara",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,CW_USEDEFAULT,CW_USEDEFAULT,440,140,owner,nullptr,instance,this);
    HWND edit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",query.c_str(),WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,16,18,300,30,window_,reinterpret_cast<HMENU>(1),instance,nullptr);SendMessageW(edit,EM_SETLIMITTEXT,256,0);
    HWND clear=CreateWindowW(L"BUTTON",L"Temizle",WS_CHILD|WS_VISIBLE|WS_TABSTOP,325,18,85,30,window_,reinterpret_cast<HMENU>(2),instance,nullptr);
    for(HWND child:{edit,clear})SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);
    ShowWindow(window_,SW_SHOW);SetForegroundWindow(window_);SetFocus(edit);SendMessageW(edit,EM_SETSEL,0,-1);
  }
private:
  static LRESULT CALLBACK proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){auto* self=reinterpret_cast<SearchWindow*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));if(msg==WM_NCCREATE){self=static_cast<SearchWindow*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);self->window_=hwnd;SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
    if(self&&msg==WM_COMMAND){if(LOWORD(wp)==2)SetDlgItemTextW(hwnd,1,L"");if(LOWORD(wp)==1&&HIWORD(wp)==EN_CHANGE){wchar_t query[257]{};GetDlgItemTextW(hwnd,1,query,257);if(self->changed_)self->changed_(query);}return 0;}
    if(self&&msg==WM_DESTROY)self->window_=nullptr;return DefWindowProcW(hwnd,msg,wp,lp);}
  HWND window_{};std::function<void(std::wstring)> changed_;
};
}
