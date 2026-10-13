#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
namespace ni {
struct AudioOutput {std::wstring id,name;bool operator==(const AudioOutput&)const=default;};
struct AudioSession {std::wstring id,name;float volume{};bool muted{};std::wstring app_key;bool favorite{};bool operator==(const AudioSession&)const=default;};
struct AudioSnapshot {
  bool available{},mic_available{},muted{},mic_muted{},output_switch_supported{};
  float volume{};
  std::wstring output_id,output_name,mic_name,error;
  std::vector<AudioOutput> outputs;
  std::vector<AudioSession> sessions;
};
class AudioService {
public:
  ~AudioService(){stop();}
  void start(std::function<void()> changed);
  void stop();
  AudioSnapshot snapshot()const;
  void volume(float value);
  void mute(bool microphone,bool value);
  void output(std::wstring id);
  void session_volume(std::wstring id,float value);
  void session_mute(std::wstring id,bool value);
private:
  struct Command {int type{};float value{};std::wstring id;};
  void queue(Command command);
  void run(std::stop_token stop);
  mutable std::mutex mutex_;std::condition_variable_any cv_;
  std::vector<Command> commands_;AudioSnapshot snapshot_;std::jthread worker_;std::function<void()> changed_;
};
}
