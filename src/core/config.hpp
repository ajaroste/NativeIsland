#pragma once
#include <filesystem>
#include <string>

namespace ni {
struct Config {
  int collapsed_width{228};
  int collapsed_height{37};
  int expanded_width{465};
  int expanded_height{334};
  int monitor_index{0};
  int top_gap{7};
  int horizontal_percent{50};
  int hotkey_key{0x49};
  int hotkey_modifiers{3}; // Ctrl + Alt
  int theme_index{0};
  int custom_accent{-1};
  int opacity_percent{100};
  int shadow_percent{65};
  int animation_percent{100};
  int animation_style{0};
  int animation_duration_ms{300};
  int last_page{0};
  int island_shape{0}; // 0 capsule, 1 top-attached droplet
  bool remember_page{true};
  bool follow_system_theme{false};
  bool auto_collapse{true};
  bool pin_on_click{true};
  bool auto_hide_fullscreen{true};
  bool hover_expand{true};
  bool clipboard_history{false};
  bool start_with_windows{false};
  bool reduced_motion{false};
  bool smart_notifications{true};
  bool windows_notifications{false};
  bool windows_notification_content{true};
  int clipboard_ttl_minutes{30};
  bool expire_pinned_clips{false};
  bool reminder_notifications{true};
  int reminder_morning_hour{8};
  std::wstring language{L"auto"};

  static std::filesystem::path path();
  static Config load();
  static Config parse(const std::string& json);
  bool save() const;
};

bool set_startup(bool enabled);
}

