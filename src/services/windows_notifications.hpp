#pragma once
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Notifications.Management.h>
#include <winrt/Windows.UI.Notifications.h>
#include <map>
#include <deque>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <set>
#include <cstdint>
#include <chrono>

namespace ni {
struct WindowsNotice {uint32_t id{};int64_t created{};std::wstring app,title,body,app_id;};
struct NoticeLogoCache {
  mutable std::mutex mutex;
  std::map<std::wstring,std::shared_ptr<const std::vector<std::uint8_t>>> ready;
  std::set<std::wstring> pending;
  std::map<std::wstring,std::chrono::steady_clock::time_point> failed_at;
};
// Source is the Windows-reported application identity, never an inferred web service.
// Text is display-only: the notification listener cannot promise reply/open actions.
struct WindowsNoticePresentation {std::wstring source,title,preview;};
WindowsNoticePresentation windows_notice_presentation(const WindowsNotice& notice,bool show_content);
// No notification content is persisted. Initial history is only a baseline.
class NotificationDelta {
public:
  std::vector<WindowsNotice> update(const std::vector<WindowsNotice>& current);
  bool contains(uint32_t id)const{return seen_.contains(id);}
  void reset(){seen_.clear();baseline_=false;}
private:
  std::map<uint32_t,int64_t> seen_;bool baseline_{};
};
std::wstring windows_notice_text(const WindowsNotice& notice,bool content);
class WindowsNotifications {
public:
  void enable(bool enabled);
  void request_access(); // Must be called on the UI thread from explicit user action.
  void poll(); // Nonblocking async operation state machine, also on the UI thread.
  std::vector<WindowsNotice> take();
  std::shared_ptr<const std::vector<std::uint8_t>> logo_for(const std::wstring& app_id) const;
  bool contains(uint32_t id)const{return delta_.contains(id);}
  const std::wstring& status()const{return status_;}
private:
  void clear();
  bool enabled_{};
  std::wstring status_{L"Kapalı · Bildirim içeriği kaydedilmez."};
  NotificationDelta delta_;
  std::shared_ptr<NoticeLogoCache> logo_cache_{std::make_shared<NoticeLogoCache>()};
  std::vector<WindowsNotice> incoming_;
  winrt::Windows::UI::Notifications::Management::UserNotificationListener listener_{nullptr};
  winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::UI::Notifications::Management::UserNotificationListenerAccessStatus> access_{nullptr};
  winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Foundation::Collections::IVectorView<winrt::Windows::UI::Notifications::UserNotification>> query_{nullptr};
};
}
