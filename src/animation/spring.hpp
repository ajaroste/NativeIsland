#pragma once
#include <algorithm>
#include <cmath>

namespace ni {
class Spring {
public:
  void reset(float value) noexcept { value_ = target_ = value; velocity_ = 0.0f; }
  void target(float value) noexcept { target_ = value; }
  float value() const noexcept { return value_; }
  bool settled() const noexcept { return std::abs(value_ - target_) < 0.08f && std::abs(velocity_) < 0.08f; }
  float update(float dt) noexcept {
    dt = std::clamp(dt, 0.0f, 0.033f);
    constexpr float stiffness = 310.0f;
    constexpr float damping = 30.0f;
    velocity_ += (stiffness * (target_ - value_) - damping * velocity_) * dt;
    value_ += velocity_ * dt;
    if (settled()) { value_ = target_; velocity_ = 0.0f; }
    return value_;
  }
private:
  float value_{};
  float target_{};
  float velocity_{};
};
}

