#pragma once
#include <algorithm>
#include <string_view>

namespace ni {
// Windows does not supply notification layout measurements. Keep small alerts
// narrow, with up to three readable preview lines for longer texts.
struct CompactNoticeSize {int width{};int height{};};
struct NoticeImageLayout {float source_left{},text_left{};bool portrait_visible{};};
// One text column remains stable while the application logo loads asynchronously.
inline NoticeImageLayout notice_image_layout(bool portrait_available,bool){const float left=portrait_available?76.f:56.f;return {left,left,portrait_available};}

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
                                             std::wstring_view body,bool portrait_available=false){
  const int heading=std::max(notice_approx_width(source,7),notice_approx_width(title,9));
  const int preview=std::min(notice_approx_width(body,8),380);
  const int width=std::clamp(std::max(heading,preview)+(portrait_available?112:90),336,500);
  const int body_width=notice_approx_width(body,8);
  const int line_width=width-(portrait_available?110:90);
  if(body.empty())return {width,portrait_available?96:76};
  if(body_width<=line_width)return {width,96};
  if(body_width<=line_width*2)return {width,120};
  return {width,142};
}
}
