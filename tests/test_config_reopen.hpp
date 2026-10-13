#pragma once
#include "core/config.hpp"
#include <fstream>
#include <iostream>
inline int config_reopen_tests(){
  std::ifstream in(std::filesystem::path(NI_SOURCE_DIR)/"tests/fixtures/settings-reopen.json",std::ios::binary);
  std::string fixture{std::istreambuf_iterator<char>(in),{}};
  auto parsed=ni::Config::parse(fixture);
  if(fixture.size()<700||parsed.horizontal_percent!=88||parsed.last_page!=4||!parsed.hover_expand)return 53;
  // Caller sets disposable LOCALAPPDATA; never use the real user's profile.
  auto directory=std::filesystem::path(NI_SOURCE_DIR)/"out"/"config-reopen-profile";
  wchar_t previous[32768]{};DWORD size=GetEnvironmentVariableW(L"LOCALAPPDATA",previous,32768);
  struct Restore{const wchar_t* value;~Restore(){SetEnvironmentVariableW(L"LOCALAPPDATA",value);}} restore{size&&size<32768?previous:nullptr};
  if(!SetEnvironmentVariableW(L"LOCALAPPDATA",directory.c_str()))return 54;
  if(!parsed.save())return 55;
  auto loaded=ni::Config::load();if(loaded.horizontal_percent!=88||loaded.last_page!=4||!loaded.hover_expand)return 56;
  loaded.theme_index=5;loaded.reminder_morning_hour=10;
  if(!loaded.save())return 57;
  auto reopened=ni::Config::load();if(reopened.theme_index!=5||reopened.horizontal_percent!=88||reopened.reminder_morning_hour!=10)return 58;
  std::ifstream saved(ni::Config::path(),std::ios::binary);std::string text{std::istreambuf_iterator<char>(saved),{}};
  if(text.size()<700)return 59;
  for(auto invalid:{"{\"theme\":4,}","{\"theme\":04}","{\"theme\":4}garbage","{\"theme\":truex}","{\"theme\":4,\"theme\":5}","{\"theme\":4.0}"})if(ni::Config::parse(invalid).theme_index!=0)return 60;
  auto whitespace=std::string("{\n\"theme\":5,")+std::string(60000,' ')+"\"horizontalPercent\":88}";
  if(ni::Config::parse(whitespace).horizontal_percent!=88)return 61;
  std::cout<<"config: complete saved settings, reopen, backup and bounded whitespace parsing passed\n";
  return 0;
}
