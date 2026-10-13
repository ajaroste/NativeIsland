#include "core/config.hpp"
#include <windows.h>
#include <appmodel.h>
#include <algorithm>
#include <fstream>
#include <charconv>
#include <map>
#include <optional>
#include <string_view>
#include <stdexcept>
#include <sstream>

namespace ni {
namespace {
std::string read_file(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary|std::ios::ate);
  if(!stream||stream.tellg()<0||stream.tellg()>=65536)return {};
  stream.seekg(0);
  return {std::istreambuf_iterator<char>(stream), {}};
}
using Fields=std::map<std::string,std::string>;
// This schema is a flat object of ASCII keys and integer/boolean values.
// Iterative, bounded parsing avoids MSVC regex's recursive matching stack.
std::optional<Fields> flat_json(std::string_view json) {
  if(json.empty()||json.size()>=65536)return std::nullopt;
  size_t pos=0;Fields fields;
  auto space=[&]{while(pos<json.size()&&(json[pos]==' '||json[pos]=='\t'||json[pos]=='\r'||json[pos]=='\n'))++pos;};
  auto take=[&](char ch){space();if(pos==json.size()||json[pos]!=ch)return false;++pos;return true;};
  auto alpha=[](char ch){return (ch>='A'&&ch<='Z')||(ch>='a'&&ch<='z');};
  auto digit=[](char ch){return ch>='0'&&ch<='9';};
  if(!take('{'))return std::nullopt;
  space();if(pos<json.size()&&json[pos]=='}'){++pos;space();return pos==json.size()?std::optional<Fields>(std::move(fields)):std::nullopt;}
  for(;;){
    if(!take('"'))return std::nullopt;
    size_t start=pos;if(pos==json.size()||!alpha(json[pos]))return std::nullopt;
    while(pos<json.size()&&(alpha(json[pos])||digit(json[pos])))++pos;
    std::string key(json.substr(start,pos-start));
    if(pos==json.size()||json[pos++]!='"'||!take(':'))return std::nullopt;
    space();start=pos;
    if(json.substr(pos,4)=="true")pos+=4;
    else if(json.substr(pos,5)=="false")pos+=5;
    else {
      if(pos<json.size()&&json[pos]=='-')++pos;
      if(pos==json.size()||!digit(json[pos]))return std::nullopt;
      if(json[pos]=='0')++pos;else while(pos<json.size()&&digit(json[pos]))++pos;
    }
    if(!fields.emplace(std::move(key),std::string(json.substr(start,pos-start))).second)return std::nullopt;
    space();if(pos==json.size())return std::nullopt;
    if(json[pos]=='}'){++pos;space();return pos==json.size()?std::optional<Fields>(std::move(fields)):std::nullopt;}
    if(json[pos++]!=',')return std::nullopt;
  }
}
int integer(const std::string& json, const char* key, int fallback) {
  auto fields=flat_json(json);if(!fields)return fallback;
  auto found=fields->find(key);if(found==fields->end())return fallback;
  auto& token=found->second;if(token=="true"||token=="false")return fallback;
  int result{};auto [end,error]=std::from_chars(token.data(),token.data()+token.size(),result);
  if(error==std::errc::result_out_of_range)throw std::out_of_range("config integer");
  return error==std::errc{}&&end==token.data()+token.size()?result:fallback;
}
bool boolean(const std::string& json, const char* key, bool fallback) {
  auto fields=flat_json(json);if(!fields)return fallback;
  auto found=fields->find(key);if(found==fields->end())return fallback;
  return found->second=="true"?true:found->second=="false"?false:fallback;
}
bool valid_json(const std::string& json){
  return flat_json(json).has_value();
}
}

std::filesystem::path Config::path() {
  wchar_t buffer[MAX_PATH]{};
  DWORD size = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, MAX_PATH);
  std::filesystem::path base = size ? buffer : L".";
  return base / L"NativeIsland" / L"settings.json";
}

