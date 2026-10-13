#pragma once
#include <unknwn.h>
#include <utility>

namespace ni {
template <class T> class ComPtr {
public:
  ComPtr() = default;
  ~ComPtr() { reset(); }
  ComPtr(const ComPtr&) = delete;
  ComPtr& operator=(const ComPtr&) = delete;
  ComPtr(ComPtr&& other) noexcept : value_(std::exchange(other.value_, nullptr)) {}
  ComPtr& operator=(ComPtr&& other) noexcept {
    if (this != &other) { reset(); value_ = std::exchange(other.value_, nullptr); }
    return *this;
  }
  T* get() const noexcept { return value_; }
  T** put() noexcept { reset(); return &value_; }
  T* operator->() const noexcept { return value_; }
  explicit operator bool() const noexcept { return value_ != nullptr; }
  void reset(T* value = nullptr) noexcept { if (value_) value_->Release(); value_ = value; }
  template<class U> HRESULT as(ComPtr<U>& out) const noexcept {
    return value_ ? value_->QueryInterface(__uuidof(U), reinterpret_cast<void**>(out.put())) : E_POINTER;
  }
private:
  T* value_{};
};
}

