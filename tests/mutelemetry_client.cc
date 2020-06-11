// FIXME: rename to client!!!!

#include <muroute/funudp.h>
#include <muroute/subsystem.h>
#include <mutelemetry/mutelemetry.h>
#include <boost/program_options.hpp>
#include <iostream>

#include "../../muroute/include/muroute/mavlink2/common/mavlink.h"

using namespace std;
using namespace boost::program_options;
using namespace fflow;
using namespace mutelemetry;
using namespace mutelemetry_network;

message_handler_note_t mutelemetry_proto_handlers[] = {
    {fflow::proto::FLOWPROTO_ASYNC,
     [](uint8_t *ptr, size_t len,
        fflow::SparseAddress sa) -> fflow::pointprec_t {
       // TODO:
       assert(0);
       return 1.0;
     }},
};

void send_mavlink_message(RouteSystemPtr roster, mavlink_message_t &msg) {
  uint8_t buffer[1500];
  uint8_t *data = buffer;
  size_t len = mavlink_msg_to_send_buffer(data, &msg);

  // FIXME: bcast only
  // auto a = SparseAddress(0, fflow::proto::FLOWPROTO_ASYNC, 0);
  // roster->sendmavmsg(msg, {a});

  auto a = SparseAddress(0, 0, 0);
  roster->sendmavmsg(msg, {a});
}

void send_ack(RouteSystemPtr roster, int cmd, int sys_id, int comp_id,
              bool success) {
  mavlink_message_t msg;

  mavlink_msg_command_ack_pack(
      sys_id, comp_id, &msg, cmd,
      success ? MAV_RESULT_ACCEPTED : MAV_RESULT_FAILED, 0 /*progress*/,
      0 /*result_param2*/, 0 /*target_system*/, 255 /*target_component*/);

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

  const string iface = varmap["iface"].as<std::string>();
  const uint32_t port = varmap["port"].as<uint32_t>();

  RouteSystemPtr roster = nullptr;
  shared_ptr<AbstractEdgeInterface> udptr = nullptr;

  udptr = createEdgeFunctionByName("EdgeUdp");

  bool open_res = iface == ""
                      ? (dynamic_cast<EdgeUdp *>(udptr.get()))->open(port)
                      : udptr->open(iface, port);
  if (!open_res) {
    cout << "Bad interface" << endl;
    return 1;
  }

  roster = RouteSystem::createRouteSys();
  roster->setMcastId(1);  // ?
  roster->setMcompId(1);  // ?
  roster->add_edge_transport(udptr);
  roster->add_protocol2(mutelemetry_proto_handlers,
                        sizeof(mutelemetry_proto_handlers) /
                            sizeof(mutelemetry_proto_handlers[0]));

  send_ack(roster, MAV_CMD_LOGGING_START, 1, 1, true);

  pause();

  return 0;
}
