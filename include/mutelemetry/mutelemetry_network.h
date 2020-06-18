#pragma once

#include <muroute/subsystem.h>
#include <atomic>
#include <functional>
#include "muroute/mavlink2/common/mavlink.h"
#include "mutelemetry/mutelemetry_tools.h"

#define CHECK_PARSE_VALIDITY_STREAMER

namespace mutelemetry_network {

class MutelemetryStreamer {
 private:
  enum class StreamerState : uint8_t {
    STATE_INIT = 0,
    STATE_CONNECTED,
    STATE_SEND_DEF,
    STATE_RESEND_DEF,
    STATE_ACK_RECV,
    STATE_ACK_WAIT,
    STATE_RUNNING
  };

  friend std::ostream &operator<<(std::ostream &o, const StreamerState &s) {
    static const std::string states[] = {"INIT",       "CONNECTED", "SEND_DEF",
                                         "RESEND_DEF", "ACK_RECV",  "ACK_WAIT",
                                         "RUNNING"};
    if (s <= StreamerState::STATE_RUNNING)
      o << states[uint8_t(s)];
    else
      o << "UNKNOWN";
    return o;
  }

 public:
  static constexpr uint32_t port = 7788;
  static uint32_t get_port() { return port; }

 public:
  MutelemetryStreamer()
      : discarding_(false),
        running_(false),
        roster_(nullptr),
        data_queue_(nullptr),
        state_(StreamerState::STATE_INIT),
        seq_(0),
        target_system_(0),
        target_component_(0),
        try_connect_cntr_(0),
        sync_timeout_(0),
        send_cntr_(0) {}
  MutelemetryStreamer(const MutelemetryStreamer &) = delete;
  MutelemetryStreamer &operator=(const MutelemetryStreamer &) = delete;

  virtual ~MutelemetryStreamer() { release(); }

 public:
  // FIXME: make start/stop interface
  void run(bool rt);
  bool init(
      fflow::RouteSystemPtr,
      mutelemetry_tools::ConcQueue<mutelemetry_tools::SerializedDataPtr> *);
  void release() {}

 private:
  void sync_loop();
  void discard_loop();
  void main_loop();

 private:
  std::atomic<bool> discarding_;
  std::atomic<bool> running_;
  fflow::RouteSystemPtr roster_;
  mutelemetry_tools::ConcQueue<mutelemetry_tools::SerializedDataPtr>
      *data_queue_;
  std::atomic<StreamerState> state_;
  size_t seq_;
  std::vector<mutelemetry_tools::SerializedDataPtr> definitions_;
#ifdef CHECK_PARSE_VALIDITY_STREAMER
  // only for debug since ULog parser is not thread-safe
  std::mutex parser_mutex_;
#endif

  // protocol definition
 private:
  fflow::pointprec_t proto_command_handler(uint8_t *, size_t,
                                           fflow::SparseAddress);
  fflow::pointprec_t proto_logging_ack_handler(uint8_t *, size_t,
                                               fflow::SparseAddress);

  uint8_t target_system_;
  uint8_t target_component_;
  uint8_t try_connect_cntr_;
  uint16_t sync_timeout_;
  std::atomic<uint64_t> send_cntr_;

  static constexpr size_t proto_table_len = 2;

  fflow::message_handler_note_t proto_table[proto_table_len] = {
      {MAVLINK_MSG_ID_COMMAND_LONG /* #76 */,
       std::bind(&MutelemetryStreamer::proto_command_handler, this,
                 std::placeholders::_1, std::placeholders::_2,
                 std::placeholders::_3)},
      {MAVLINK_MSG_ID_LOGGING_ACK /* #268 */,
       [this](uint8_t *a, size_t b,
              fflow::SparseAddress c) -> fflow::pointprec_t {
         return proto_logging_ack_handler(a, b, c);
       }},
  };

  bool send_ulog(const uint8_t *, size_t);
  bool send_ulog_ack(const uint8_t *, size_t, uint16_t);
  inline bool set_state(StreamerState old_state, StreamerState new_state) {
    return state_.compare_exchange_strong(old_state, new_state);
  }
};

}  // namespace mutelemetry_network
