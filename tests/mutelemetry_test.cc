#include <assert.h>
#include <boost/program_options.hpp>
#include <future>
#include <sstream>
#include <thread>
#include <utility>

#include <muroute/funudp.h>
#include <muroute/subsystem.h>
#include <mutelemetry/mutelemetry.h>

#include "test_types.h"

using namespace std;
using namespace fflow;
using namespace mutelemetry;
using namespace mutelemetry_ulog;
using namespace mutelemetry_network;
using namespace boost::program_options;

class DataType0Serializable : public Serializable, public DataType0 {
 public:
  vector<uint8_t> serialize() override {
    stringstream ss;
    ss << name() << " serialization ";  //<< fixed << setprecision(4) << *this;
    LOG(INFO) << ss.str();
    return DataType0::serialize();
  }
};

class DataType1Serializable : public Serializable, public DataType1 {
 public:
  vector<uint8_t> serialize() override {
    uint64_t timestamp = 0;
    vector<uint8_t> serialized(sizeof(timestamp) + sizeof(DataType1));
    stringstream ss;
    ss << name() << " serialization ";  //<< fixed << setprecision(4) << *this;
    LOG(INFO) << ss.str();
    size_t len = sizeof(timestamp);
    memcpy(&serialized[0], &timestamp, len);
    memcpy(&serialized[len], a, sizeof(a));
    len += sizeof(a);
    memcpy(&serialized[len], b, sizeof(b));
    assert(serialized.size() == sizeof(b) + len);
    LOG(INFO) << name() << " serialization finished";
    return serialized;
  }
};

vector<uint8_t> DataType2_serialize(const DataType2 &d2) {
  size_t len = 0;
  uint64_t timestamp = 0;
  size_t v_size = sizeof(d2.a[0]);
  size_t new_size = sizeof(timestamp) + d2.a.size() * v_size;
  vector<uint8_t> serialized(new_size);

  memcpy(&serialized[0], &timestamp, v_size);
  len += sizeof(timestamp);

  for (auto v : d2.a) {
    memcpy(&serialized[len], &v, v_size);
    len += v_size;
  }

  for (const string &v : d2.b) {
    v_size = v.length();
    new_size += v_size;
    serialized.resize(new_size);
    memcpy(&serialized[len], v.c_str(), v_size);
    len += v_size;
  }

  serialized.emplace_back((uint8_t)d2.c);
  return serialized;
}

vector<uint8_t> DataType3_serialize(const DataType3 &d3) {
  uint64_t timestamp = 0;
  vector<uint8_t> serialized(sizeof(timestamp));

  memcpy(&serialized[0], &timestamp, sizeof(timestamp));

  for (DataType0 v : d3.a) {
    vector<uint8_t> a_serialized = v.serialize();
    serialized.insert(serialized.end(), a_serialized.begin(),
                      a_serialized.end());
  }

  DataType1Serializable d1;
  memcpy(d1.a, d3.b.a, sizeof(d1.a));
  memcpy(d1.b, d3.b.b, sizeof(d1.b));
  vector<uint8_t> b_serialized = d1.serialize();
  serialized.insert(serialized.end(), b_serialized.begin(), b_serialized.end());

  vector<uint8_t> c_serialized = DataType2_serialize(d3.c);
  serialized.insert(serialized.end(), c_serialized.begin(), c_serialized.end());

  return serialized;
}

vector<uint8_t> DataType4_serialize(const DataType4 &d4) {
  uint64_t timestamp = 0;
  vector<uint8_t> serialized(sizeof(timestamp));
  stringstream ss;
  ss << TOSTR(DataType4)
     << " serialization ";  //<< fixed << setprecision(4) << d;
  LOG(INFO) << ss.str();

  memcpy(&serialized[0], &timestamp, sizeof(timestamp));

  for (int i = 0; i < d4.size(); ++i) {
    vector<uint8_t> d3_serialized = DataType3_serialize(d4[i]);
    serialized.insert(serialized.end(), d3_serialized.begin(),
                      d3_serialized.end());
  }
  LOG(INFO) << TOSTR(DataType4) << " external serialization finished";
  return serialized;
}

using RResult = pair<bool, thread::id>;
using RResultFutures = vector<future<RResult>>;

struct ThreadInternals {
  chrono::milliseconds delay_;
  int n_repeats_;
  ThreadInternals() = default;
  ThreadInternals(chrono::milliseconds delay, int n_repeats)
      : delay_(delay), n_repeats_(n_repeats) {}
};

const vector<ThreadInternals> params = {
    {chrono::milliseconds(20), 100},  // 0
    {chrono::milliseconds(50), 80},   // 1
    {chrono::milliseconds(100), 50},  // 2
    {chrono::milliseconds(150), 20},  // 3
    {chrono::milliseconds(500), 20},  // 4
};

