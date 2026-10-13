#pragma once
#include "services/reminders.hpp"
#include "services/google_calendar.hpp"
#include <fstream>
#include <iostream>
inline int reminder_tests(){
  using namespace ni;using namespace winrt::Windows::Data::Json;
  auto fail=[](const char* detail){std::cerr<<"Reminder test failed: "<<detail<<'\n';return 40;};
  if(oauth_challenge("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk")!="E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM")return fail("RFC7636 PKCE");
  if(oauth_query("code=a%2Bb&state=xyz","code")!="a+b"||oauth_encode("a/b +")!="a%2Fb%20%2B"||oauth_decode("%GG")!="")return fail("OAuth query encoding");
  auto utc=parse_calendar_time(L"2026-10-12T09:00:00+03:00");if(!utc||calendar_utc(utc)!=L"2026-10-12T06:00:00Z"||parse_calendar_time(L"2026-10-12T06:00:00.123Z")!=utc||parse_calendar_time(L"2026-99-12T09:00:00Z")!=0)return fail("RFC3339 timezone");
  auto events=parse_calendar_events(JsonObject::Parse(LR"({"items":[{"id":"recurring_1","summary":"Toplantı 🔔","start":{"dateTime":"2026-10-12T09:00:00+03:00"},"end":{"dateTime":"2026-10-12T10:00:00+03:00"}},{"id":"all","summary":"Tüm gün","start":{"date":"2026-10-12"},"end":{"date":"2026-10-13"}},{"id":"deleted","status":"cancelled"}]})"),L"primary");
  if(events.size()!=2||!events[0].google||!events[1].all_day||events[0].start!=utc||events[0].id!=L"google:primary:recurring_1")return fail("Calendar event fixture");
  SYSTEMTIME date{2026,10,0,12,8,1,0,0};auto morning=reminder_utc(date);auto [begin,end]=reminder_day_bounds(morning);if(begin>=morning||end<=morning||reminder_day(begin)!=L"2026-10-12")return fail("local day boundaries");
  if(reminder_day(reminder_day_at_offset(morning,-1))!=L"2026-10-11"||reminder_day(reminder_day_at_offset(morning,1))!=L"2026-10-13")return fail("adjacent civil days");
  SYSTEMTIME leap{2024,3,0,1,12,0,0,0},year{2026,1,0,1,12,0,0,0};
  if(reminder_day(reminder_day_at_offset(reminder_utc(leap),-1))!=L"2024-02-29"||reminder_day(reminder_day_at_offset(reminder_utc(year),-1))!=L"2025-12-31")return fail("leap/month/year boundaries");
  Reminder previous;previous.id=L"previous";previous.start=begin-3600;previous.end=begin;previous.done=true;
  Reminder current;current.id=L"current";current.start=morning;current.end=morning+3600;
  Reminder next;next.id=L"next";next.start=end;next.end=end+3600;
  std::vector<Reminder> days{previous,current,next};
  auto yesterday=reminders_for_day(days,reminder_day_at_offset(morning,-1),true),today=reminders_for_day(days,morning,true),tomorrow=reminders_for_day(days,reminder_day_at_offset(morning,1),true);
  if(yesterday.size()!=1||yesterday[0].id!=L"previous"||today.size()!=1||today[0].id!=L"current"||tomorrow.size()!=1||tomorrow[0].id!=L"next"||reminders_today(days,morning).size()!=1||!reminders_for_day(days,reminder_day_at_offset(morning,-1)).empty())return fail("selected day and completion filtering");
  GUID id{};CoCreateGuid(&id);wchar_t guid[40]{};StringFromGUID2(id,guid,40);auto root=std::filesystem::current_path()/L"reminder-tests"/guid;auto path=root/L"reminders.json";
  if(reminders_pending_today(days,morning).size()!=1||!reminders_pending_today(days,current.end).empty())return fail("expired reminders leave collapsed summary");
  current.done=true;if(!reminders_pending_today({current},morning).empty()||reminders_for_day({current},morning,true).size()!=1)return fail("completed stays in history only");
  ReminderStore store(path);if(!store.load())return fail("new store");Reminder local;local.id=L"local:test";local.title=L"Uzun başlık \"test\"\nTürkçe 🔔";local.start=morning+3600;local.end=local.start+3600;
  ReminderStore expired_store(root/L"expired.json");Reminder expired=current;expired.done=false;expired.id=L"expired";expired.title=L"Expired event";expired.start=morning-3600;expired.end=morning;
  if(!expired_store.load()||!expired_store.add(expired)||!expired_store.tick(morning,true,8).empty())return fail("expired event produces no daily or due notice on startup");
  if(!store.add(local)||store.add(local)||store.snapshot().size()!=1)return fail("add/duplicate/persistence");
  if(!store.tick(morning,false,8).empty()||!store.tick(begin,true,8).empty())return fail("disabled/before morning");
  auto notices=store.tick(morning,true,8);if(notices.size()!=1||!notices[0].summary||!store.tick(morning,true,8).empty())return fail("one daily summary");
  ReminderStore update(path);if(!update.load()||update.snapshot().size()!=1||update.snapshot()[0].title!=local.title||!update.tick(morning,true,8).empty())return fail("update preserves data and delivered summary");
  notices=update.tick(local.start,true,8);if(notices.size()!=1||notices[0].summary||!update.tick(local.start+10,true,8).empty())return fail("due event no duplicates");
  ReminderStore restart(path);if(!restart.load()||!restart.tick(local.start+10,true,8).empty())return fail("due delivery survives restart");
  if(!restart.replace_google(events)||restart.snapshot().size()!=3||restart.remove(events[0].id))return fail("Google readonly merge");
  if(!restart.complete(events[0].id)||!restart.replace_google(events))return fail("local Google completion");
  auto merged=restart.snapshot();auto it=std::find_if(merged.begin(),merged.end(),[&](auto& r){return r.id==events[0].id;});if(it==merged.end()||!it->done)return fail("completion preserved on sync");
  if(!restart.replace_google({})||restart.snapshot().size()!=1)return fail("disconnect preserves local");
  auto original=restart.snapshot();SetFileAttributesW(path.c_str(),FILE_ATTRIBUTE_READONLY);Reminder second=local;second.id=L"local:second";bool saved=restart.add(second);SetFileAttributesW(path.c_str(),FILE_ATTRIBUTE_NORMAL);if(saved||restart.snapshot().size()!=original.size())return fail("write failure rollback");
  if(!atomic_user_file(path,"{broken",false))return fail("corrupt fixture");ReminderStore recovery(path);if(!recovery.load()||recovery.snapshot().empty()||recovery.error().empty())return fail("backup recovery");
  auto future=root/L"future.json";if(!atomic_user_file(future,"{\"version\":99,\"entries\":[]}",false))return fail("future fixture");ReminderStore newer(future);if(newer.load()||newer.add(local))return fail("future schema overwrite protection");
  auto malformed=root/L"bad.json";atomic_user_file(malformed,"{broken",false);ReminderStore broken(malformed);if(broken.load()||broken.add(local))return fail("corrupt data overwrite protection");
  auto config=Config::parse("{\"theme\":4,\"reminderMorningHour\":99,\"reminderNotifications\":false,\"lastPage\":5}");if(config.theme_index!=4||config.reminder_morning_hour!=23||config.reminder_notifications||config.last_page!=5)return fail("config migration preserves old settings");
  std::cout<<"reminders: persistence, restart/update, backup, schema, daily/due deduplication, OAuth PKCE and Calendar fixtures passed\n";return 0;
}
