#pragma once

#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <fstream>
#include <memory>
#include <mutex>
#include <stack>
#include <string>
#include <vector>

#include "mutelemetry/mutelemetry_tools.h"

namespace mutelemetry_logger {

class MutelemetryLogger {
  class DataBuffer {
   public:
    DataBuffer() : data_size_(0), is_full_(false) {}
    DataBuffer(const DataBuffer &) = delete;
    DataBuffer &operator=(const DataBuffer &) = delete;
    virtual ~DataBuffer() = default;

   public:
    bool add(mutelemetry_tools::SerializedDataPtr dp) {
      size_t data_size = dp->size();
      std::lock_guard<std::mutex> lock(mutex_);
      if (!has_data()) add_started_ = std::chrono::system_clock::now();
      if (data_size + data_size_ >= max_data_size_) is_full_ = true;
      data_size_ += data_size;
      buffer_.emplace_back(dp);
      return is_full_;
    }

    bool can_start_io() const {
      if (!has_data()) return false;
      double time_sec = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now() - add_started_)
                            .count();
      return time_sec >= can_start_io_interval_;
    }

    mutelemetry_tools::SerializedDataPtr operator[](size_t idx) {
      std::lock_guard<std::mutex> lock(mutex_);
      if (idx >= buffer_.size()) return nullptr;
      return buffer_[idx];
    }

    inline bool has_data() const { return data_size_ > 0; }
    inline size_t size() const { return data_size_; }
    inline bool full() const { return is_full_; }

    mutelemetry_tools::SerializedData data() const {
      mutelemetry_tools::SerializedData result;
      std::lock_guard<std::mutex> lock(mutex_);
      size_t total = buffer_.size();
      for (size_t i = 0; i < total; ++i) {
        mutelemetry_tools::SerializedDataPtr dp = buffer_[i];
        result.insert(result.end(), dp->begin(), dp->end());
      }
      return result;
    }

    inline void clear() {
      std::lock_guard<std::mutex> lock(mutex_);
      buffer_.clear();
      data_size_ = 0;
      is_full_ = false;
    }

   public:
    static constexpr size_t max_data_size_ = 4096;
    static constexpr double can_start_io_interval_ = 1.0;  // 1 sec

   private:
    std::chrono::time_point<std::chrono::system_clock> add_started_;
    size_t data_size_;
    bool is_full_;
    std::vector<mutelemetry_tools::SerializedDataPtr> buffer_;
    mutable std::mutex mutex_;
  };

 public:
  MutelemetryLogger();
  MutelemetryLogger(const MutelemetryLogger &) = delete;
  MutelemetryLogger &operator=(const MutelemetryLogger &) = delete;

  virtual ~MutelemetryLogger() {
    stop();
    release();
  }

 public:
  const inline std::string &get_logname() const { return filename_; }

  void start();
  void stop() { running_ = false; }
  bool init(
      const std::string &,
      mutelemetry_tools::ConcQueue<mutelemetry_tools::SerializedDataPtr> *);
  void release(bool on_file_error = false);

 private:
  inline void flush() {
    if (file_.is_open()) file_.flush();
  }

  inline bool write(const mutelemetry_tools::SerializedData &result) {
    std::lock_guard<std::mutex> lock(io_mutex_);
    file_.write(reinterpret_cast<const char *>(result.data()), result.size());
    return !file_.bad();
  }

  void main_loop();
  void start_io_worker(DataBuffer *, bool do_flush = false);

 private:
  std::atomic<bool> running_;
  mutelemetry_tools::ConcQueue<mutelemetry_tools::SerializedDataPtr>
      *data_queue_;
  std::string filename_;
  std::ofstream file_;
  std::array<DataBuffer, 8> mem_pool_;
  std::stack<DataBuffer *> pool_stacked_index_;
  std::atomic<DataBuffer *> curr_idx_;
  mutable std::mutex idx_mutex_;
  mutable std::mutex io_mutex_;
};

}  // namespace mutelemetry_logger
