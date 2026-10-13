#pragma once
#include <algorithm>

namespace ni::layout {
constexpr int scale_dip(int dip, unsigned dpi) noexcept {
  return static_cast<int>((static_cast<long long>(dip) * dpi + 48) / 96);
}
constexpr int centered_left(int monitor_left, int monitor_width, int window_width) noexcept {
  return monitor_left + (monitor_width - window_width) / 2;
}
constexpr int top_gap(int requested) noexcept { return std::clamp(requested, 0, 40); }
}