Config Config::load() {
  auto json=read_file(path());if(valid_json(json))return parse(json);
  auto backup=path();backup+=L".bak";return parse(read_file(backup));
}
Config Config::parse(const std::string& json) {
  Config c;
  if (json.empty()) return c;
  try {
  if (!valid_json(json)) return c;
  c.collapsed_width = integer(json, "collapsedWidth", c.collapsed_width);
  c.collapsed_height = integer(json, "collapsedHeight", c.collapsed_height);
  c.expanded_width = integer(json, "expandedWidth", c.expanded_width);
  c.expanded_height = integer(json, "expandedHeight", c.expanded_height);
  if(integer(json,"version",0)<3) {
    if(c.collapsed_width==170||c.collapsed_width==196)c.collapsed_width=228;
    if(c.collapsed_height==34||c.collapsed_height==38)c.collapsed_height=37;
    if(c.expanded_width==430||c.expanded_width==440)c.expanded_width=465;
    if(c.expanded_height==154||c.expanded_height==260)c.expanded_height=334;
  }
  c.monitor_index = integer(json, "monitorIndex", c.monitor_index);
  c.horizontal_percent=std::clamp(integer(json,"horizontalPercent",50),0,100);
  c.hotkey_key=std::clamp(integer(json,"hotkeyKey",0x49),0,255);
  c.hotkey_modifiers=integer(json,"hotkeyModifiers",3)&15;
  c.top_gap = std::clamp(integer(json, "topGap", integer(json, "offsetY", c.top_gap)), 0, 40);
  c.auto_hide_fullscreen = boolean(json, "autoHideFullscreen", c.auto_hide_fullscreen);
  c.hover_expand = boolean(json, "hoverExpand", c.hover_expand);
  c.clipboard_history = boolean(json, "clipboardHistory", c.clipboard_history);
  c.start_with_windows = boolean(json, "startWithWindows", c.start_with_windows);
  c.reduced_motion = boolean(json, "reducedMotion", c.reduced_motion);
  c.smart_notifications=boolean(json,"smartNotifications",true);
  c.windows_notifications=boolean(json,"windowsNotifications",false);
  c.windows_notification_content=boolean(json,"windowsNotificationContent",true);
  c.clipboard_ttl_minutes=std::clamp(integer(json,"clipboardTtl",30),0,1440);
  c.expire_pinned_clips=boolean(json,"expirePinnedClips",false);
  c.reminder_notifications=boolean(json,"reminderNotifications",true);
  c.reminder_morning_hour=std::clamp(integer(json,"reminderMorningHour",8),0,23);
  c.theme_index = std::clamp(integer(json, "theme", 0), 0, 5);
  c.custom_accent = std::clamp(integer(json, "customAccent", -1), -1, 0xFFFFFF);
  c.opacity_percent = std::clamp(integer(json, "opacity", 100), 50, 100);
  c.shadow_percent = std::clamp(integer(json, "shadow", 65), 0, 100);
  c.animation_percent = std::clamp(integer(json, "animationSpeed", 100), 50, 200);
  c.animation_style=std::clamp(integer(json,"animationStyle",0),0,3);
  c.animation_duration_ms=std::clamp(integer(json,"animationDuration",300),120,600);
  c.last_page = std::clamp(integer(json, "lastPage", 0), 0, 7);
  c.island_shape=std::clamp(integer(json,"islandShape",0),0,1);
  c.remember_page = boolean(json, "rememberPage", true);
  c.follow_system_theme = boolean(json, "followSystemTheme", false);
  c.auto_collapse = boolean(json, "autoCollapse", true);
  c.pin_on_click = boolean(json, "pinOnClick", true);
  c.collapsed_width = std::clamp(c.collapsed_width, 180, 320);
  c.collapsed_height = std::clamp(c.collapsed_height, 32, 48);
  c.expanded_width = std::clamp(c.expanded_width, 465, 650);
  c.expanded_height = std::clamp(c.expanded_height, 334, 500);
  } catch (...) { return Config{}; }
  return c;
}

