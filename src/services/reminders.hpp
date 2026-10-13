#pragma once
#include <windows.h>
#include <filesystem>
#include <string>
#include <vector>
#include <mutex>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>

namespace ni {
struct Reminder {
  std::wstring id,title,calendar;
  int64_t start{},end{}; // UTC Unix seconds; all-day boundaries are local midnight.
  bool all_day{},done{},google{};
};
struct ReminderNotice {std::wstring title,body;bool summary{};};
int64_t reminder_now();
int64_t reminder_utc(const SYSTEMTIME& local);
SYSTEMTIME reminder_local(int64_t utc);
std::wstring reminder_day(int64_t utc);
std::wstring reminder_time(const Reminder& reminder);
std::pair<int64_t,int64_t> reminder_day_bounds(int64_t now);
std::vector<Reminder> reminders_today(const std::vector<Reminder>& entries,int64_t now);
std::vector<Reminder> reminders_pending_today(const std::vector<Reminder>& entries,int64_t now);
int64_t reminder_day_at_offset(int64_t now,int offset);
std::vector<Reminder> reminders_for_day(const std::vector<Reminder>& entries,int64_t day,bool include_done=false);
int64_t parse_calendar_time(const std::wstring& value,bool date_only=false);
std::wstring calendar_utc(int64_t seconds);
std::vector<Reminder> parse_calendar_events(const winrt::Windows::Data::Json::JsonObject& root,const std::wstring& calendar);
bool atomic_user_file(const std::filesystem::path& path,const std::string& contents,bool backup=true);
class ReminderStore {
public:
  explicit ReminderStore(std::filesystem::path path={});
  bool load();
  bool add(Reminder value);
  bool remove(const std::wstring& id);
  bool complete(const std::wstring& id);
  bool replace_google(std::vector<Reminder> events);
  std::vector<Reminder> snapshot() const;
  std::vector<ReminderNotice> tick(int64_t now,bool enabled,int morning_hour);
  std::wstring error() const;
private:
  bool save_locked();
  std::filesystem::path path_;
  mutable std::mutex mutex_;
  std::vector<Reminder> entries_;
  std::wstring summary_day_,error_;
  std::vector<std::wstring> delivered_;
  bool writable_{true},recovered_{};
};
}
