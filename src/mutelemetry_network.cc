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

//#define MAVPAYLOAD_TO_MAVMSG(payload) \
//  ((mavlink_message_t *)(payload - offsetof(mavlink_message_t, payload64)))

fflow::pointprec_t MutelemetryStreamer::proto_command_handler(
    uint8_t *payload, size_t len, fflow::SparseAddress sa) {
  mavlink_message_t *rxmsg = MAVPAYLOAD_TO_MAVMSG(payload);
  mavlink_command_long_t lcmd;
  mavlink_msg_command_long_decode(rxmsg, &lcmd);

  // ACKNOWLEDGE
  mavlink_command_ack_t ack;
  ack.command = lcmd.command;
  ack.result = MAV_RESULT_ACCEPTED;

  std::shared_ptr<mavlink_message_t> msg =
      std::make_shared<mavlink_message_t>();
  mavlink_msg_command_ack_encode(roster_->getMcastId(), roster_->getMcompId(),
                                 &(*msg), &ack);
  uint32_t targetMcastId = sa.group_id;
  uint32_t targetCompId = sa.instance_id;
  fflow::post_function<void>(
      [msg, targetMcastId, targetCompId, this](void) -> void {
        roster_->sendmavmsg(
            *msg, {fflow::SparseAddress(targetMcastId, targetCompId, 0)});
      });

  assert(0);
#if 0
       const DataHeader *hdr = reinterpret_cast<const DataHeader *>(ptr);
       if (isValidHashType(hdr->type_)) {
         handleData(hdr);
       } else {
         cout << "$$$ Invalid data received $$$" << endl;
       }
#endif
  return 1.0;
}

fflow::pointprec_t MutelemetryStreamer::proto_command_ack_handler(
    uint8_t *payload, size_t len, fflow::SparseAddress sa) {
  mavlink_message_t *rxmsg = MAVPAYLOAD_TO_MAVMSG(payload);
  mavlink_command_ack_t ack;
  mavlink_msg_command_ack_decode(rxmsg, &ack);
  return 1.0;
}

bool MutelemetryStreamer::init(
    RouteSystemPtr roster,
    mutelemetry_tools::ConcQueue<mutelemetry_tools::SerializedDataPtr>
        *data_queue) {
  if (running_ || roster_ != nullptr || data_queue_ != nullptr) return false;
  if (roster == nullptr || data_queue == nullptr) return false;

  roster->add_protocol2(proto_table, proto_table_len);

  roster_ = roster;
  data_queue_ = data_queue;
  return true;
}

void MutelemetryStreamer::sync_loop() {
  assert(running_ == false);

  while (true) {
    if (data_queue_->empty()) {
      this_thread::sleep_for(chrono::milliseconds(100));
      continue;
    }

    auto dp = data_queue_->front();
    const uint8_t *buffer = dp->data();
    if (check_ulog_data_begin(buffer)) break;

    data_queue_->dequeue();

    // TODO:
  }

  running_ = true;
}

void MutelemetryStreamer::main_loop() {
  while (running_ && !data_queue_->empty()) {
    auto dp = data_queue_->dequeue();
    assert(dp != nullptr);
    // TODO:
  }
}

void MutelemetryStreamer::run() {
  post_function<void>([&](void) -> void { sync_loop(); });
  this_thread::sleep_for(chrono::milliseconds(100));
  add_periodic<void>(([&](void) -> void { main_loop(); }), 0.000001, 0.1);
}
