#pragma once
#include <algorithm>
#include <string_view>

namespace ni {
// Windows does not supply notification layout measurements. Keep small alerts
// narrow, and reserve two preview lines when message text is long.
struct CompactNoticeSize {int width{};int height{};};

inline int notice_approx_width(std::wstring_view text,int latin_advance){
  int total=0;
  for(wchar_t ch:text){
    total+=(ch>=0x1100?2:1)*latin_advance;
    if(total>1024)break;
  }
  return total;
}
inline CompactNoticeSize compact_notice_size(std::wstring_view source,
                                             std::wstring_view title,
                                             std::wstring_view body){
  const int heading=std::max(notice_approx_width(source,7),notice_approx_width(title,9));
  const int preview=std::min(notice_approx_width(body,8),380);
  const int width=std::clamp(std::max(heading,preview)+112,336,500);
  const bool wrap=!body.empty()&&notice_approx_width(body,8)>width-90;
  return {width,body.empty()?76:(wrap?120:96)};
}
}
