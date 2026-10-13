#pragma once
#include "services/reminders.hpp"
#include <thread>
#include <condition_variable>
#include <functional>
namespace ni {
struct GoogleCalendarInfo {std::wstring id,name;bool selected{};};
struct GoogleTask {std::wstring id,title,notes,due;bool completed{};};
struct GoogleTaskList {std::wstring id,name;};
enum class TaskOperation {add,complete,remove};
struct TaskRequest {std::wstring method,path;std::string body;};
TaskRequest google_task_request(TaskOperation operation,const std::wstring& list,const std::wstring& id={},const std::wstring& title={},bool completed=true);
inline std::vector<size_t> pending_task_indices(const std::vector<GoogleTask>& tasks){std::vector<size_t> indices;for(size_t i=0;i<tasks.size();++i)if(!tasks[i].completed)indices.push_back(i);return indices;}
std::vector<GoogleTask> parse_google_tasks(const winrt::Windows::Data::Json::JsonObject& root);
struct GoogleCalendarStatus {
  bool busy{},connected{};
  int64_t last_sync{};
  std::wstring message{L"Google Takvim bağlı değil"};
  std::vector<GoogleCalendarInfo> calendars;
  bool tasks_enabled{};
  std::wstring tasks_message{L"Google Tasks bağlı değil · Ayarlardan bağla"},task_list_id;
  std::vector<GoogleTaskList> task_lists;
  std::vector<GoogleTask> tasks;
};
inline bool tasks_connected(const GoogleCalendarStatus& status){return status.connected&&status.tasks_enabled;}
std::string oauth_encode(const std::string& value);
std::string oauth_decode(const std::string& value);
std::string oauth_query(const std::string& query,const std::string& key);
std::string oauth_challenge(const std::string& verifier);
class GoogleCalendarService {
public:
  ~GoogleCalendarService(){stop();}
  void start(ReminderStore& store,std::function<void()> changed,bool enabled=true);
  void stop();
  void connect(std::filesystem::path client_file,bool tasks=false);
  void enable_tasks();
  void select_task_list(std::wstring id);
  void add_task(std::wstring list,std::wstring title);
  void complete_task(std::wstring list,std::wstring id,bool completed);
  void delete_task(std::wstring list,std::wstring id);
  void disconnect();
  void refresh();
  void select(std::vector<std::wstring> ids);
  GoogleCalendarStatus snapshot()const;
private:
  void run(std::stop_token stop);
  mutable std::mutex mutex_;
  std::condition_variable_any cv_;
  std::jthread worker_;
  ReminderStore* store_{};
  std::function<void()> changed_;
  GoogleCalendarStatus status_;
  std::filesystem::path client_file_;
  bool disconnect_{},refresh_{},selection_pending_{};
  std::vector<std::wstring> selected_;
  struct TaskCommand {std::wstring list,id,title;bool completed{};TaskOperation operation{TaskOperation::add};};
  std::vector<TaskCommand> task_commands_;
  std::wstring task_selection_;
  bool authorize_tasks_{};
};
}
