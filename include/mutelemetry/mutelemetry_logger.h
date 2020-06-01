#pragma once

#include "mutelemetry/mutelemetry_tools.h"

#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace mutelemetry_logger {

class MutelemetryLogger {
  class DataBuffer {
   public:
    DataBuffer() : data_size_(0) {}
    DataBuffer(const DataBuffer &) = delete;
    DataBuffer &operator=(const DataBuffer &) = delete;
    virtual ~DataBuffer() = default;

   public:
    bool add(mutelemetry_tools::SerializedDataPtr dp) {
      bool is_full = false;
      size_t data_size = dp->size();
      if (data_size + data_size_ >= max_data_size_) is_full = true;
      if (has_data()) add_started_ = std::chrono::system_clock::now();
      data_size_ += data_size;
      buffer_.emplace_back(dp);
      return is_full;
    }

    bool can_start_io() const {
      if (!has_data()) return false;
      double time_sec = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now() - add_started_)
                            .count();
      return time_sec >= can_start_io_interval_;
    }

    mutelemetry_tools::SerializedDataPtr operator[](size_t idx) {
      if (idx >= buffer_.size()) return nullptr;
      return buffer_[idx];
    }

    inline bool has_data() const { return data_size_ > 0; }
    inline size_t size() const { return data_size_; }

    mutelemetry_tools::SerializedData data() const {
      mutelemetry_tools::SerializedData result;
      size_t total = buffer_.size();
      for (size_t i = 0; i < total; ++i) {
        mutelemetry_tools::SerializedDataPtr dp = buffer_[i];
        result.insert(result.end(), dp->begin(), dp->end());
      }
      return result;
    }

    inline void clear() {
      buffer_.clear();
      data_size_ = 0;
    }

   public:
    static constexpr size_t max_data_size_ = 4096;
    static constexpr double can_start_io_interval_ = 1.0;  // 1 sec

   private:
    std::chrono::time_point<std::chrono::system_clock> add_started_;
    size_t data_size_;
    std::vector<mutelemetry_tools::SerializedDataPtr> buffer_;
  };

 public:
  MutelemetryLogger();
  MutelemetryLogger(const MutelemetryLogger &) = delete;
  MutelemetryLogger &operator=(const MutelemetryLogger &) = delete;

  virtual ~MutelemetryLogger() { release(); }

 public:
  const inline std::string &get_logname() const { return filename_; }

  void run();
  bool init(
      const std::string &,
      mutelemetry_tools::ConcQueue<mutelemetry_tools::SerializedDataPtr> *);
  void release(bool on_file_error = false);

 private:
  inline void flush() {
    if (running_) file_.flush();
  }

  void start_io_worker(DataBuffer *, bool do_flush = false);

 private:
  std::atomic<bool> running_;
  mutelemetry_tools::ConcQueue<mutelemetry_tools::SerializedDataPtr>
      *data_queue_;
  std::string filename_;
  std::ofstream file_;
  std::array<DataBuffer, 8> mem_pool_;
  mutelemetry_tools::ConcStack<DataBuffer *> pool_stacked_index_;
  DataBuffer *curr_idx_;
  mutable std::mutex mutex_;
};

}  // namespace mutelemetry_logger