bool Config::save() const {
  auto previous=read_file(path());try{if(valid_json(previous)&&integer(previous,"version",0)>3)return false;}catch(...){return false;}
  std::error_code ec;
  std::filesystem::create_directories(path().parent_path(), ec);
  auto temporary=path();temporary+=L".tmp";
  std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
  if (!out) return false;
  out << "{\n"
      << "  \"version\": 3,\n"
      << "  \"collapsedWidth\": " << collapsed_width << ",\n"
      << "  \"collapsedHeight\": " << collapsed_height << ",\n"
      << "  \"expandedWidth\": " << expanded_width << ",\n"
      << "  \"expandedHeight\": " << expanded_height << ",\n"
      << "  \"monitorIndex\": " << monitor_index << ",\n"
      << "  \"horizontalPercent\": " << horizontal_percent << ",\n"
      << "  \"hotkeyKey\": " << hotkey_key << ",\n"
      << "  \"hotkeyModifiers\": " << hotkey_modifiers << ",\n"
      << "  \"topGap\": " << std::clamp(top_gap, 0, 40) << ",\n"
      << "  \"theme\": " << theme_index << ",\n"
      << "  \"customAccent\": " << custom_accent << ",\n"
      << "  \"opacity\": " << opacity_percent << ",\n"
      << "  \"shadow\": " << shadow_percent << ",\n"
      << "  \"animationSpeed\": " << animation_percent << ",\n"
      << "  \"animationStyle\": " << animation_style << ",\n"
      << "  \"animationDuration\": " << animation_duration_ms << ",\n"
      << "  \"lastPage\": " << last_page << ",\n"
      << "  \"islandShape\": " << island_shape << ",\n"
      << "  \"rememberPage\": " << (remember_page ? "true" : "false") << ",\n"
      << "  \"followSystemTheme\": " << (follow_system_theme ? "true" : "false") << ",\n"
      << "  \"autoCollapse\": " << (auto_collapse ? "true" : "false") << ",\n"
      << "  \"pinOnClick\": " << (pin_on_click ? "true" : "false") << ",\n"
      << "  \"autoHideFullscreen\": " << (auto_hide_fullscreen ? "true" : "false") << ",\n"
      << "  \"hoverExpand\": " << (hover_expand ? "true" : "false") << ",\n"
      << "  \"clipboardHistory\": " << (clipboard_history ? "true" : "false") << ",\n"
      << "  \"startWithWindows\": " << (start_with_windows ? "true" : "false") << ",\n"
      << "  \"reducedMotion\": " << (reduced_motion ? "true" : "false") << ",\n"
      << "  \"smartNotifications\": " << (smart_notifications ? "true" : "false") << ",\n"
      << "  \"windowsNotifications\": " << (windows_notifications ? "true" : "false") << ",\n"
      << "  \"windowsNotificationContent\": " << (windows_notification_content ? "true" : "false") << ",\n"
      << "  \"clipboardTtl\": " << clipboard_ttl_minutes << ",\n"
      << "  \"expirePinnedClips\": " << (expire_pinned_clips ? "true" : "false") << ",\n"
      << "  \"reminderNotifications\": " << (reminder_notifications ? "true" : "false") << ",\n"
      << "  \"reminderMorningHour\": " << reminder_morning_hour << "\n}\n";
  out.flush();bool ok=out.good();out.close();
  auto backup=path();backup+=L".bak";
  if(ok&&valid_json(previous)&&std::filesystem::exists(path(),ec)&&!CopyFileW(path().c_str(),backup.c_str(),FALSE))return false;
  return ok && MoveFileExW(temporary.c_str(),path().c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
}

bool set_startup(bool enabled) {
  HKEY key{};
  if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, nullptr,
                      0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) return false;
  bool ok{};
  if (enabled) {
    wchar_t exe[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring value = L"\"" + std::wstring(exe) + L"\"";
    wchar_t family[PACKAGE_FAMILY_NAME_MAX_LENGTH+1]{};UINT32 size=static_cast<UINT32>(std::size(family));
    if(GetCurrentPackageFamilyName(&size,family)==ERROR_SUCCESS){wchar_t directory[MAX_PATH]{};GetWindowsDirectoryW(directory,MAX_PATH);value=L"\""+std::wstring(directory)+L"\\explorer.exe\" shell:AppsFolder\\"+family+L"!App";}
    ok = RegSetValueExW(key, L"NativeIsland", 0, REG_SZ,
      reinterpret_cast<const BYTE*>(value.c_str()), static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
  } else {
    const auto result = RegDeleteValueW(key, L"NativeIsland");
    ok = result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND;
  }
  RegCloseKey(key);
  return ok;
}
}

