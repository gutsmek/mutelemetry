#include "mutelemetry/mutelemetry_network.h"

#include <glog/logging.h>
#include <muqueue/erqperiodic.h>
#include <muqueue/scheduler.h>
#include <cassert>
#include <chrono>
#include <thread>

#include "mutelemetry/mutelemetry_tools.h"

using namespace std;
using namespace fflow;
using namespace mutelemetry_network;
using namespace mutelemetry_tools;

fflow::pointprec_t MutelemetryStreamer::proto_command_handler(
    uint8_t *payload, size_t len, fflow::SparseAddress sa) {
  mavlink_message_t *rxmsg = MAVPAYLOAD_TO_MAVMSG(payload);
  mavlink_command_long_t lcmd;
  mavlink_msg_command_long_decode(rxmsg, &lcmd);

  uint32_t targetMcastId = sa.group_id;
  uint32_t targetCompId = sa.instance_id;

  //  LOG(INFO) << "Command (telem) : " << targetMcastId << "  to "
  //            << uint32_t(lcmd.target_system) << " cmd:" <<
  //            uint32_t(lcmd.command)
  //            << " from:" << uint32_t(targetCompId) << std::endl;
  //  assert(targetMcastId == lcmd.target_system);
  // assert(targetCompId == lcmd.target_component);

  // ACKNOWLEDGE
  mavlink_command_ack_t ack;
  ack.command = lcmd.command;
  ack.result = MAV_RESULT_ACCEPTED;
  if (state_ == StreamerState::STATE_INIT &&
      lcmd.command == MAV_CMD_LOGGING_START) {
    ack.result_param2 = MutelemetryStreamer::get_port();
    ack.progress = ack.result;
    ack.target_system = targetMcastId;    // lcmd.target_system;
    ack.target_component = targetCompId;  // lcmd.target_component;

    std::shared_ptr<mavlink_message_t> msg =
        std::make_shared<mavlink_message_t>();
    mavlink_msg_command_ack_encode(roster_->getMcastId(), roster_->getMcompId(),
                                   &(*msg), &ack);

    fflow::post_function<void>([msg, targetMcastId, targetCompId,
                                this](void) -> void {
      roster_->sendmavmsg(
          *msg, {fflow::SparseAddress(targetMcastId, targetCompId, 0)});
      if (state_ == StreamerState::STATE_INIT) {
        ++try_connect_cntr_;
        LOG(INFO) << ">>>>> Connection attempt " << uint32_t(try_connect_cntr_);
        // FIXME: temporal workaround for connection establishment
        // if (try_connect_cntr_ == 2) {
        target_system_ = targetMcastId;
        target_component_ = targetCompId;
        set_state(state_, StreamerState::STATE_CONNECTED);
        LOG(INFO) << "Connection established, state changed to " << state_;
        //}
      }
    });
  }

  return 1.0;
}

fflow::pointprec_t MutelemetryStreamer::proto_logging_ack_handler(
    uint8_t *payload, size_t len, fflow::SparseAddress sa) {
  mavlink_message_t *rxmsg = MAVPAYLOAD_TO_MAVMSG(payload);
  mavlink_logging_ack_t logging_ack;
  mavlink_msg_logging_ack_decode(rxmsg, &logging_ack);

  StreamerState state = state_.load();

  switch (state) {
    case StreamerState::STATE_ACK_WAIT: {
      StreamerState new_state = logging_ack.sequence == (uint16_t)seq_
                                    ? StreamerState::STATE_ACK_RECV
                                    : StreamerState::STATE_RESEND_DEF;
      bool result = set_state(state, new_state);
      if (!result) {
        LOG(ERROR) << "Another thread has changed the state: "
                   << "Was => " << state << " Now => " << state_.load()
                   << " Must be => " << new_state;
        assert(0);
      }
    } break;

    default:
      LOG(INFO) << "Receiving logging ack message in " << state << " state";
      break;
  }

  return 1.0;
}

bool MutelemetryStreamer::init(RouteSystemPtr roster,
                               ConcQueue<SerializedDataPtr> *data_queue) {
  if (running_ || roster_ != nullptr || data_queue_ != nullptr) return false;
  if (roster == nullptr || data_queue == nullptr) return false;

  roster->add_protocol2(proto_table, proto_table_len);

  roster_ = roster;
  data_queue_ = data_queue;
  return true;
}

bool MutelemetryStreamer::send_ulog(const uint8_t *data, size_t data_len) {
  if (data_len > MAVLINK_MSG_LOGGING_DATA_FIELD_DATA_LEN) {
    LOG(ERROR) << "Message size is too big: " << data_len
               << " bytes while max possible is "
               << MAVLINK_MSG_LOGGING_DATA_FIELD_DATA_LEN << " bytes";
    return false;
  }

  std::shared_ptr<mavlink_message_t> msg =
      std::make_shared<mavlink_message_t>();

  uint16_t msg_len = mavlink_msg_logging_data_pack(
      roster_->getMcastId(), roster_->getMcompId(), &(*msg), target_system_,
      target_component_, 0, uint8_t(data_len), 255, data);
  (void)msg_len;
  fflow::post_function<void>([msg, this](void) -> void {
    roster_->sendmavmsg(
        *msg, {fflow::SparseAddress(target_system_, target_component_, 0)});
  });

  return true;
}

