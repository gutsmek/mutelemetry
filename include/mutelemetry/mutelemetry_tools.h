#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>
#include <stack>
#include <vector>

namespace mutelemetry_tools {

using SerializedData = std::vector<uint8_t>;
using SerializedDataPtr = std::shared_ptr<SerializedData>;

template <typename T>
class ConcQueue {
 public:
  ConcQueue() = default;
  virtual ~ConcQueue() = default;

  template <typename... Args>
  void enqueue(Args &&... args) {
    addData_protected([&] { queue_.emplace(std::forward<Args>(args)...); });
  }

  T dequeue(void) noexcept {
    std::unique_lock<std::mutex> lock{mutex_};
    while (queue_.empty()) {
      condNewData_.wait(lock);
    }
    auto elem = std::move(queue_.front());
    queue_.pop();
    return elem;
  }

  size_t size(void) const {
    std::lock_guard<std::mutex> lock{mutex_};
    return queue_.size();
  }

  bool empty(void) const {
    std::lock_guard<std::mutex> lock{mutex_};
    return queue_.empty();
  }

 private:
  template <class F>
  void addData_protected(F &&fct) {
    std::unique_lock<std::mutex> lock{mutex_};
    fct();
    lock.unlock();
    condNewData_.notify_one();
  }

  std::queue<T> queue_;
  mutable std::mutex mutex_;
  std::condition_variable condNewData_;
};

template <typename T>
class ConcStack {
 public:
  ConcStack() = default;
  virtual ~ConcStack() = default;

  template <typename... Args>
  void push(Args &&... args) {
    addData_protected([&] { stack_.emplace(std::forward<Args>(args)...); });
  }

  T pop(void) noexcept {
    std::unique_lock<std::mutex> lock{mutex_};
    while (stack_.empty()) {
      condNewData_.wait(lock);
    }
    auto elem = std::move(stack_.top());
    stack_.pop();
    return elem;
  }

  size_t size(void) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stack_.size();
  }

  bool empty(void) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stack_.empty();
  }

 private:
  template <class F>
  void addData_protected(F &&fct) {
    std::unique_lock<std::mutex> lock{mutex_};
    fct();
    lock.unlock();
    condNewData_.notify_one();
  }

  std::stack<T> stack_;
  mutable std::mutex mutex_;
  std::condition_variable condNewData_;
};

}  // namespace mutelemetry_tools
