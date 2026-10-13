#pragma once
#include "core/config.hpp"
#include "ui/theme.hpp"
#include "services/app_launcher.hpp"
#include "services/clipboard_entry.hpp"
#include "services/audio_service.hpp"
#include "services/reminders.hpp"
#include "services/google_calendar.hpp"
#include <d2d1.h>
#include <vector>
#include <string>
#include <memory>
#include <cstdint>

namespace ni {
inline constexpr int shadow_padding=16;
enum class Page { widgets, files, media, clipboard, applications, reminders, sound, tasks };
enum class Action { tab, settings, previous, play, next, seek, reveal_file, remove_file,
                    copy_clip, remove_clip, toggle_clip, clear_clip, scroll_up, scroll_down,
                    drag_file, add_app, launch_app, remove_app, preview_clip, pin_clip,search_clips,volume,mute_output,mute_mic,select_output,sound_settings,manage_reminders,complete_reminder,reminder_day,session_volume,session_mute,add_task,complete_task,select_task_list,refresh_tasks,favorite_audio,delete_task };
struct HitTarget { D2D1_RECT_F rect; Action action; size_t index{}; bool enabled{true};std::wstring key; };
struct ViewModel {
  AudioSnapshot audio;
  GoogleCalendarStatus google;
  int island_shape{};
  std::vector<Reminder> reminders;
  std::vector<Reminder> pending_today_reminders;
  int reminder_offset{};
  int64_t reminder_date{};
  bool has_today_reminders{};
  std::wstring calendar_status;
  std::wstring clipboard_query,notice;
  std::wstring notice_app,notice_title,notice_body;
  std::shared_ptr<const std::vector<std::uint8_t>> notice_logo;
  std::vector<size_t> clipboard_indices;
  int notice_kind{};
  float notice_age{},notice_value{};
  bool reduced_motion{};
  Page page{Page::widgets};
  Theme theme{theme_for(0)};
  float opacity{1};
  float shadow{.65f};
  float page_opacity{1};
  float expansion{};
  float wave_phase{},wave_mix{};
  float height{252};
  float content_width{500};
  bool clipboard_enabled{};
  bool drop_active{};
  size_t scroll{};
  int hovered{-1};
  std::vector<std::wstring> files;
  std::vector<ClipboardEntry> clipboard;
  std::vector<AppShortcut> applications;
};
}
