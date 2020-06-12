#pragma once

#include <muroute/subsystem.h>
#include <atomic>
#include <functional>
#include "muroute/mavlink2/common/mavlink.h"
#include "mutelemetry/mutelemetry_tools.h"

using namespace std::placeholders;

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

  // protocol definition
 private:
  fflow::pointprec_t proto_command_handler(uint8_t *, size_t,
                                           fflow::SparseAddress);

  fflow::pointprec_t proto_command_ack_handler(uint8_t *, size_t,
                                               fflow::SparseAddress);

  size_t proto_table_len = 2;

  fflow::message_handler_note_t proto_table[2] = {
      {MAVLINK_MSG_ID_COMMAND_LONG,
       std::bind(&MutelemetryStreamer::proto_command_handler, this, _1, _2,
                 _3)},
      {MAVLINK_MSG_ID_LOGGING_ACK /* #268 */,
       [this](uint8_t *a, size_t b,
              fflow::SparseAddress c) -> fflow::pointprec_t {
         return proto_command_ack_handler(a, b, c);
       }},
  };
};

}  // namespace mutelemetry_network
