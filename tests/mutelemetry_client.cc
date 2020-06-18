#include <muroute/funudp.h>
#include <muroute/subsystem.h>
#include <mutelemetry/mutelemetry.h>
#include <atomic>
#include <boost/program_options.hpp>
#include <iostream>
#include <set>

#include "muroute/mavlink2/common/mavlink.h"

using namespace std;
using namespace boost::program_options;
using namespace fflow;
using namespace mutelemetry;
using namespace mutelemetry_network;
using namespace mutelemetry_tools;

void send_mavlink_message(mavlink_message_t &msg, bool bcast);

RouteSystemPtr roster = nullptr;
set<uint16_t> sequence_keeper;
atomic<bool> is_connected(false);
uint64_t msg_cntr = 0;

message_handler_note_t mutelemetry_proto_handlers[] = {
    {MAVLINK_MSG_ID_COMMAND_LONG,
     [](uint8_t *payload, size_t len,
        fflow::SparseAddress sa) -> fflow::pointprec_t {
       mavlink_message_t *rxmsg = MAVPAYLOAD_TO_MAVMSG(payload);
       mavlink_command_long_t cmd;
       mavlink_msg_command_long_decode(rxmsg, &cmd);
       // TODO:
       assert(0);
       return 1.0;
     }},
    {MAVLINK_MSG_ID_COMMAND_ACK /* #77 */,
     [](uint8_t *payload, size_t len,
        fflow::SparseAddress sa) -> fflow::pointprec_t {
       mavlink_message_t *rxmsg = MAVPAYLOAD_TO_MAVMSG(payload);
       mavlink_command_ack_t ack;
       mavlink_msg_command_ack_decode(rxmsg, &ack);

       if (ack.result == MAV_RESULT_ACCEPTED &&
           ack.result_param2 == MutelemetryStreamer::get_port()) {
         cout << "Connected to server" << endl;
         // assert(is_connected);
         is_connected = true;
       }

       return 1.0;
     }},
    {MAVLINK_MSG_ID_LOGGING_DATA /* #266 */,
     [](uint8_t *payload, size_t len,
        fflow::SparseAddress sa) -> fflow::pointprec_t {
       mavlink_message_t *rxmsg = MAVPAYLOAD_TO_MAVMSG(payload);
       mavlink_logging_data_t logd;
       mavlink_msg_logging_data_decode(rxmsg, &logd);

       cout << "ULog message num " << ++msg_cntr << " received" << endl;
       const uint8_t *buffer = logd.data;
       if (!check_ulog_valid(buffer))
         cout << "Validity check failed for data" << endl;

       // TODO: send to stdout using parser user-defined handlers
       // and/or flush to disk

       return 1.0;
     }},
    {MAVLINK_MSG_ID_LOGGING_DATA_ACKED /* #267 */,
     [](uint8_t *payload, size_t len,
        fflow::SparseAddress sa) -> fflow::pointprec_t {
       mavlink_message_t *rxmsg = MAVPAYLOAD_TO_MAVMSG(payload);
       mavlink_logging_data_acked_t logda;
       mavlink_msg_logging_data_acked_decode(rxmsg, &logda);

       uint16_t sequence = logda.sequence;
       bool is_new = false;
       if (sequence_keeper.find(sequence) == sequence_keeper.end()) {
         sequence_keeper.insert(sequence);
         is_new = true;
       }

       const uint8_t *buffer = logda.data;
       if (is_new) {
         if (!check_ulog_valid(buffer)) {
           cout << "Validity check failed for definitions" << endl;
           assert(0);
         } else {
           // TODO: flush to disk
         }
       }

       mavlink_logging_ack_t logging_ack;
       logging_ack.sequence = sequence;
       logging_ack.target_system = sa.group_id;
       logging_ack.target_component = sa.instance_id;

       mavlink_message_t msg;
       mavlink_msg_logging_ack_encode(roster->getMcastId(),
                                      roster->getMcompId(), &msg, &logging_ack);
       send_mavlink_message(msg, false);

       return 1.0;
     }},
};

void send_mavlink_message(mavlink_message_t &msg, bool bcast) {
  uint8_t buffer[500];
  uint8_t *data = buffer;
  size_t len = mavlink_msg_to_send_buffer(data, &msg);

  // send to our multicast group only
  uint32_t comp_id = bcast ? 0 : roster->getMcompId();
  auto a = SparseAddress(roster->getMcastId(), comp_id, 0);
  roster->sendmavmsg(msg, {a});
}

void command_start_logging() {
  mavlink_message_t msg;

  int sys_id = roster->getMcastId();
  int comp_id = roster->getMcompId();

  mavlink_msg_command_long_pack(sys_id, comp_id, &msg, sys_id, 0,
                                MAV_CMD_LOGGING_START, 0, 0.0, 0.0, 0.0, 0.0,
                                0.0, 0.0, 0.0);

  send_mavlink_message(msg, true);
}

// FIXME:
void connect() {
  command_start_logging();
  sleep(1);
  while (!is_connected) {
    command_start_logging();
    sleep(1);
  }
}

void command_stop_logging(RouteSystemPtr roster) {
  mavlink_message_t msg;

  int sys_id = roster->getMcastId();
  int comp_id = roster->getMcompId();

  mavlink_msg_command_long_pack(sys_id, comp_id, &msg, sys_id, 0,
                                MAV_CMD_LOGGING_STOP, 0, 0.0, 0.0, 0.0, 0.0,
                                0.0, 0.0, 0.0);

  send_mavlink_message(msg, false);
}

void setup_roster(shared_ptr<AbstractEdgeInterface> udptr) {
  roster = RouteSystem::createRouteSys();
  roster->setMcastId(1);  // ?
  roster->setMcompId(1);  // ?
  roster->add_edge_transport(udptr);
  roster->add_protocol2(mutelemetry_proto_handlers,
                        sizeof(mutelemetry_proto_handlers) /
                            sizeof(mutelemetry_proto_handlers[0]));
}

int main(int argc, char **argv) {
  options_description options("Mutelemetry client allowed options");
  options.add_options()("help", "Print help")
      //
      ("iface", value<std::string>()->default_value(""),
       "Interface to use for logging")
      //
      ("port",
       value<uint32_t>()->default_value(MutelemetryStreamer::get_port()),
       "UDP port");

  variables_map varmap;
  store(parse_command_line(argc, argv, options, command_line_style::unix_style),
        varmap);
  notify(varmap);

  if (varmap.count("help") > 0) {
    cout << options << "\n";
    return 1;
  }

  const string iface = varmap["iface"].as<string>();
  const uint32_t port = varmap["port"].as<uint32_t>();

  shared_ptr<AbstractEdgeInterface> udptr = createEdgeFunctionByName("EdgeUdp");

  bool open_res = iface == ""
                      ? (dynamic_cast<EdgeUdp *>(udptr.get()))->open(port)
                      : udptr->open(iface, port);
  if (!open_res) {
    cout << "Bad interface" << endl;
    return 1;
  }

  setup_roster(udptr);
  connect();
  pause();

  return 0;
}
