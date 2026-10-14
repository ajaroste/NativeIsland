#pragma once
#include <array>
namespace ni {
enum class Icon { grid, files, music, clipboard, settings, cpu, gpu, ram, disk, network, previous, play, pause, next, close, reveal, plus, bluetooth, volume, microphone, battery, pin, headphones, bell, check, tasks, pencil };
// Lucide vector geometry, bundled locally. See assets/LUCIDE-LICENSE.txt.
inline constexpr std::array<const char*,27> icon_shapes{{
  "<rect x='3' y='3' width='7' height='7' rx='1'/><rect x='14' y='3' width='7' height='7' rx='1'/><rect x='3' y='14' width='7' height='7' rx='1'/><rect x='14' y='14' width='7' height='7' rx='1'/>",
  "<path d='M15 2h-4a2 2 0 0 0-2 2v11a2 2 0 0 0 2 2h8a2 2 0 0 0 2-2V8'/><path d='M15 2v5a1 1 0 0 0 1 1h5L15 2'/><path d='M5 7a2 2 0 0 0-2 2v11a2 2 0 0 0 2 2h8'/>",
  "<path d='M9 18V5l12-2v13'/><circle cx='6' cy='18' r='3'/><circle cx='18' cy='16' r='3'/>",
  "<rect width='8' height='4' x='8' y='2' rx='1'/><path d='M16 4h2a2 2 0 0 1 2 2v14a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2V6a2 2 0 0 1 2-2h2'/>",
  "<path d='M9.671 4.136a2.34 2.34 0 0 1 4.659 0 2.34 2.34 0 0 0 3.319 1.915 2.34 2.34 0 0 1 2.33 4.033 2.34 2.34 0 0 0 0 3.831 2.34 2.34 0 0 1-2.33 4.033 2.34 2.34 0 0 0-3.319 1.915 2.34 2.34 0 0 1-4.659 0 2.34 2.34 0 0 0-3.32-1.915 2.34 2.34 0 0 1-2.33-4.033 2.34 2.34 0 0 0 0-3.831A2.34 2.34 0 0 1 6.35 6.051a2.34 2.34 0 0 0 3.319-1.915'/><circle cx='12' cy='12' r='3'/>",
  "<rect x='4' y='4' width='16' height='16' rx='2'/><rect x='8' y='8' width='8' height='8' rx='1'/><path d='M7 2v2M12 2v2M17 2v2M7 20v2M12 20v2M17 20v2M2 7h2M2 12h2M2 17h2M20 7h2M20 12h2M20 17h2'/>",
  "<rect x='2' y='3' width='20' height='14' rx='2'/><path d='M8 21h8M12 17v4'/>",
  "<rect x='2' y='6' width='20' height='10' rx='2'/><path d='M4 18v-2M8 18v-2M12 18v-2M16 18v-2M20 18v-2M8 12v-2M12 12v-2M16 12v-2'/>",
  "<path d='M2 12 6 4h12l4 8v6a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2zM2 12h20M6 16h.01M10 16h.01'/>",
  "<path d='m3 16 4 4 4-4M7 20V4m14 4-4-4-4 4M17 4v16'/>",
  "<path d='M21 4 7 12l14 8zM3 20V4'/>",
  "<path d='m5 3 14 9-14 9z'/>",
  "<rect x='14' y='3' width='5' height='18' rx='1'/><rect x='5' y='3' width='5' height='18' rx='1'/>",
  "<path d='m3 4 14 8-14 8zM21 4v16'/>",
  "<path d='M18 6 6 18M6 6l12 12'/>",
  "<path d='M15 3h6v6M10 14 21 3M18 13v6a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2h6'/>",
  "<path d='M12 5v14M5 12h14'/>",
  "<path d='m7 7 10 10-5 5V2l5 5L7 17'/>",
  "<path d='M11 5 6 9H3v6h3l5 4V5M15 8a6 6 0 0 1 0 8M18 5a10 10 0 0 1 0 14'/>",
  "<rect x='9' y='2' width='6' height='12' rx='3'/><path d='M5 10v2a7 7 0 0 0 14 0v-2M12 19v3M8 22h8'/>",
  "<rect x='2' y='6' width='18' height='12' rx='2'/><path d='M22 10v4M6 10v4M10 10v4'/>",
  "<path d='m12 3 3 6 6 1-4.5 4.5 1 6.5-5.5-3-5.5 3 1-6.5L3 10l6-1z'/>",
  "<path d='M3 14v-3a9 9 0 0 1 18 0v3'/><rect x='3' y='12' width='4' height='9' rx='2'/><rect x='17' y='12' width='4' height='9' rx='2'/>",
  "<path d='M18 8a6 6 0 0 0-12 0c0 7-3 7-3 9h18c0-2-3-2-3-9M10 21h4'/>",
  "<path d='m5 12 4 4L19 6'/>",
  "<path d='m3 6 2 2 4-4M13 6h8m-18 9 2 2 4-4M13 15h8M13 20h8'/>",
  "<path d='m16 3 5 5M3 21l5-1L21 7l-4-4L4 16z'/>"
}};
}
