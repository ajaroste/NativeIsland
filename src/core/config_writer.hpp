#pragma once
#include "core/config.hpp"
#include "core/diagnostics.hpp"
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>
namespace ni {
class ConfigWriter {
public:
  ConfigWriter():worker_([this](std::stop_token stop){
    for(;;) {
      std::unique_lock lock(mutex_);cv_.wait(lock,stop,[this]{return pending_.has_value();});
      if(!pending_)return;
      Config value=*pending_;pending_.reset();lock.unlock();guarded_service("settings-write-exception",[&]{value.save();});
    }
  }){}
  ~ConfigWriter(){worker_.request_stop();cv_.notify_all();}
  void enqueue(const Config& value){std::scoped_lock lock(mutex_);pending_=value;cv_.notify_one();}
private:
  std::mutex mutex_;std::condition_variable_any cv_;std::optional<Config> pending_;std::jthread worker_;
};
}
