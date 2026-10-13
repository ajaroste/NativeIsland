#pragma once
#include "services/google_calendar.hpp"
#include "ui/island_shape.hpp"
#include "services/audio_favorites.hpp"
inline int tasks_tests(){
  using winrt::Windows::Data::Json::JsonObject;
  auto tasks=ni::parse_google_tasks(JsonObject::Parse(LR"({"items":[{"id":"done","title":"Bitti","status":"completed"},{"id":"a","title":"Türkçe görev","due":"2026-10-14T00:00:00.000Z"},{"id":"hidden","hidden":true},{"id":"deleted","deleted":true},{"title":"No id"},{"id":"b","notes":"Uzun not"}]})"));
  if(tasks.size()!=2||tasks[0].id!=L"a"||tasks[1].id!=L"b"||tasks[0].completed||tasks[0].due.substr(0,10)!=L"2026-10-14")return 70;
  if(!ni::parse_google_tasks(JsonObject::Parse(LR"({"items":[{"id":"done","status":"completed"}]})")).empty())return 102;
  std::vector<ni::GoogleTask> mixed{{L"done",L"",L"",L"",true},{L"pending",L"",L"",L"",false}};if(ni::pending_task_indices(mixed)!=std::vector<size_t>{1})return 103;
  auto deletion=ni::google_task_request(ni::TaskOperation::remove,L"list/a",L"task?b");if(deletion.method!=L"DELETE"||deletion.path!=L"/tasks/v1/lists/list%2Fa/tasks/task%3Fb"||!deletion.body.empty())return 104;
  auto addition=ni::google_task_request(ni::TaskOperation::add,L"list",{},L"Türkçe \"görev\"");if(addition.method!=L"POST"||addition.path!=L"/tasks/v1/lists/list/tasks"||JsonObject::Parse(winrt::to_hstring(addition.body)).GetNamedString(L"title")!=L"Türkçe \"görev\"")return 105;
  auto completion=ni::google_task_request(ni::TaskOperation::complete,L"list",L"task");if(completion.method!=L"PATCH"||JsonObject::Parse(winrt::to_hstring(completion.body)).GetNamedString(L"status")!=L"completed")return 106;
  bool rejected=false;try{ni::google_task_request(ni::TaskOperation::remove,L"list",L"");}catch(const std::invalid_argument&){rejected=true;}if(!rejected)return 107;
  if(!ni::parse_google_tasks(JsonObject::Parse(L"{}")).empty())return 71;
  auto config=ni::Config::parse("{\"islandShape\":99,\"lastPage\":7}");if(config.island_shape!=1||config.last_page!=7||ni::Config::parse("{}").island_shape!=0)return 72;
  if(!ni::island_contains(-10,0,500,250,30,true)||ni::island_contains(-10,16,500,250,30,true)||ni::island_contains(0,249,500,250,30,true)||!ni::island_contains(250,249,500,250,30,true)||ni::island_contains(0,0,500,250,30,false))return 73;
  if(ni::page_height(ni::Page::media,0,0,false,false)!=250||ni::page_height(ni::Page::sound,9,0,false,false)!=512||ni::page_height(ni::Page::tasks,10,0,false,false)!=414)return 74;
  ni::GoogleCalendarStatus connection;if(ni::tasks_connected(connection))return 90;connection.connected=true;if(ni::tasks_connected(connection))return 91;connection.tasks_enabled=true;if(!ni::tasks_connected(connection))return 92;connection.connected=false;if(ni::tasks_connected(connection))return 93;
  auto favorite_path=std::filesystem::temp_directory_path()/(L"native-island-favorites-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64())+L".json");
  ni::AudioFavorites favorites(favorite_path);if(!favorites.toggle(L"C:\\Apps\\Spotify.exe"))return 94;
  ni::AudioFavorites reopened(favorite_path);reopened.load();ni::AudioSnapshot audio;audio.sessions={{L"first",L"Chrome",.5f,false,L"c:\\apps\\chrome.exe"},{L"second",L"Spotify",.8f,false,L"c:\\apps\\spotify.exe"},{L"third",L"Other",.3f,false,L"c:\\apps\\other.exe"}};reopened.apply(audio);if(audio.sessions[0].id!=L"second"||!audio.sessions[0].favorite||audio.sessions[1].id!=L"first")return 95;
  audio.sessions.erase(audio.sessions.begin());reopened.apply(audio);if(audio.sessions.size()!=2||audio.sessions[0].favorite)return 96;
  auto returning=audio;returning.sessions.push_back({L"restarted-session",L"Spotify",.7f,false,L"C:\\Apps\\Spotify.exe"});reopened.apply(returning);if(returning.sessions[0].id!=L"restarted-session"||!returning.sessions[0].favorite)return 101;
  if(!reopened.toggle(L"c:\\apps\\spotify.exe"))return 97;ni::AudioFavorites removed(favorite_path);removed.load();audio.sessions.push_back({L"new-session",L"Spotify",.7f,false,L"c:\\apps\\spotify.exe"});removed.apply(audio);if(audio.sessions.back().favorite||audio.sessions[0].id!=L"first")return 98;
  std::error_code ec;std::filesystem::remove(favorite_path,ec);for(auto suffix:{L".bak",L".tmp"}){auto artifact=favorite_path;artifact+=suffix;std::filesystem::remove(artifact,ec);}
  return 0;
}