bool MutelemetryStreamer::send_ulog_ack(const uint8_t *data, size_t data_len,
                                        uint16_t seq) {
  if (data_len > MAVLINK_MSG_LOGGING_DATA_ACKED_FIELD_DATA_LEN) {
    LOG(ERROR) << "Message size is too big: " << data_len
               << " bytes while max possible is "
               << MAVLINK_MSG_LOGGING_DATA_ACKED_FIELD_DATA_LEN << " bytes";
    return false;
  }

  // LOG(INFO) << "Send ULog definition message of size=" << data_len
  //          << " with seq=" << uint32_t(seq);

  mavlink_message_t msg;
  uint16_t msg_len = mavlink_msg_logging_data_acked_pack(
      roster_->getMcastId(), roster_->getMcompId(), &msg, target_system_,
      target_component_, seq, uint8_t(data_len), 255, data);
  (void)msg_len;
  roster_->sendmavmsg(
      msg, {fflow::SparseAddress(target_system_, target_component_, 0)});

  return true;
}

void MutelemetryStreamer::sync_loop() {
  LOG(INFO) << "Sync loop started";
  assert(running_ == false);

  while (true) {
    if (data_queue_->empty()) {
      this_thread::sleep_for(chrono::milliseconds(50));
      continue;
    }

    auto dp = data_queue_->front();
    const uint8_t *buffer = dp->data();

#ifdef CHECK_PARSE_VALIDITY_STREAMER
    if (!check_ulog_valid(buffer)) {
      LOG(ERROR) << "Validity check failed for definitions";
      assert(0);
    }
#endif

    if (check_ulog_data_begin(buffer)) {
      // start discarding ulog data until all definitions are sent,
      // this is needed to keep up to time data on client side
      LOG(INFO) << "Definitions section size = " << definitions_.size();
      discarding_ = true;
      break;
    }

    data_queue_->dequeue();
    definitions_.emplace_back(dp);
  }

  size_t defs_sz = definitions_.size();

  while (seq_ < defs_sz) {
    StreamerState state = state_.load();

    //    LOG(INFO) << "State is " << state;

    switch (state) {
      case StreamerState::STATE_CONNECTED:
        assert(target_system_ != 0);
        assert(target_component_ != 0);
      case StreamerState::STATE_RESEND_DEF:
      case StreamerState::STATE_SEND_DEF: {
        SerializedDataPtr dp = definitions_[seq_];
        if (!send_ulog_ack(dp->data(), dp->size(), uint16_t(seq_))) {
          LOG(ERROR) << "Failed to send ULog definition message";
          assert(0);
          break;
        }
        sync_timeout_ = 0;
        StreamerState new_state = StreamerState::STATE_ACK_WAIT;
        bool result = set_state(state, new_state);
        if (!result) {
          LOG(ERROR) << "Another thread has changed the state: "
                     << "Was => " << state << " Now => " << state_
                     << " Must be => " << new_state;
          assert(0);
        }
      } break;

      case StreamerState::STATE_ACK_RECV: {
        StreamerState new_state = StreamerState::STATE_SEND_DEF;
        bool result = set_state(state, new_state);
        if (!result) {
          LOG(ERROR) << "Another thread has changed the state: "
                     << "Was => " << state << " Now => " << state_
                     << " Must be => " << new_state;
          assert(0);
        }
        seq_++;
      } break;

      case StreamerState::STATE_INIT:
        this_thread::sleep_for(chrono::milliseconds(50));
        break;

      case StreamerState::STATE_ACK_WAIT:
        this_thread::sleep_for(chrono::milliseconds(50));
        sync_timeout_ += 50;
        if (sync_timeout_ == 1000) {
          // sync timeout (approx. 1s) reached, resend the last message
          StreamerState new_state = StreamerState::STATE_RESEND_DEF;
          bool result = set_state(state, new_state);
          if (!result) {
            LOG(ERROR) << "Another thread has changed the state: "
                       << "Was => " << state << " Now => " << state_
                       << " Must be => " << new_state;
            assert(0);
          }
        }
        break;

      default:
        LOG(ERROR) << "UNKNOWN state";
        assert(0);
    }
  }

  set_state(state_.load(), StreamerState::STATE_RUNNING);
  running_ = true;
  LOG(INFO) << "Sync loop exited";
}

void MutelemetryStreamer::discard_loop() {
  while (!discarding_) this_thread::sleep_for(chrono::milliseconds(50));
  LOG(INFO) << "Discard loop started";
  while (!running_) data_queue_->dequeue();
  LOG(INFO) << "Discard loop exited";
}

void MutelemetryStreamer::main_loop() {
  if (!running_) return;

  LOG(INFO) << "State is " << state_;
  assert(state_ == StreamerState::STATE_RUNNING);

  // LOG(INFO) << "Main loop started";
  while (!data_queue_->empty()) {
    auto dp = data_queue_->dequeue();
    assert(dp != nullptr);

    const uint8_t *buffer = dp->data();
#ifdef CHECK_PARSE_VALIDITY_STREAMER
    {
      std::lock_guard<std::mutex> lock(parser_mutex_);
      if (!check_ulog_valid(buffer)) {
        LOG(ERROR) << "Validity check failed for data";
        assert(0);
      }
    }
#endif

    // send ULog data without acknowledgement
    if (!send_ulog(buffer, dp->size())) {
      // LOG(INFO) << "ULog data won't fit into mavlink buffer, skipping";
    } else {
      LOG(INFO) << "ULog packet " << ++send_cntr_ << " sent";
    }
  }
  // LOG(INFO) << "Main loop exited";
}

void MutelemetryStreamer::run(bool rt) {
  post_function<void>([&](void) -> void { sync_loop(); });
  if (rt) post_function<void>([&](void) -> void { discard_loop(); });
  add_periodic<void>(([&](void) -> void { main_loop(); }), 0.000001, 0.1);
}
