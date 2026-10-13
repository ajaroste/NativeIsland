#pragma once
#include "services/clipboard_entry.hpp"
#include <cwctype>
namespace ni {
inline std::vector<size_t> clipboard_matches(const std::vector<ClipboardEntry>& entries,const std::wstring& query){
  std::vector<size_t> result;for(size_t i=entries.size();i>0;--i){auto& entry=entries[i-1];std::wstring label=entry.image()?L"görsel image":entry.text;
    if(query.empty()||FindNLSStringEx(LOCALE_NAME_USER_DEFAULT,FIND_FROMSTART|NORM_IGNORECASE,label.data(),static_cast<int>(label.size()),query.data(),static_cast<int>(query.size()),nullptr,nullptr,nullptr,0)>=0)result.push_back(i-1);}
  std::stable_sort(result.begin(),result.end(),[&](size_t a,size_t b){return entries[a].pinned&&!entries[b].pinned;});return result;
}
inline bool expire_clipboard(std::vector<ClipboardEntry>& entries,ULONGLONG now,int minutes,bool pinned_too){
  if(minutes==0)return false;size_t before=entries.size();std::erase_if(entries,[&](auto& entry){return (!entry.pinned||pinned_too)&&now>=entry.created_at&&now-entry.created_at>=static_cast<ULONGLONG>(minutes)*60000;});return before!=entries.size();
}
}
