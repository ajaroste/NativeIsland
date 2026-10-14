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
    open_form(instance,owner,{}, {},[add=std::move(add)](std::wstring title){if(add)add(std::move(title));return true;});
  }
  void open_edit(HINSTANCE instance,HWND owner,std::wstring key,std::wstring title,std::function<bool(std::wstring)> save){open_form(instance,owner,std::move(key),std::move(title),std::move(save));}
  const std::wstring& task_key()const{return task_key_;}
private:
  void open_form(HINSTANCE instance,HWND owner,std::wstring key,std::wstring title,std::function<bool(std::wstring)> save){
    if(window_){SetForegroundWindow(window_);return;}task_key_=std::move(key);original_=std::move(title);save_=std::move(save);submitting_=false;WNDCLASSW c{};c.hInstance=instance;c.lpfnWndProc=proc;c.lpszClassName=L"NativeIslandTaskEntry";c.hCursor=LoadCursorW(nullptr,IDC_ARROW);c.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);RegisterClassW(&c);
    RECT r{};GetWindowRect(owner,&r);UINT dpi=GetDpiForWindow(owner);auto s=[&](int x){return MulDiv(x,dpi,96);};window_=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST,c.lpszClassName,task_key_.empty()?L"Yeni görev":L"Görevi düzenle",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,r.left+s(20),r.top+s(70),s(440),s(155),owner,nullptr,instance,this);if(!window_)return;
    int corners=2;DwmSetWindowAttribute(window_,33,&corners,sizeof(corners));
    HWND edit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,s(16),s(20),s(390),s(30),window_,reinterpret_cast<HMENU>(static_cast<INT_PTR>(title_id)),instance,nullptr);SendMessageW(edit,EM_SETLIMITTEXT,1024,0);SetWindowTextW(edit,original_.c_str());
    HWND button=CreateWindowW(L"BUTTON",task_key_.empty()?L"Ekle":L"Kaydet",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,s(306),s(65),s(100),s(30),window_,reinterpret_cast<HMENU>(static_cast<INT_PTR>(add_id)),instance,nullptr);
    CreateWindowW(L"STATIC",L"",WS_CHILD|WS_VISIBLE,s(16),s(66),s(280),s(34),window_,reinterpret_cast<HMENU>(1003),instance,nullptr);
    for(auto child:{edit,button})SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);ShowWindow(window_,SW_SHOW);SetForegroundWindow(window_);SetFocus(edit);
  }
  static LRESULT CALLBACK proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){auto* self=reinterpret_cast<TaskEntryWindow*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));if(msg==WM_NCCREATE){self=static_cast<TaskEntryWindow*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));self->window_=hwnd;}
    if(self&&msg==WM_COMMAND){if(LOWORD(wp)==IDCANCEL){self->close();return 0;}if(LOWORD(wp)==add_id&&HIWORD(wp)==BN_CLICKED&&reinterpret_cast<HWND>(lp)==GetDlgItem(hwnd,add_id)){
      if(self->submitting_)return 0;wchar_t raw[1025]{};GetDlgItemTextW(hwnd,title_id,raw,1025);std::wstring title=raw;auto first=title.find_first_not_of(L" \t\r\n");if(first==std::wstring::npos){SetFocus(GetDlgItem(hwnd,title_id));return 0;}title=title.substr(first,title.find_last_not_of(L" \t\r\n")-first+1);
      if(!self->task_key_.empty()&&title==self->original_){self->close();return 0;}
      self->submitting_=true;EnableWindow(GetDlgItem(hwnd,add_id),FALSE);auto save=self->save_;bool accepted=false;try{accepted=save&&save(std::move(title));}catch(...){accepted=false;}
      if(accepted)self->close();else{self->submitting_=false;EnableWindow(GetDlgItem(hwnd,add_id),TRUE);SetDlgItemTextW(hwnd,1003,L"Şu anda kaydedilemiyor. Tekrar deneyin.");}return 0;
    }}
    if(self&&msg==WM_DESTROY){self->window_=nullptr;self->submitting_=false;self->task_key_.clear();self->original_.clear();self->save_={};}return DefWindowProcW(hwnd,msg,wp,lp);
  }
  HWND window_{};std::wstring task_key_,original_;bool submitting_{};std::function<bool(std::wstring)> save_;
};
}
