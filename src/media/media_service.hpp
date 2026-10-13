#pragma once
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <cstdint>

namespace ni {
struct MediaSnapshot {
  std::wstring title,artist,source;
  bool playing{},available{},can_play{},can_previous{},can_next{},can_seek{};
  double position{},duration{};
  std::vector<std::uint8_t> art_pixels;
  unsigned art_width{},art_height{};
  std::uint64_t art_version{};
};
class MediaService {
public:
  using Callback = std::function<void()>;
  ~MediaService();
  void start(Callback callback);
  void stop();
  MediaSnapshot snapshot() const;
  void play_pause();
  void next();
  void previous();
  void seek(double seconds);
  void set_visible(bool value){visible_=value;}
private:
  enum class Command { none, play_pause, next, previous, seek };
  void run(std::stop_token stop);
  mutable std::mutex mutex_;
  MediaSnapshot snapshot_;
  Callback callback_;
  std::atomic<Command> command_{Command::none};
  std::atomic<double> seek_seconds_{};
  std::jthread worker_;
  std::atomic<bool> visible_{true};
};
}

