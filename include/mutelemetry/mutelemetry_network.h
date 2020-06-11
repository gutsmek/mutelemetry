#pragma once

#include <muroute/subsystem.h>
#include <atomic>

#include "mutelemetry/mutelemetry_tools.h"

namespace mutelemetry_network {

class MutelemetryStreamer {
 public:
  static constexpr uint32_t port = 7788;
  static uint32_t get_port() { return port; }

 public:
  MutelemetryStreamer()
      : running_(false), roster_(nullptr), data_queue_(nullptr) {}
  MutelemetryStreamer(const MutelemetryStreamer &) = delete;
  MutelemetryStreamer &operator=(const MutelemetryStreamer &) = delete;

  virtual ~MutelemetryStreamer() { release(); }

 public:
  // FIXME: make start/stop interface
  void run();
  bool init(
      fflow::RouteSystemPtr,
      mutelemetry_tools::ConcQueue<mutelemetry_tools::SerializedDataPtr> *);
  void release() {}

 private:
  void sync_loop();
  void main_loop();

 private:
  std::atomic<bool> running_;
  fflow::RouteSystemPtr roster_;
  mutelemetry_tools::ConcQueue<mutelemetry_tools::SerializedDataPtr>
      *data_queue_;
};

}  // namespace mutelemetry_network