auto generator = [](int type) {
  bool ret = true;
  int iteration = 0;
  RResult result{false, this_thread::get_id()};

  LOG(INFO) << "Starting Thread [" << get<1>(result) << "] for type " << type
            << endl;

  try {
    int n_repeats = params[type].n_repeats_;
    while (n_repeats-- > 0 && ret) {
      switch (type) {
        case 0: {
          ret = MuTelemetry::getInstance().store_data(
              shared_ptr<Serializable>{new DataType0Serializable},
              DataType0::name(), "d01");
          if (ret) {
            ret = MuTelemetry::getInstance().store_data(
                shared_ptr<Serializable>{new DataType0Serializable},
                DataType0::name(), "d02");
          }
        } break;

        case 1:
          ret = MuTelemetry::getInstance().store_data(
              shared_ptr<Serializable>{new DataType1Serializable},
              DataType1::name(), "d1");
          if (ret) {
            ret = MuTelemetry::getInstance().store_data(
                shared_ptr<Serializable>{new DataType0Serializable},
                DataType0::name(), "d03");
          }
          break;

        case 2: {
          DataType2 d2;
          SerializerFunc s = [&d2]() {
            stringstream ss;
            ss << DataType2::name() << " serialization ";
            //<< fixed << setprecision(4) << d2;
            LOG(INFO) << ss.str();
            vector<uint8_t> serialized = DataType2_serialize(d2);
            LOG(INFO) << DataType2::name()
                      << " functional serialization finished" << endl;
            return serialized;
          };
          ret = MuTelemetry::getInstance().store_data(s, DataType2::name(),
                                                      TOSTR(d2));
        } break;

        case 3: {
          DataType3 d3;
          SerializerFunc s = [&d3]() {
            stringstream ss;
            ss << DataType3::name() << " serialization ";
            //<< fixed << setprecision(4) << d3;
            LOG(INFO) << ss.str();
            vector<uint8_t> serialized = DataType3_serialize(d3);
            LOG(INFO) << DataType3::name()
                      << " functional serialization finished" << endl;
            return serialized;
          };
          ret = MuTelemetry::getInstance().store_data(s, DataType3::name(),
                                                      TOSTR(d3));
        } break;

        case 4: {
          DataType4 d4;
          vector<uint8_t> d4_serialized = DataType4_serialize(d4);
          ret = MuTelemetry::getInstance().store_data(
              d4_serialized, TOSTR(DataType4), TOSTR(d4));
        } break;

        default:
          ret = false;
          break;
      }

      stringstream ss;
      ss << "Thread [" << get<1>(result) << "]: iteration: " << iteration++
         << " result: " << ret;
      MuTelemetry::getInstance().store_message(ss.str(), ULogLevel::Info);

      this_thread::sleep_for(chrono::milliseconds(params[type].delay_));
    }
  } catch (...) {
    stringstream ss;
    ret = false;
    ss << "Thread [" << get<1>(result) << "]: exception caught";
    MuTelemetry::getInstance().store_message(ss.str(), ULogLevel::Err);
  }

  get<0>(result) = ret;
  LOG(INFO) << "Thread [" << get<1>(result) << "] exited" << endl;
  return result;
};

int main(int argc, char **argv) {
  // google::InitGoogleLogging(argv[0]);

  options_description options("Allowed options");
  options.add_options()("help", "Print help")
      //
      ("iface", value<std::string>()->default_value(""),
       "Interface to use with ULog Streamer")
      //
      ("port",
       value<uint32_t>()->default_value(MutelemetryStreamer::get_port()),
       "UDP port of ULog Streamer")
      //
      ("threads", value<uint32_t>()->default_value(params.size()),
       "Number of threads to use in test");

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
  const uint32_t n_threads = varmap["threads"].as<uint32_t>();

  RouteSystemPtr roster = nullptr;
  shared_ptr<AbstractEdgeInterface> udptr = nullptr;

  if (iface != "") {
    udptr = createEdgeFunctionByName("EdgeUdp");
    if (udptr->open(iface, port)) {
      roster = RouteSystem::createRouteSys();
      roster->setMcastId(1);  // ?
      roster->setMcompId(1);  // ?
      roster->add_edge_transport(udptr);
      // protocol will be added by MuTelemetry
    }
  }

  MuTelemetry::init(roster);

  MuTelemetry &mt = MuTelemetry::getInstance();

  if (!mt.is_enabled()) return 1;

  mt.register_info("company", "EDEL LLC");
  mt.register_info("sys_name", "RBPi4");
  mt.register_info("replay", mt.get_logname());
  mt.register_param("int32_t param1", 123);
  mt.register_data_format(DataType0::name(), DataType0::fields());
  mt.register_data_format(DataType1::name(), DataType1::fields());
  mt.register_data_format(DataType2::name(), DataType2::fields());
  mt.register_data_format(DataType3::name(), DataType3::fields());
  mt.register_data_format("DataType4", "DataType3[3] array;");
  mt.register_param("float param2", -3.01f);

  RResultFutures futures(n_threads);

  for (uint32_t i = 0; i < n_threads; ++i)
    futures[i] = std::async(launch::async, generator, i % 5);

  for (auto &f : futures) {
    RResult result = f.get();
    if (!get<0>(result))
      LOG(ERROR) << "Thread [" << get<1>(result) << "] failed" << endl;
  }

  pause();

  return 0;
}
