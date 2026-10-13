#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace ni {
struct SystemSnapshot {
  bool ready{};
  bool gpu_available{};
  double gpu_percent{};
  double cpu_percent{};
  std::uint64_t ram_used{};
  std::uint64_t ram_total{};
  std::uint64_t disk_used{}, disk_total{};
  std::vector<std::wstring> bluetooth_audio;
  double download_bytes_per_sec{};
  double upload_bytes_per_sec{};
  std::wstring adapter{L"Veri yok"};
  std::uint64_t uptime_seconds{};
};

class SystemMonitor {
public:
  using Callback = std::function<void()>;
  ~SystemMonitor();
  void start(Callback callback);
  void stop();
  SystemSnapshot snapshot() const;
  void set_visible(bool visible){visible_=visible;}
private:
  void run(std::stop_token stop);
  mutable std::mutex mutex_;
  SystemSnapshot snapshot_;
  Callback callback_;
  std::jthread worker_;
  std::atomic<bool> visible_{true};
};
}

