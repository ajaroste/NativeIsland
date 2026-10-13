#pragma once
#include "animation/spring.hpp"
#include "animation/animation_manager.hpp"
#include "core/config.hpp"
#include "core/config_writer.hpp"
#include "media/media_service.hpp"
#include "rendering/dcomp_renderer.hpp"
#include "telemetry/system_monitor.hpp"
#include "ui/settings_window.hpp"
#include "files/drop_target.hpp"
#include "ui/search_window.hpp"
#include "ui/reminder_window.hpp"
#include "ui/task_entry_window.hpp"
#include "services/audio_favorites.hpp"
#include <optional>
#include <deque>
#include <windows.h>
#include <shellapi.h>
#include <chrono>
#include <string>
#include <vector>

namespace ni {
class IslandWindow {
public:
  explicit IslandWindow(HINSTANCE instance);
  ~IslandWindow();
  bool create(bool verification=false);
  int run();
  int export_previews(const std::filesystem::path& directory);
  int run_for(unsigned milliseconds);
private:
  enum class State { collapsed, expanding, expanded, collapsing, pinned, hidden };
  static LRESULT CALLBACK window_proc(HWND, UINT, WPARAM, LPARAM);
  void select_task_list();
  LRESULT handle(UINT, WPARAM, LPARAM);
  void render();
  void set_expanded(bool expanded, bool pin = false);
  void position();
  void animate(bool frame_ready=false);
  void schedule_collapse();
  bool pointer_inside() const;
  bool point_inside_capsule(POINT client) const;
  void show_menu(POINT point);
  void update_clipboard();
  void add_tray();
  void remove_tray();
  void check_fullscreen();
  void activate(const HitTarget& target, float x);
  int hit_at(POINT point) const;
  void apply_settings();
  int expanded_height() const;
  void resize_for_content();
  void add_application();
  void update_media_animation();
  void smart_events();
  void notify(std::wstring text,int kind,float value=0);
  void select_audio_output();
  void reminder_events();
  void show_reminder_notice();
  void open_reminders();
  void windows_notification_events();
  WindowsNotifications windows_notifications_;
  std::deque<WindowsNotice> windows_notice_queue_;
  uint32_t windows_notice_id_{};
  HINSTANCE instance_{}; HWND window_{}; UINT dpi_{96};
  TaskEntryWindow task_entry_;
  std::optional<HitTarget> pressed_target_;
  Config config_; ConfigWriter config_writer_; DCompRenderer renderer_; SystemMonitor monitor_; MediaService media_;
  Spring width_, height_; State state_{State::collapsed}; State state_before_hidden_{State::collapsed};
  bool expanded_{}; bool hidden_for_fullscreen_{};
  bool outgoing_drag_{};bool pointer_over_{};bool background_press_{};
  int anchor_center_x_{}; int anchor_top_{};
  POINT drag_origin_{};
  std::chrono::steady_clock::time_point last_frame_{};
  NOTIFYICONDATAW tray_{};
  std::vector<std::wstring> files_; std::vector<ClipboardEntry> clipboard_;
  bool manually_hidden_{},moving_{};
  int drag_center_{};
  bool verification_{};
  bool drawing_animation_{};
  bool menu_active_{};
  AudioService audio_;
  AudioFavorites audio_favorites_;
  AudioSnapshot previous_audio_;
  bool audio_baseline_{};
  int battery_warning_{};
  std::vector<std::wstring> previous_bluetooth_;
  bool bluetooth_baseline_{};
  std::chrono::steady_clock::time_point notice_start_{};
  std::chrono::steady_clock::time_point notice_deadline_{};
  SearchWindow search_;
  ReminderStore reminders_;
  GoogleCalendarService google_calendar_;
  ReminderWindow reminder_window_;
  std::deque<ReminderNotice> reminder_notices_;
  bool reminder_summary_open_{};
  uint64_t clipboard_revision_{},view_clipboard_revision_{~0ull};
  void toggle_hidden();
  ViewModel view_;
  AnimationManager animation_;
  SettingsWindow settings_;
  DropTarget* drop_target_{};
  AppLauncher apps_;
  float wave_target_{};
  bool waves_running_{};
  int planned_height_{};
  std::chrono::steady_clock::time_point wave_start_{},wave_frame_{};
  int pressed_hit_{-1};
  std::chrono::steady_clock::time_point page_transition_{};
};
}

