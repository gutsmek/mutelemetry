#include <muroute/funudp.h>
#include <muroute/subsystem.h>
#include <mutelemetry/mutelemetry.h>
#include <boost/program_options.hpp>
#include <iostream>

#include "muroute/mavlink2/common/mavlink.h"

using namespace std;
using namespace boost::program_options;
using namespace fflow;
using namespace mutelemetry;
using namespace mutelemetry_network;

message_handler_note_t mutelemetry_proto_handlers[] = {
    {MAVLINK_MSG_ID_COMMAND_LONG,
     [](uint8_t *payload, size_t len,
        fflow::SparseAddress sa) -> fflow::pointprec_t {
       // TODO:
       mavlink_message_t *rxmsg = MAVPAYLOAD_TO_MAVMSG(payload);
       mavlink_command_long_t cmd;
       mavlink_msg_command_long_decode(rxmsg, &cmd);
       assert(0);
       return 1.0;
     }},
    {MAVLINK_MSG_ID_COMMAND_ACK,
     [](uint8_t *payload, size_t len,
        fflow::SparseAddress sa) -> fflow::pointprec_t {
       // TODO:
       mavlink_message_t *rxmsg = MAVPAYLOAD_TO_MAVMSG(payload);
       mavlink_command_ack_t ack;
       mavlink_msg_command_ack_decode(rxmsg, &ack);

       assert(0);
       return 1.0;
     }},
    {MAVLINK_MSG_ID_LOGGING_DATA /* #266 */,
     [](uint8_t *payload, size_t len,
        fflow::SparseAddress sa) -> fflow::pointprec_t {
       // TODO:
       mavlink_message_t *rxmsg = MAVPAYLOAD_TO_MAVMSG(payload);
       mavlink_logging_data_t logd;
       mavlink_msg_logging_data_decode(rxmsg, &logd);
       assert(0);
       return 1.0;
     }},
    {MAVLINK_MSG_ID_LOGGING_DATA_ACKED /* #267 */,
     [](uint8_t *payload, size_t len,
        fflow::SparseAddress sa) -> fflow::pointprec_t {
       // TODO:
       mavlink_message_t *rxmsg = MAVPAYLOAD_TO_MAVMSG(payload);
       mavlink_logging_data_acked_t logda;
       mavlink_msg_logging_data_acked_decode(rxmsg, &logda);
       assert(0);
       return 1.0;
     }},
};

void send_mavlink_message(RouteSystemPtr roster, mavlink_message_t &msg) {
  uint8_t buffer[500];
  uint8_t *data = buffer;
  size_t len = mavlink_msg_to_send_buffer(data, &msg);

  // send to our multicast group only
  auto a = SparseAddress(roster->getMcastId(), 0, 0);
  roster->sendmavmsg(msg, {a});
}

void command_start_logging(RouteSystemPtr roster, int cmd, bool success) {
  mavlink_message_t msg;

  int sys_id = roster->getMcastId();
  int comp_id = roster->getMcompId();

  mavlink_msg_command_long_pack(sys_id, comp_id, &msg, sys_id, 0,
                                MAV_CMD_LOGGING_START, 0, 0.0, 0.0, 0.0, 0.0,
                                0.0, 0.0, 0.0);

  send_mavlink_message(roster, msg);
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

  RouteSystemPtr roster = RouteSystem::createRouteSys();
  roster->setMcastId(1);  // ?
  roster->setMcompId(1);  // ?
  roster->add_edge_transport(udptr);
  roster->add_protocol2(mutelemetry_proto_handlers,
                        sizeof(mutelemetry_proto_handlers) /
                            sizeof(mutelemetry_proto_handlers[0]));

  // just a test of connection
  command_start_logging(roster, MAV_CMD_LOGGING_START, true);

  pause();

  return 0;
}
