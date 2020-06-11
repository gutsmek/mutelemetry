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

message_handler_note_t mutelemetry_proto_handlers[] = {
    {fflow::proto::FLOWPROTO_ASYNC,
     [](uint8_t *ptr, size_t len,
        fflow::SparseAddress sa) -> fflow::pointprec_t {
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
     }},
};

bool MutelemetryStreamer::init(
    RouteSystemPtr roster,
    mutelemetry_tools::ConcQueue<mutelemetry_tools::SerializedDataPtr>
        *data_queue) {
  if (running_ || roster_ != nullptr || data_queue_ != nullptr) return false;
  if (roster == nullptr || data_queue == nullptr) return false;

  roster->add_protocol2(mutelemetry_proto_handlers,
                        sizeof(mutelemetry_proto_handlers) /
                            sizeof(mutelemetry_proto_handlers[0]));

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
