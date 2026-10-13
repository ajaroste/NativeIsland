#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <functional>
namespace ni {
  struct AppShortcut;
  AppShortcut shell_icon(std::wstring path);
struct AppShortcut {std::wstring path,name;std::vector<std::uint8_t> pixels;unsigned width{},height{};};
class AppLauncher {
public:
  ~AppLauncher();
  void start(std::function<void()> changed,bool persistence=true);
  void stop();
  void add(std::wstring path);
  void remove(size_t index);
  void launch(size_t index);
  std::vector<AppShortcut> snapshot()const;
private:
  void run(std::stop_token stop);
  std::vector<AppShortcut> apps_;
  mutable std::mutex mutex_;std::condition_variable_any cv_;
  std::vector<std::wstring> additions_,launches_;
  std::vector<size_t> removals_;
  std::jthread worker_;std::function<void()> changed_;bool persistence_{};
};
}
