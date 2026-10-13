#pragma once
#include <array>
#include <cstdint>
#include <algorithm>

namespace ni {
struct Theme {
  const wchar_t* name;
  std::uint32_t background, surface, elevated, border, text, secondary, accent, accent_soft;
};
inline constexpr std::array<Theme, 6> themes{{
  {L"Zümrüt",0x050608,0x171A20,0x242A31,0x30343C,0xF6F7F9,0xA4AAB3,0x72E6AA,0x183B2A},
  {L"Okyanus",0x071725,0x102B3F,0x1C394C,0x24465C,0xF0F8FF,0x9FB8CC,0x56C9FF,0x123A50},
  {L"Mor",0x140F24,0x241C38,0x302746,0x45375A,0xF9F4FF,0xBBADCF,0xBF94FF,0x332044},
  {L"Gül",0x211015,0x311B25,0x3A2630,0x5A3447,0xFFF6FA,0xD5AABB,0xFF8AAC,0x47202F},
  {L"Altın",0x1C1710,0x30281B,0x3B3022,0x54432B,0xFFF8ED,0xCDBCA2,0xFFCB71,0x42311B},
  {L"Açık",0xFBFCFF,0xEEF1F7,0xE5EBF3,0xCDD5E1,0x1A2432,0x687789,0x0875D1,0xDBEFFF}
}};
inline Theme theme_for(int index, int accent = -1) {
  auto result = themes[std::clamp(index, 0, 5)];
  if (accent >= 0 && accent <= 0xFFFFFF) {
    result.accent=static_cast<std::uint32_t>(accent);result.accent_soft=0;
    for(int shift:{0,8,16}) {
      unsigned channel=static_cast<unsigned>(((result.background>>shift)&255)*.82+((result.accent>>shift)&255)*.18);
      result.accent_soft|=channel<<shift;
    }
  }
  return result;
}
}
