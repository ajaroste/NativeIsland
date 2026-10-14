#include <winsock2.h>
#include <ws2tcpip.h>
#include "services/google_calendar.hpp"
#include "core/diagnostics.hpp"
#include "core/config.hpp"
#include <winhttp.h>
#include <wincrypt.h>
#include <bcrypt.h>
#include <shellapi.h>
#include <fstream>
#include <format>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <utility>

namespace ni {
using namespace winrt::Windows::Data::Json;
namespace {
std::string read(const std::filesystem::path& path){std::ifstream in(path,std::ios::binary|std::ios::ate);if(!in||in.tellg()>8*1024*1024)throw std::runtime_error("file");in.seekg(0);return {std::istreambuf_iterator<char>(in),{}};}
std::string base64(const unsigned char* data,DWORD count){DWORD size{};if(!CryptBinaryToStringA(data,count,CRYPT_STRING_BASE64|CRYPT_STRING_NOCRLF,nullptr,&size))throw std::runtime_error("base64");std::string out(size,'\0');if(!CryptBinaryToStringA(data,count,CRYPT_STRING_BASE64|CRYPT_STRING_NOCRLF,out.data(),&size))throw std::runtime_error("base64");out.resize(size);while(!out.empty()&&(out.back()=='='||out.back()=='\0'))out.pop_back();std::replace(out.begin(),out.end(),'+','-');std::replace(out.begin(),out.end(),'/','_');return out;}
std::string random_token(){unsigned char bytes[32]{};if(BCryptGenRandom(nullptr,bytes,32,BCRYPT_USE_SYSTEM_PREFERRED_RNG)!=0)throw std::runtime_error("random");return base64(bytes,32);}
struct HttpHandle{HINTERNET value{};~HttpHandle(){if(value)WinHttpCloseHandle(value);}};
struct Response {DWORD status{};std::string text;};
Response request(const wchar_t* host,const std::wstring& path,const wchar_t* method=L"GET",const std::string& body={},const std::string& token={},bool json=false){
  HttpHandle session{WinHttpOpen(L"NativeIsland/0.7",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};if(!session.value)throw std::runtime_error("network");
  WinHttpSetTimeouts(session.value,5000,5000,10000,10000);
  HttpHandle connection{WinHttpConnect(session.value,host,INTERNET_DEFAULT_HTTPS_PORT,0)};if(!connection.value)throw std::runtime_error("network");
  HttpHandle req{WinHttpOpenRequest(connection.value,method,path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE)};if(!req.value)throw std::runtime_error("network");
  DWORD redirects=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;WinHttpSetOption(req.value,WINHTTP_OPTION_REDIRECT_POLICY,&redirects,sizeof(redirects));
  std::wstring headers=L"Accept: application/json\r\n";if(!body.empty())headers+=json?L"Content-Type: application/json; charset=utf-8\r\n":L"Content-Type: application/x-www-form-urlencoded\r\n";if(!token.empty())headers+=L"Authorization: Bearer "+std::wstring(token.begin(),token.end())+L"\r\n";
  if(!WinHttpSendRequest(req.value,headers.c_str(),static_cast<DWORD>(headers.size()),body.empty()?nullptr:const_cast<char*>(body.data()),static_cast<DWORD>(body.size()),static_cast<DWORD>(body.size()),0)||!WinHttpReceiveResponse(req.value,nullptr))throw std::runtime_error("network");
  Response out;DWORD size=sizeof(out.status);if(!WinHttpQueryHeaders(req.value,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&out.status,&size,WINHTTP_NO_HEADER_INDEX))throw std::runtime_error("network");
  DWORD available{};while(WinHttpQueryDataAvailable(req.value,&available)){if(!available)return out;if(out.text.size()+available>8*1024*1024)throw std::runtime_error("size");size_t offset=out.text.size();out.text.resize(offset+available);DWORD got{};if(!WinHttpReadData(req.value,out.text.data()+offset,available,&got))throw std::runtime_error("network");out.text.resize(offset+got);if(!got)return out;}throw std::runtime_error("network");
}
JsonObject json_response(const Response& response){if(response.status<200||response.status>=300){if(response.status==401)throw std::runtime_error("expired");if(response.status==403)throw std::runtime_error("permission");if(response.status==429)throw std::runtime_error("quota");throw std::runtime_error("server");}return JsonObject::Parse(winrt::to_hstring(response.text));}
std::filesystem::path vault_path(){return Config::path().parent_path()/L"google-calendar.dat";}
void vault_save(const JsonObject& root){auto json=winrt::to_string(root.Stringify());if(json.size()>7*1024*1024)throw std::runtime_error("vault");DATA_BLOB in{static_cast<DWORD>(json.size()),reinterpret_cast<BYTE*>(json.data())},out{};
  if(!CryptProtectData(&in,L"NativeIsland Google Calendar",nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&out))throw std::runtime_error("vault");std::string bytes(reinterpret_cast<char*>(out.pbData),out.cbData);LocalFree(out.pbData);if(!atomic_user_file(vault_path(),bytes))throw std::runtime_error("vault");
}
JsonObject vault_load(){auto bytes=read(vault_path());DATA_BLOB in{static_cast<DWORD>(bytes.size()),reinterpret_cast<BYTE*>(bytes.data())},out{};if(!CryptUnprotectData(&in,nullptr,nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&out))throw std::runtime_error("vault");std::string json(reinterpret_cast<char*>(out.pbData),out.cbData);SecureZeroMemory(out.pbData,out.cbData);LocalFree(out.pbData);return JsonObject::Parse(winrt::to_hstring(json));}
struct SocketHandle{SOCKET value{INVALID_SOCKET};~SocketHandle(){if(value!=INVALID_SOCKET)closesocket(value);}};
JsonObject authorize(const JsonObject& client,std::stop_token stop,bool tasks=false){
  WSADATA data{};if(WSAStartup(MAKEWORD(2,2),&data))throw std::runtime_error("socket");struct Cleanup{~Cleanup(){WSACleanup();}} cleanup;
  SocketHandle listener{socket(AF_INET,SOCK_STREAM,IPPROTO_TCP)};if(listener.value==INVALID_SOCKET)throw std::runtime_error("socket");
  BOOL exclusive=TRUE;setsockopt(listener.value,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,reinterpret_cast<const char*>(&exclusive),sizeof(exclusive));
  sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);addr.sin_port=0;
  if(bind(listener.value,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))||listen(listener.value,4))throw std::runtime_error("socket");int length=sizeof(addr);getsockname(listener.value,reinterpret_cast<sockaddr*>(&addr),&length);
  std::string redirect="http://127.0.0.1:"+std::to_string(ntohs(addr.sin_port));std::string verifier=random_token(),state=random_token();
  std::string scopes="https://www.googleapis.com/auth/calendar.readonly";if(tasks)scopes+=" https://www.googleapis.com/auth/tasks";
  std::string url="https://accounts.google.com/o/oauth2/v2/auth?client_id="+oauth_encode(winrt::to_string(client.GetNamedString(L"client_id")))+"&redirect_uri="+oauth_encode(redirect)+"&response_type=code&scope="+oauth_encode(scopes)+"&access_type=offline&prompt=consent&code_challenge_method=S256&code_challenge="+oauth_challenge(verifier)+"&state="+state;
  auto browser=ShellExecuteW(nullptr,L"open",winrt::to_hstring(url).c_str(),nullptr,nullptr,SW_SHOWNORMAL);if(reinterpret_cast<INT_PTR>(browser)<=32)throw std::runtime_error("browser");
  auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(3);std::string code;
  while(!stop.stop_requested()&&std::chrono::steady_clock::now()<deadline){fd_set ready;FD_ZERO(&ready);FD_SET(listener.value,&ready);timeval wait{0,100000};if(select(0,&ready,nullptr,nullptr,&wait)<=0)continue;
    SocketHandle peer{accept(listener.value,nullptr,nullptr)};if(peer.value==INVALID_SOCKET)continue;DWORD timeout=2000;setsockopt(peer.value,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<const char*>(&timeout),sizeof(timeout));
    std::string incoming;char buffer[2048]{};while(incoming.size()<16384&&incoming.find("\r\n\r\n")==std::string::npos){int got=recv(peer.value,buffer,sizeof(buffer),0);if(got<=0)break;incoming.append(buffer,got);}
    auto end=incoming.find(" HTTP/");bool valid=incoming.starts_with("GET /?")&&end!=std::string::npos;
    std::string query=valid?incoming.substr(6,end-6):"";valid=valid&&oauth_query(query,"state")==state;
    const char* reply=valid?"HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\nConnection: close\r\n\r\nNativeIsland: Yetkilendirme alindi. Bu sekmeyi kapatabilirsiniz.":"HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\nInvalid callback";
    send(peer.value,reply,static_cast<int>(strlen(reply)),0);if(!valid)continue;
    if(!oauth_query(query,"error").empty())throw std::runtime_error("denied");code=oauth_query(query,"code");if(!code.empty())break;
  }
  if(code.empty())throw std::runtime_error(stop.stop_requested()?"cancelled":"timeout");
  std::string body="grant_type=authorization_code&code="+oauth_encode(code)+"&client_id="+oauth_encode(winrt::to_string(client.GetNamedString(L"client_id")))+"&client_secret="+oauth_encode(winrt::to_string(client.GetNamedString(L"client_secret",L"")))+"&redirect_uri="+oauth_encode(redirect)+"&code_verifier="+oauth_encode(verifier);
  auto tokens=json_response(request(L"oauth2.googleapis.com",L"/token",L"POST",body));if(tokens.GetNamedString(L"refresh_token",L"").empty())throw std::runtime_error("refresh");return tokens;
}
}
std::string oauth_encode(const std::string& value){constexpr char hex[]="0123456789ABCDEF";std::string out;for(unsigned char c:value){if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~')out+=static_cast<char>(c);else{out+='%';out+=hex[c>>4];out+=hex[c&15];}}return out;}
std::string oauth_decode(const std::string& value){auto digit=[](char c){return c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:c>='a'&&c<='f'?c-'a'+10:-1;};std::string out;for(size_t i=0;i<value.size();++i){if(value[i]=='%'&&i+2<value.size()){int a=digit(value[i+1]),b=digit(value[i+2]);if(a<0||b<0)return {};out+=static_cast<char>(a*16+b);i+=2;}else out+=value[i]=='+'?' ':value[i];}return out;}
std::string oauth_query(const std::string& query,const std::string& key){for(size_t start=0;start<query.size();){auto end=query.find('&',start);auto part=query.substr(start,end==std::string::npos?end:end-start);auto equals=part.find('=');if(equals!=std::string::npos&&oauth_decode(part.substr(0,equals))==key)return oauth_decode(part.substr(equals+1));if(end==std::string::npos)break;start=end+1;}return {};}
std::string oauth_challenge(const std::string& verifier){BCRYPT_ALG_HANDLE algorithm{};if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)!=0)throw std::runtime_error("hash");unsigned char digest[32]{};NTSTATUS result=BCryptHash(algorithm,nullptr,0,reinterpret_cast<PUCHAR>(const_cast<char*>(verifier.data())),static_cast<ULONG>(verifier.size()),digest,32);BCryptCloseAlgorithmProvider(algorithm,0);if(result!=0)throw std::runtime_error("hash");return base64(digest,32);}
std::vector<GoogleTask> parse_google_tasks(const JsonObject& root){std::vector<GoogleTask> tasks;for(auto item:root.GetNamedArray(L"items",JsonArray{})){auto o=item.GetObject();if(o.GetNamedBoolean(L"deleted",false)||o.GetNamedBoolean(L"hidden",false)||o.GetNamedString(L"status",L"needsAction")==L"completed")continue;GoogleTask t;t.id=o.GetNamedString(L"id",L"");t.title=o.GetNamedString(L"title",L"");t.notes=o.GetNamedString(L"notes",L"");t.due=o.GetNamedString(L"due",L"");if(!t.id.empty())tasks.push_back(std::move(t));}return tasks;}
TaskRequest google_task_request(TaskOperation operation,const std::wstring& list,const std::wstring& id,const std::wstring& title,bool completed){
  if(list.empty()||(operation!=TaskOperation::add&&id.empty()))throw std::invalid_argument("task identity");
  auto encoded=[](const std::wstring& value){return std::wstring(winrt::to_hstring(oauth_encode(winrt::to_string(value))));};TaskRequest result;
  result.path=L"/tasks/v1/lists/"+encoded(list)+L"/tasks";
  if(operation!=TaskOperation::add)result.path+=L"/"+encoded(id);
  if(operation==TaskOperation::remove){result.method=L"DELETE";return result;}
  const bool title_update=operation==TaskOperation::add||operation==TaskOperation::edit;
  if(title_update&&(title.empty()||title.size()>1024))throw std::invalid_argument("task title");
  JsonObject body;result.method=operation==TaskOperation::add?L"POST":L"PATCH";body.Insert(title_update?L"title":L"status",JsonValue::CreateStringValue(title_update?title:completed?L"completed":L"needsAction"));result.body=winrt::to_string(body.Stringify());return result;
}
GoogleCalendarStatus GoogleCalendarService::snapshot()const{std::scoped_lock lock(mutex_);return status_;}
void GoogleCalendarService::start(ReminderStore& store,std::function<void()> changed,bool enabled){store_=&store;changed_=std::move(changed);if(enabled)worker_=std::jthread([this](std::stop_token stop){
  if(!guarded_service("google-worker-exception",[&]{run(stop);})){std::scoped_lock lock(mutex_);status_.busy=false;status_.connected=false;status_.message=L"Google servisi başlatılamadı. Ada çalışmaya devam ediyor; tanılama startup.log dosyasında.";if(changed_)changed_();}
});}
void GoogleCalendarService::stop(){if(worker_.joinable()){worker_.request_stop();cv_.notify_all();worker_.join();}}
void GoogleCalendarService::connect(std::filesystem::path file,bool tasks){std::scoped_lock lock(mutex_);client_file_=std::move(file);authorize_tasks_=tasks;cv_.notify_one();}
void GoogleCalendarService::enable_tasks(){std::scoped_lock lock(mutex_);authorize_tasks_=true;cv_.notify_one();}
void GoogleCalendarService::select_task_list(std::wstring id){std::scoped_lock lock(mutex_);task_selection_=std::move(id);refresh_=true;cv_.notify_one();}
void GoogleCalendarService::add_task(std::wstring list,std::wstring title){if(title.empty()||title.size()>1024||list.empty())return;std::scoped_lock lock(mutex_);if(task_commands_.size()>=32)return;task_commands_.push_back({std::move(list),{},std::move(title),false});cv_.notify_one();}
void GoogleCalendarService::complete_task(std::wstring list,std::wstring id,bool done){if(list.empty()||id.empty())return;std::scoped_lock lock(mutex_);if(task_commands_.size()>=32)return;task_commands_.push_back({std::move(list),std::move(id),{},done,TaskOperation::complete});cv_.notify_one();}
void GoogleCalendarService::delete_task(std::wstring list,std::wstring id){if(list.empty()||id.empty())return;std::scoped_lock lock(mutex_);if(task_commands_.size()>=32)return;task_commands_.push_back({std::move(list),std::move(id),{},false,TaskOperation::remove});cv_.notify_one();}
bool GoogleCalendarService::update_task(std::wstring list,std::wstring id,std::wstring title){
  if(list.empty()||id.empty()||title.empty()||title.size()>1024)return false;
  std::scoped_lock lock(mutex_);if(!worker_.joinable()||!tasks_connected(status_)||status_.busy||status_.task_list_id!=list||std::none_of(status_.tasks.begin(),status_.tasks.end(),[&](const auto& task){return task.id==id&&!task.completed;})||task_commands_.size()>=32||!task_edit_gate_.acquire(list,id))return false;
  try{task_commands_.push_back({list,id,std::move(title),false,TaskOperation::edit});}catch(...){task_edit_gate_.release(list,id);throw;}
  status_.busy=true;status_.tasks_message=L"Görev kaydediliyor…";cv_.notify_one();if(changed_)changed_();return true;
}
void GoogleCalendarService::disconnect(){std::scoped_lock lock(mutex_);disconnect_=true;cv_.notify_one();}
void GoogleCalendarService::refresh(){std::scoped_lock lock(mutex_);refresh_=true;cv_.notify_one();}
void GoogleCalendarService::select(std::vector<std::wstring> ids){std::scoped_lock lock(mutex_);selected_=std::move(ids);selection_pending_=true;cv_.notify_one();}
void GoogleCalendarService::run(std::stop_token stop){
  winrt::init_apartment(winrt::apartment_type::multi_threaded);
  struct Apartment{~Apartment(){winrt::uninit_apartment();}} apartment;
  JsonObject vault;bool connected=false;std::string access;int64_t expiry=0;
  diagnostic("google-worker-ready");
  auto publish=[&](std::wstring message,bool busy){std::scoped_lock lock(mutex_);status_.message=std::move(message);status_.busy=busy;status_.connected=connected;if(changed_)changed_();};
  try{if(std::filesystem::exists(vault_path())){vault=vault_load();connected=!vault.GetNamedString(L"refresh_token",L"").empty();if(vault.GetNamedBoolean(L"tasks_enabled",false)){std::scoped_lock lock(mutex_);status_.tasks_enabled=true;status_.task_list_id=vault.GetNamedString(L"task_list",L"");status_.tasks=parse_google_tasks(vault.GetNamedObject(L"tasks_cache",JsonObject{}));for(auto item:vault.GetNamedArray(L"task_lists_cache",JsonArray{})){auto o=item.GetObject();status_.task_lists.push_back({std::wstring(o.GetNamedString(L"id")),std::wstring(o.GetNamedString(L"title"))});}status_.tasks_message=L"Son eşitlenen görevler · bağlantı kontrol ediliyor…";}publish(connected?L"Google Takvim senkronize ediliyor…":L"Google Takvim bağlı değil",false);}}catch(...){publish(L"Google bağlantı verisi okunamadı. JSON ile yeniden bağlanın; mevcut dosya korunuyor.",false);}
  auto next=std::chrono::steady_clock::now();
  while(!stop.stop_requested()){
    std::filesystem::path client;bool disconnect=false,refresh=false,selection=false,tasks_auth=false;std::vector<std::wstring> selected;std::wstring task_selection;std::vector<TaskCommand> task_commands;
    {std::unique_lock lock(mutex_);cv_.wait_until(lock,stop,next,[&]{return !client_file_.empty()||disconnect_||refresh_||selection_pending_||authorize_tasks_||!task_commands_.empty();});if(stop.stop_requested())break;client.swap(client_file_);disconnect=std::exchange(disconnect_,false);refresh=std::exchange(refresh_,false);selection=std::exchange(selection_pending_,false);selected.swap(selected_);tasks_auth=std::exchange(authorize_tasks_,false);task_selection.swap(task_selection_);task_commands.swap(task_commands_);}
    refresh=refresh||!task_commands.empty();
    try{
      if(disconnect){publish(L"Google bağlantısı kaldırılıyor…",true);auto path=vault_path();for(auto suffix:{L"",L".bak",L".tmp"}){auto file=path;file+=suffix;std::error_code ec;std::filesystem::remove(file,ec);if(ec)throw std::runtime_error("vault");}connected=false;vault=JsonObject{};access.clear();task_commands.clear();tasks_auth=false;client.clear();if(!store_->replace_google({}))throw std::runtime_error("store");{std::scoped_lock lock(mutex_);status_.calendars.clear();status_.tasks.clear();status_.task_lists.clear();status_.task_list_id.clear();status_.tasks_enabled=false;status_.tasks_message=L"Google Tasks bağlantısı kaldırıldı";}publish(L"Google bağlantısı kaldırıldı; yerel hatırlatıcılar korundu",false);}
      if(!client.empty()||tasks_auth){
        publish(L"Google hesabına tarayıcıdan izin verin (3 dakika)…",true);JsonObject credentials;if(!client.empty()){auto root=JsonObject::Parse(winrt::to_hstring(read(client)));credentials=root.GetNamedObject(L"installed");}else if(connected)credentials=vault;else throw std::runtime_error("client");auto id=credentials.GetNamedString(L"client_id");if(id.empty()||id.size()>512||!std::wstring(id).ends_with(L".apps.googleusercontent.com"))throw std::runtime_error("client");
        tasks_auth=tasks_auth||vault.GetNamedBoolean(L"tasks_enabled",false);
        auto tokens=authorize(credentials,stop,tasks_auth);if(stop.stop_requested())break;
        JsonObject new_vault;new_vault.Insert(L"version",JsonValue::CreateNumberValue(1));new_vault.Insert(L"client_id",JsonValue::CreateStringValue(id));new_vault.Insert(L"client_secret",JsonValue::CreateStringValue(credentials.GetNamedString(L"client_secret",L"")));new_vault.Insert(L"refresh_token",tokens.GetNamedValue(L"refresh_token"));new_vault.Insert(L"tasks_enabled",JsonValue::CreateBooleanValue(tasks_auth));if(vault.HasKey(L"selected"))new_vault.Insert(L"selected",vault.GetNamedValue(L"selected"));if(vault.HasKey(L"task_list"))new_vault.Insert(L"task_list",vault.GetNamedValue(L"task_list"));vault_save(new_vault);vault=new_vault;access=winrt::to_string(tokens.GetNamedString(L"access_token"));expiry=reminder_now()+static_cast<int64_t>(tokens.GetNamedNumber(L"expires_in",3600))-60;connected=true;refresh=true;
      }
      if(selection&&connected){JsonArray ids;for(auto& id:selected)ids.Append(JsonValue::CreateStringValue(id));auto previous=vault.GetNamedArray(L"selected",JsonArray{});bool had=vault.HasKey(L"selected");vault.Insert(L"selected",ids);try{vault_save(vault);}catch(...){if(had)vault.Insert(L"selected",previous);else vault.Remove(L"selected");throw;}refresh=true;}
      if(connected&&(refresh||std::chrono::steady_clock::now()>=next)){
        publish(L"Google Takvim senkronize ediliyor…",true);
        if(access.empty()||reminder_now()>=expiry){auto body="grant_type=refresh_token&client_id="+oauth_encode(winrt::to_string(vault.GetNamedString(L"client_id")))+"&client_secret="+oauth_encode(winrt::to_string(vault.GetNamedString(L"client_secret",L"")))+"&refresh_token="+oauth_encode(winrt::to_string(vault.GetNamedString(L"refresh_token")));auto response=request(L"oauth2.googleapis.com",L"/token",L"POST",body);if(response.status==400)throw std::runtime_error("expired");auto tokens=json_response(response);access=winrt::to_string(tokens.GetNamedString(L"access_token"));expiry=reminder_now()+static_cast<int64_t>(tokens.GetNamedNumber(L"expires_in",3600))-60;}
        if(vault.GetNamedBoolean(L"tasks_enabled",false)){
          try{
            auto encoded=[](const std::wstring& s){return std::wstring(winrt::to_hstring(oauth_encode(winrt::to_string(s))));};
            {std::scoped_lock lock(mutex_);status_.tasks_enabled=true;status_.tasks_message=L"Google Tasks eşitleniyor…";}
            std::vector<GoogleTaskList> lists;std::wstring page;
            for(int p=0;p<100;++p){if(stop.stop_requested())break;auto root=json_response(request(L"tasks.googleapis.com",L"/tasks/v1/users/@me/lists?maxResults=100"+page,L"GET",{},access));for(auto item:root.GetNamedArray(L"items",JsonArray{})){auto o=item.GetObject();lists.push_back({std::wstring(o.GetNamedString(L"id")),std::wstring(o.GetNamedString(L"title",L"Görevler"))});}auto token=root.GetNamedString(L"nextPageToken",L"");if(token.empty())break;if(p==99)throw std::runtime_error("size");page=L"&pageToken="+encoded(std::wstring(token));}
            std::wstring chosen=task_selection.empty()?std::wstring(vault.GetNamedString(L"task_list",L"")):task_selection;
            if(std::none_of(lists.begin(),lists.end(),[&](const auto& l){return l.id==chosen;}))chosen=lists.empty()?L"":lists.front().id;
            if(chosen!=std::wstring(vault.GetNamedString(L"task_list",L""))){vault.Insert(L"task_list",JsonValue::CreateStringValue(chosen));vault_save(vault);}
            for(auto& c:task_commands){if(stop.stop_requested())break;auto mutation=google_task_request(c.operation,c.list,c.id,c.title,c.completed);auto response=request(L"tasks.googleapis.com",mutation.path,mutation.method.c_str(),mutation.body,access,true);
              if(c.operation!=TaskOperation::remove||response.status<200||response.status>=300)json_response(response);
              if(c.operation==TaskOperation::edit){
                {std::scoped_lock lock(mutex_);if(status_.task_list_id==c.list)for(auto& task:status_.tasks)if(task.id==c.id)task.title=c.title;if(changed_)changed_();}
                if(vault.GetNamedString(L"task_list",L"")==c.list){auto cache=vault.GetNamedObject(L"tasks_cache",JsonObject{});for(auto item:cache.GetNamedArray(L"items",JsonArray{})){auto task=item.GetObject();if(task.GetNamedString(L"id",L"")==c.id)task.Insert(L"title",JsonValue::CreateStringValue(c.title));}vault_save(vault);}
              }
              // Only remove locally after Google confirms the mutation, even if the following list refresh fails.
              if(c.operation==TaskOperation::remove||(c.operation==TaskOperation::complete&&c.completed)){
                {std::scoped_lock lock(mutex_);if(status_.task_list_id==c.list)std::erase_if(status_.tasks,[&](const auto& task){return task.id==c.id;});if(changed_)changed_();}
                if(vault.GetNamedString(L"task_list",L"")==c.list){auto cache=vault.GetNamedObject(L"tasks_cache",JsonObject{});auto items=cache.GetNamedArray(L"items",JsonArray{});for(uint32_t i=items.Size();i>0;--i)if(items.GetAt(i-1).GetObject().GetNamedString(L"id",L"")==c.id)items.RemoveAt(i-1);cache.Insert(L"items",items);vault.Insert(L"tasks_cache",cache);vault_save(vault);}
              }
            }
            std::vector<GoogleTask> tasks;page.clear();
            if(!chosen.empty())for(int p=0;p<100;++p){if(stop.stop_requested())break;auto root=json_response(request(L"tasks.googleapis.com",L"/tasks/v1/lists/"+encoded(chosen)+L"/tasks?maxResults=100&showCompleted=false&showHidden=false&showDeleted=false"+page,L"GET",{},access));auto batch=parse_google_tasks(root);tasks.insert(tasks.end(),batch.begin(),batch.end());if(tasks.size()>10000)throw std::runtime_error("size");auto token=root.GetNamedString(L"nextPageToken",L"");if(token.empty())break;if(p==99)throw std::runtime_error("size");page=L"&pageToken="+encoded(std::wstring(token));}
            if(stop.stop_requested())break;std::stable_sort(tasks.begin(),tasks.end(),[](const auto& a,const auto& b){return a.completed<b.completed;});
            JsonArray cached_tasks,cached_lists;for(auto& t:tasks){JsonObject o;o.Insert(L"id",JsonValue::CreateStringValue(t.id));o.Insert(L"title",JsonValue::CreateStringValue(t.title));o.Insert(L"notes",JsonValue::CreateStringValue(t.notes));o.Insert(L"due",JsonValue::CreateStringValue(t.due));o.Insert(L"status",JsonValue::CreateStringValue(t.completed?L"completed":L"needsAction"));cached_tasks.Append(o);}for(auto& l:lists){JsonObject o;o.Insert(L"id",JsonValue::CreateStringValue(l.id));o.Insert(L"title",JsonValue::CreateStringValue(l.name));cached_lists.Append(o);}JsonObject cache;cache.Insert(L"items",cached_tasks);vault.Insert(L"tasks_cache",cache);vault.Insert(L"task_lists_cache",cached_lists);vault_save(vault);
            {std::scoped_lock lock(mutex_);status_.task_lists=std::move(lists);status_.task_list_id=chosen;status_.tasks=std::move(tasks);status_.tasks_message=L"Google Tasks güncel";}
          }catch(...){std::scoped_lock lock(mutex_);status_.tasks_message=task_commands.empty()?L"Tasks eşitlenemedi. Tasks API ve izinlerini kontrol edin; önceki liste korundu.":L"Görev işlemi doğrulanamadı. Eşitleyip kontrol edin; otomatik tekrar gönderilmedi.";}
        }
        std::vector<GoogleCalendarInfo> calendars;std::wstring page;
        for(int pages=0;pages<100;++pages){if(stop.stop_requested())break;auto path=L"/calendar/v3/users/me/calendarList?maxResults=250"+page;auto root=json_response(request(L"www.googleapis.com",path,L"GET",{},access));for(auto item:root.GetNamedArray(L"items",JsonArray{})){auto obj=item.GetObject();GoogleCalendarInfo info;info.id=obj.GetNamedString(L"id");info.name=obj.GetNamedString(L"summary",L"Takvim");info.selected=obj.GetNamedBoolean(L"selected",obj.GetNamedBoolean(L"primary",false));if(vault.HasKey(L"selected")){info.selected=false;for(auto saved:vault.GetNamedArray(L"selected"))if(saved.GetString()==info.id)info.selected=true;}calendars.push_back(info);}auto token=root.GetNamedString(L"nextPageToken",L"");if(token.empty())break;if(pages==99)throw std::runtime_error("size");page=L"&pageToken="+std::wstring(winrt::to_hstring(oauth_encode(winrt::to_string(token))));}
        auto now=reminder_now();auto begin=reminder_day_bounds(reminder_day_at_offset(now,-1)).first;auto until=reminder_day_bounds(reminder_day_at_offset(now,8)).first;
        std::vector<Reminder> events;
        for(auto& calendar:calendars)if(calendar.selected){std::wstring token;
          for(int pages=0;pages<100;++pages){if(stop.stop_requested())break;
            auto path=L"/calendar/v3/calendars/"+std::wstring(winrt::to_hstring(oauth_encode(winrt::to_string(calendar.id))))+L"/events?singleEvents=true&orderBy=startTime&maxResults=2500&timeMin="+std::wstring(winrt::to_hstring(oauth_encode(winrt::to_string(calendar_utc(begin)))))+L"&timeMax="+std::wstring(winrt::to_hstring(oauth_encode(winrt::to_string(calendar_utc(until)))))+token;
            auto root=json_response(request(L"www.googleapis.com",path,L"GET",{},access));auto batch=parse_calendar_events(root,calendar.id);events.insert(events.end(),batch.begin(),batch.end());if(events.size()>10000)throw std::runtime_error("size");auto next_token=root.GetNamedString(L"nextPageToken",L"");if(next_token.empty())break;if(pages==99)throw std::runtime_error("size");token=L"&pageToken="+std::wstring(winrt::to_hstring(oauth_encode(winrt::to_string(next_token))));
          }
        }
        if(stop.stop_requested())break;if(!store_->replace_google(std::move(events)))throw std::runtime_error("store");{std::scoped_lock lock(mutex_);status_.calendars=std::move(calendars);status_.last_sync=reminder_now();}publish(L"Google Takvim güncel · salt okunur",false);
      }
    }catch(const std::exception& e){std::string code=e.what();std::wstring message=code=="expired"?L"Google izni sona erdi. JSON dosyasıyla yeniden bağlanın.":code=="permission"?L"Google Takvim API veya okuma izni eksik. Kurulum yönergesini kontrol edin.":code=="denied"?L"Google yetkilendirmesi iptal edildi.":code=="timeout"?L"Google yetkilendirme süresi doldu. Yeniden deneyin.":code=="vault"?L"Google bağlantı verisi kaydedilemedi veya kaldırılamadı.":code=="store"?store_->error():L"Google bağlantısı başarısız. Ağ ve OAuth Masaüstü JSON dosyasını kontrol edin.";publish(std::move(message),false);
    }catch(...){publish(L"Google yanıtı veya istemci JSON dosyası geçersiz. Önceki etkinlikler korundu.",false);}
    {std::scoped_lock lock(mutex_);for(const auto& command:task_commands)if(command.operation==TaskOperation::edit)task_edit_gate_.release(command.list,command.id);if(disconnect)task_edit_gate_.clear();}
    next=std::chrono::steady_clock::now()+std::chrono::minutes(5);
  }
}
}
