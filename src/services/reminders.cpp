#include "services/reminders.hpp"
#include "core/config.hpp"
#include <objbase.h>
#include <fstream>
#include <algorithm>
#include <format>
#include <set>
#include <cmath>

namespace ni {
using namespace winrt::Windows::Data::Json;
namespace {
constexpr int64_t epoch=11644473600LL;
std::string read(const std::filesystem::path& path){std::ifstream in(path,std::ios::binary|std::ios::ate);if(!in||in.tellg()>8*1024*1024)throw std::runtime_error("file");in.seekg(0);return {std::istreambuf_iterator<char>(in),{}};}
JsonValue str(const std::wstring& value){return JsonValue::CreateStringValue(value);}
int64_t raw_time(const SYSTEMTIME& time){FILETIME file{};if(!SystemTimeToFileTime(&time,&file))return 0;ULARGE_INTEGER value{};value.LowPart=file.dwLowDateTime;value.HighPart=file.dwHighDateTime;return static_cast<int64_t>(value.QuadPart/10000000)-epoch;}
SYSTEMTIME raw_system(int64_t seconds){ULARGE_INTEGER value{};value.QuadPart=static_cast<uint64_t>(seconds+epoch)*10000000;FILETIME file{value.LowPart,value.HighPart};SYSTEMTIME time{};FileTimeToSystemTime(&file,&time);return time;}
}
int64_t reminder_now(){FILETIME file{};GetSystemTimeAsFileTime(&file);ULARGE_INTEGER value{};value.LowPart=file.dwLowDateTime;value.HighPart=file.dwHighDateTime;return static_cast<int64_t>(value.QuadPart/10000000)-epoch;}
int64_t reminder_utc(const SYSTEMTIME& local){SYSTEMTIME utc{};return TzSpecificLocalTimeToSystemTime(nullptr,&local,&utc)?raw_time(utc):0;}
SYSTEMTIME reminder_local(int64_t utc){auto time=raw_system(utc);SYSTEMTIME local{};SystemTimeToTzSpecificLocalTime(nullptr,&time,&local);return local;}
std::wstring reminder_day(int64_t utc){auto time=reminder_local(utc);return std::format(L"{:04}-{:02}-{:02}",time.wYear,time.wMonth,time.wDay);}
std::pair<int64_t,int64_t> reminder_day_bounds(int64_t now){auto local=reminder_local(now);local.wHour=local.wMinute=local.wSecond=local.wMilliseconds=0;int64_t first=reminder_utc(local);auto next=raw_system(raw_time(local)+86400);return {first,reminder_utc(next)};}
std::wstring reminder_time(const Reminder& r){auto time=reminder_local(r.start);return r.all_day?L"Tüm gün":std::format(L"{:02}:{:02}",time.wHour,time.wMinute);}
int64_t reminder_day_at_offset(int64_t now,int offset){auto local=reminder_local(now);local.wHour=12;local.wMinute=local.wSecond=local.wMilliseconds=0;auto shifted=raw_system(raw_time(local)+static_cast<int64_t>(offset)*86400);return reminder_utc(shifted);}
std::vector<Reminder> reminders_for_day(const std::vector<Reminder>& entries,int64_t day,bool include_done){auto [begin,end]=reminder_day_bounds(day);std::vector<Reminder> out;for(auto& r:entries)if((include_done||!r.done)&&r.start<end&&std::max(r.end,r.start+1)>begin)out.push_back(r);std::stable_sort(out.begin(),out.end(),[](auto& a,auto& b){return a.all_day!=b.all_day?a.all_day:a.start<b.start;});return out;}
std::vector<Reminder> reminders_today(const std::vector<Reminder>& entries,int64_t now){return reminders_for_day(entries,now);}
std::vector<Reminder> reminders_pending_today(const std::vector<Reminder>& entries,int64_t now){auto out=reminders_today(entries,now);std::erase_if(out,[now](const Reminder& r){return r.end<=now;});return out;}
int64_t parse_calendar_time(const std::wstring& value,bool date_only){
  SYSTEMTIME time{};unsigned year{},month{},day{},hour{},minute{},second{};
  if(swscanf_s(value.c_str(),L"%u-%u-%u",&year,&month,&day)!=3||year<1970||year>9999||month<1||month>12||day<1||day>31)return 0;
  time.wYear=static_cast<WORD>(year);time.wMonth=static_cast<WORD>(month);time.wDay=static_cast<WORD>(day);
  if(date_only){FILETIME validation{};if(!SystemTimeToFileTime(&time,&validation))return 0;return reminder_utc(time);}
  if(value.size()<20||value[10]!=L'T'||swscanf_s(value.c_str()+11,L"%u:%u:%u",&hour,&minute,&second)!=3||hour>23||minute>59||second>59)return 0;
  time.wHour=static_cast<WORD>(hour);time.wMinute=static_cast<WORD>(minute);time.wSecond=static_cast<WORD>(second);int64_t utc=raw_time(time);if(!utc)return 0;
  size_t zone=value.find_first_of(L"Z+-",19);if(zone==std::wstring::npos)return 0;
  if(value[zone]==L'Z')return zone+1==value.size()?utc:0;
  unsigned zh{},zm{};if(value.size()!=zone+6||swscanf_s(value.c_str()+zone+1,L"%u:%u",&zh,&zm)!=2||zh>23||zm>59)return 0;
  return utc-(value[zone]==L'+'?1:-1)*static_cast<int64_t>(zh*3600+zm*60);
}
std::wstring calendar_utc(int64_t seconds){auto t=raw_system(seconds);return std::format(L"{:04}-{:02}-{:02}T{:02}:{:02}:{:02}Z",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond);}
std::vector<Reminder> parse_calendar_events(const JsonObject& root,const std::wstring& calendar){std::vector<Reminder> out;for(auto value:root.GetNamedArray(L"items",JsonArray{})){
  auto obj=value.GetObject();if(obj.GetNamedString(L"status",L"")==L"cancelled")continue;
  auto start=obj.GetNamedObject(L"start",JsonObject{}),end=obj.GetNamedObject(L"end",JsonObject{});bool all=start.HasKey(L"date");
  auto id=obj.GetNamedString(L"id",L"");if(id.empty())continue;Reminder r;r.google=true;r.calendar=calendar;r.id=L"google:"+calendar+L":"+std::wstring(id);r.title=obj.GetNamedString(L"summary",L"Başlıksız etkinlik");r.all_day=all;
  r.start=parse_calendar_time(std::wstring(start.GetNamedString(all?L"date":L"dateTime",L"")),all);r.end=parse_calendar_time(std::wstring(end.GetNamedString(all?L"date":L"dateTime",L"")),all);
  if(r.start&&r.end>r.start)out.push_back(std::move(r));
}return out;}
bool atomic_user_file(const std::filesystem::path& path,const std::string& contents,bool backup){
  std::error_code error;std::filesystem::create_directories(path.parent_path(),error);if(error)return false;
  auto tmp=path;tmp+=L".tmp";std::ofstream stream(tmp,std::ios::binary|std::ios::trunc);if(!stream)return false;stream.write(contents.data(),static_cast<std::streamsize>(contents.size()));stream.flush();bool ok=stream.good();stream.close();if(!ok)return false;
  if(backup&&std::filesystem::exists(path,error)){auto bak=path;bak+=L".bak";if(!CopyFileW(path.c_str(),bak.c_str(),FALSE))return false;}
  return MoveFileExW(tmp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
}
ReminderStore::ReminderStore(std::filesystem::path path):path_(path.empty()?Config::path().parent_path()/L"reminders.json":std::move(path)){}
std::wstring ReminderStore::error()const{std::scoped_lock lock(mutex_);return error_;}
std::vector<Reminder> ReminderStore::snapshot()const{std::scoped_lock lock(mutex_);return entries_;}
bool ReminderStore::load(){std::scoped_lock lock(mutex_);entries_.clear();delivered_.clear();writable_=true;recovered_=false;error_.clear();summary_day_.clear();
  auto decode=[&](const std::filesystem::path& path){auto root=JsonObject::Parse(winrt::to_hstring(read(path)));auto version=root.GetNamedNumber(L"version",0);if(version>1)throw std::runtime_error("future schema");if(version!=1)throw std::runtime_error("schema");std::vector<Reminder> entries;
    std::set<std::wstring> unique;
    for(auto value:root.GetNamedArray(L"entries")){auto obj=value.GetObject();Reminder r;r.id=obj.GetNamedString(L"id");r.title=obj.GetNamedString(L"title");r.calendar=obj.GetNamedString(L"calendar",L"");double start=obj.GetNamedNumber(L"start"),end=obj.GetNamedNumber(L"end");if(!std::isfinite(start)||!std::isfinite(end)||start<=0||end<=start||end>253402300799LL||std::floor(start)!=start||std::floor(end)!=end)throw std::runtime_error("data");r.start=static_cast<int64_t>(start);r.end=static_cast<int64_t>(end);r.all_day=obj.GetNamedBoolean(L"allDay",false);r.done=obj.GetNamedBoolean(L"done",false);r.google=obj.GetNamedBoolean(L"google",false);if(r.id.empty()||r.title.size()>2048||!unique.insert(r.id).second||entries.size()>=10000)throw std::runtime_error("data");entries.push_back(std::move(r));}
    std::vector<std::wstring> delivered;for(auto val:root.GetNamedArray(L"delivered",JsonArray{}))delivered.emplace_back(val.GetString());entries_=std::move(entries);delivered_=std::move(delivered);summary_day_=root.GetNamedString(L"summaryDay",L"");};
  std::error_code ec;if(!std::filesystem::exists(path_,ec)&&!ec){auto bak=path_;bak+=L".bak";if(!std::filesystem::exists(bak,ec))return true;}
  try{decode(path_);return true;}catch(const std::exception& e){if(std::string(e.what())=="future schema"){writable_=false;error_=L"Hatırlatıcı verisi daha yeni bir sürüme ait; üzerine yazılmıyor.";return false;}}catch(...){}
  auto bak=path_;bak+=L".bak";try{decode(bak);recovered_=true;error_=L"Hatırlatıcılar yedekten kurtarıldı.";return true;}catch(...){writable_=false;error_=L"Hatırlatıcı dosyası okunamadı; veri kaybını önlemek için üzerine yazılmıyor.";return false;}
}
bool ReminderStore::save_locked(){if(!writable_)return false;JsonObject root;root.Insert(L"version",JsonValue::CreateNumberValue(1));root.Insert(L"summaryDay",str(summary_day_));JsonArray entries,delivered;for(auto& r:entries_){JsonObject obj;obj.Insert(L"id",str(r.id));obj.Insert(L"title",str(r.title));obj.Insert(L"calendar",str(r.calendar));obj.Insert(L"start",JsonValue::CreateNumberValue(static_cast<double>(r.start)));obj.Insert(L"end",JsonValue::CreateNumberValue(static_cast<double>(r.end)));obj.Insert(L"allDay",JsonValue::CreateBooleanValue(r.all_day));obj.Insert(L"done",JsonValue::CreateBooleanValue(r.done));obj.Insert(L"google",JsonValue::CreateBooleanValue(r.google));entries.Append(obj);}for(auto& id:delivered_)delivered.Append(str(id));root.Insert(L"entries",entries);root.Insert(L"delivered",delivered);
  if(!atomic_user_file(path_,winrt::to_string(root.Stringify()),!recovered_)){error_=L"Hatırlatıcı kaydedilemedi; önceki veriler korundu.";return false;}recovered_=false;error_.clear();return true;
}
bool ReminderStore::add(Reminder value){std::scoped_lock lock(mutex_);if(!writable_||value.title.empty()||value.title.size()>2048||value.start<=0||value.end<=value.start||value.end>253402300799LL||entries_.size()>=10000)return false;
  if(value.id.empty()){GUID id{};if(FAILED(CoCreateGuid(&id)))return false;wchar_t text[40]{};StringFromGUID2(id,text,40);value.id=L"local:"+std::wstring(text);}if(std::any_of(entries_.begin(),entries_.end(),[&](auto& r){return r.id==value.id;}))return false;
  entries_.push_back(std::move(value));if(save_locked())return true;entries_.pop_back();return false;
}
bool ReminderStore::remove(const std::wstring& id){std::scoped_lock lock(mutex_);auto old=entries_;auto it=std::find_if(entries_.begin(),entries_.end(),[&](auto& r){return r.id==id&&!r.google;});if(it==entries_.end())return false;entries_.erase(it);if(save_locked())return true;entries_=std::move(old);return false;}
bool ReminderStore::complete(const std::wstring& id){std::scoped_lock lock(mutex_);auto it=std::find_if(entries_.begin(),entries_.end(),[&](auto& r){return r.id==id;});if(it==entries_.end())return false;it->done=!it->done;if(save_locked())return true;it->done=!it->done;return false;}
bool ReminderStore::replace_google(std::vector<Reminder> events){std::scoped_lock lock(mutex_);if(!writable_)return false;size_t local=std::count_if(entries_.begin(),entries_.end(),[](auto& r){return !r.google;});if(local+events.size()>10000){error_=L"Hatırlatıcı sınırı aşıldı; önceki kayıtlar korundu.";return false;}auto old=entries_;for(auto& r:events){auto it=std::find_if(old.begin(),old.end(),[&](auto& prev){return prev.id==r.id;});if(it!=old.end())r.done=it->done;}std::erase_if(entries_,[](auto& r){return r.google;});entries_.insert(entries_.end(),events.begin(),events.end());if(save_locked())return true;entries_=std::move(old);return false;}
std::vector<ReminderNotice> ReminderStore::tick(int64_t now,bool enabled,int morning){std::scoped_lock lock(mutex_);std::vector<ReminderNotice> notices;if(!enabled||!writable_)return notices;auto today=reminders_pending_today(entries_,now);auto day=reminder_day(now);auto time=reminder_local(now);auto previous_day=summary_day_;auto previous_delivered=delivered_;
  if(!today.empty()&&time.wHour>=std::clamp(morning,0,23)&&summary_day_!=day){std::wstring body=std::format(L"Bugün {} hatırlatıcı var",today.size());for(size_t i=0;i<std::min<size_t>(3,today.size());++i)body+=L"\n"+reminder_time(today[i])+L" · "+today[i].title;notices.push_back({L"Günün hatırlatıcıları",body,true});summary_day_=day;}
  for(auto& r:today){auto key=day+L"|"+r.id;if(!r.all_day&&r.start<=now&&std::max(r.end,r.start+3600)>now&&std::find(delivered_.begin(),delivered_.end(),key)==delivered_.end()){if(notices.size()<5){notices.push_back({r.title,reminder_time(r)+L" · "+(r.google?L"Google Takvim":L"Hatırlatıcı"),false});delivered_.push_back(key);}}}
  std::erase_if(delivered_,[&](auto& key){return key.substr(0,10)!=day;});
  if(!notices.empty()&&!save_locked()){summary_day_=previous_day;delivered_=std::move(previous_delivered);notices.clear();}return notices;
}
}
