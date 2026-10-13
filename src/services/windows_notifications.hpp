#pragma once
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Notifications.Management.h>
#include <winrt/Windows.UI.Notifications.h>
#include <map>
#include <deque>
#include <string>
#include <vector>

namespace ni {
struct WindowsNotice {uint32_t id{};int64_t created{};std::wstring app,title,body;};
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
  bool contains(uint32_t id)const{return delta_.contains(id);}
  const std::wstring& status()const{return status_;}
private:
  void clear();
  bool enabled_{};
  std::wstring status_{L"Kapalı · Bildirim içeriği kaydedilmez."};
  NotificationDelta delta_;
  std::vector<WindowsNotice> incoming_;
  winrt::Windows::UI::Notifications::Management::UserNotificationListener listener_{nullptr};
  winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::UI::Notifications::Management::UserNotificationListenerAccessStatus> access_{nullptr};
  winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Foundation::Collections::IVectorView<winrt::Windows::UI::Notifications::UserNotification>> query_{nullptr};
};
}
