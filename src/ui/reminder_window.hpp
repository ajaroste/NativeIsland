#pragma once
#include "services/reminders.hpp"
#include <functional>
namespace ni {
class ReminderWindow {
public:
  ~ReminderWindow(){close();}
  void open(HINSTANCE instance,HWND owner,ReminderStore& store,std::function<void()> changed,int64_t selected_day=0);
  void close(){if(window_)DestroyWindow(window_);}
  bool visible()const{return window_&&IsWindowVisible(window_);}
  bool translate(MSG& message){return window_&&IsDialogMessageW(window_,&message);}
private:
  static LRESULT CALLBACK proc(HWND,UINT,WPARAM,LPARAM);
  LRESULT handle(UINT,WPARAM,LPARAM);
  void populate();
  HWND window_{};HINSTANCE instance_{};HFONT font_{};UINT dpi_{96};
  ReminderStore* store_{};
  std::function<void()> changed_;
};
}
