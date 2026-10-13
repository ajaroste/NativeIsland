#pragma once
#include "services/audio_service.hpp"
#include "services/reminders.hpp"
#include "core/config.hpp"
#include <algorithm>
#include <fstream>
#include <set>
#include <cwctype>
namespace ni {
inline std::wstring audio_app_key(std::wstring path){std::transform(path.begin(),path.end(),path.begin(),[](wchar_t c){return static_cast<wchar_t>(std::towlower(c));});return path;}
class AudioFavorites {
public:
  explicit AudioFavorites(std::filesystem::path path=Config::path().parent_path()/L"audio-favorites.json"):path_(std::move(path)){}
  void load(){try{std::ifstream in(path_,std::ios::binary|std::ios::ate);if(!in||in.tellg()>256*1024)return;in.seekg(0);std::string bytes{std::istreambuf_iterator<char>(in),{}};auto root=winrt::Windows::Data::Json::JsonObject::Parse(winrt::to_hstring(bytes));std::set<std::wstring> next;for(auto value:root.GetNamedArray(L"applications")){auto key=audio_app_key(std::wstring(value.GetString()));if(!key.empty()&&key.size()<=32768&&next.size()<256)next.insert(std::move(key));}keys_=std::move(next);}catch(...){}}
  bool toggle(const std::wstring& app){if(app.empty()||app.size()>32768)return false;auto next=keys_;auto key=audio_app_key(app);if(next.contains(key))next.erase(key);else{if(next.size()>=256)return false;next.insert(key);}try{using namespace winrt::Windows::Data::Json;JsonObject root;JsonArray values;for(auto& id:next)values.Append(JsonValue::CreateStringValue(id));root.Insert(L"applications",values);auto bytes=winrt::to_string(root.Stringify());if(bytes.size()>256*1024||!atomic_user_file(path_,bytes))return false;keys_=std::move(next);return true;}catch(...){return false;}}
  void apply(AudioSnapshot& audio)const{for(auto& s:audio.sessions)s.favorite=!s.app_key.empty()&&keys_.contains(audio_app_key(s.app_key));std::stable_sort(audio.sessions.begin(),audio.sessions.end(),[](const auto& a,const auto& b){return a.favorite>b.favorite;});}
private:
  std::filesystem::path path_;std::set<std::wstring> keys_;
};
}
