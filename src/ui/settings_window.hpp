#pragma once
#include "core/config.hpp"
#include "services/google_calendar.hpp"
#include "services/windows_notifications.hpp"
#include <windows.h>
#include <functional>
namespace ni {
class SettingsWindow {
public:
  void open(HINSTANCE instance,HWND owner,Config& config,std::function<void()> changed,GoogleCalendarService* google=nullptr,WindowsNotifications* notifications=nullptr);
  void close();
  bool capture_preview(const std::filesystem::path& path);
  bool visible()const{return window_&&IsWindowVisible(window_);}
  bool translate(MSG& message){return window_&&IsDialogMessageW(window_,&message);}
private:
  static LRESULT CALLBACK proc(HWND,UINT,WPARAM,LPARAM);
  LRESULT handle(UINT,WPARAM,LPARAM);
  void populate();
  void refresh();
  void refresh_google(bool force=false);
  void select_page(int page);
  void paint(HDC dc);
  struct PageControl{HWND window;int page;};
  std::vector<PageControl> page_controls_;
  int page_{};
  GoogleCalendarService* google_{};
  WindowsNotifications* notifications_{};
  std::vector<GoogleCalendarInfo> calendars_;
  std::wstring calendar_signature_;
  std::wstring task_signature_;
  std::vector<GoogleTaskList> task_lists_;
  HWND window_{};Config* config_{};std::function<void()> changed_;HFONT font_{},heading_font_{};HBRUSH background_{},surface_{};UINT dpi_{96};
  int scroll_{};
};
}
