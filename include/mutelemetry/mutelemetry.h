/* ----------------------------------------------------------------------------
 *
 * Copyright 2020, EDEL LLC
 * All Rights Reserved
 *
 * See LICENSE for the license information
 *
 * ULog Telemetry - storage and streaming
 * -------------------------------------------------------------------------- */

#pragma once

#include <muroute/subsystem.h>
#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>

#include "mutelemetry_logger.h"
#include "mutelemetry_network.h"
#include "mutelemetry_tools.h"
#include "mutelemetry_ulog.h"

namespace mutelemetry {

using SerializerFunc = std::function<std::vector<uint8_t>()>;

class Serializable {
 public:
  virtual std::vector<uint8_t> serialize() = 0;
};

class MuTelemetry {
  static MuTelemetry instance_;

  template <typename T>
  class ID {
    std::atomic<T> id_cntr_;
    std::unordered_map<std::string, T> ids_;

   public:
    ID() : id_cntr_(0) {}

    ID(const ID<T> &id) {
      id_cntr_ = id.id_cntr_.load();
      ids_ = id.ids_;
    }

    ID<T> &operator=(const ID<T> &that) {
      if (&that == this) return *this;
      id_cntr_ = that.id_cntr_.load();
      ids_ = that.ids_;
      return *this;
    }

    inline T get_id(const std::string &name) {
      if (ids_.find(name) == ids_.end()) ids_[name] = id_cntr_++;
      return ids_[name];
    }
  };

  bool with_network_ = false;
  bool with_local_log_ = false;
  std::string log_dir_ = "";
  std::string log_file_ = "";
  uint64_t start_timestamp_ = 0;

  ID<uint16_t> msg_id_;
  std::unordered_map<uint16_t, ID<uint8_t>> multi_id_;

  mutelemetry_tools::ConcQueue<mutelemetry_tools::SerializedDataPtr> log_queue_;
  mutelemetry_tools::ConcQueue<mutelemetry_tools::SerializedDataPtr> net_queue_;

  mutelemetry_logger::MutelemetryLogger logger_;
  mutelemetry_network::MutelemetryStreamer streamer_;

 private:
  MuTelemetry() {}
  MuTelemetry(const MuTelemetry &) = delete;
  MuTelemetry &operator=(const MuTelemetry &) = delete;

 private:
  inline uint16_t get_msg_id(const std::string &msg_name) {
    return msg_id_.get_id(msg_name);
  }

  inline uint8_t get_multi_id(uint16_t msg_id, const std::string &inst_name) {
    if (multi_id_.find(msg_id) == multi_id_.end())
      multi_id_[msg_id] = ID<uint8_t>{};
    return multi_id_[msg_id].get_id(inst_name);
  }

  inline uint8_t get_multi_id(const std::string &msg_name,
                              const std::string &inst_name) {
    return get_multi_id(get_msg_id(msg_name), inst_name);
  }

  inline std::pair<uint16_t, uint8_t> get_ids(const std::string &msg_name,
                                              const std::string &inst_name) {
    uint16_t msg_id = get_msg_id(msg_name);
    return {msg_id, get_multi_id(msg_id, inst_name)};
  }

  inline uint64_t timestamp() const {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::high_resolution_clock::now().time_since_epoch())
        .count();
  }

  bool read_config(const std::string &file = "");
  bool create_header_and_flags();

  inline void to_io(const mutelemetry_tools::SerializedDataPtr dp) {
    if (is_log_enabled()) log_queue_.enqueue(dp);
    if (is_net_enabled()) net_queue_.enqueue(dp);
  }

  bool store_data_intl(const std::vector<uint8_t> &, const std::string &,
                       const std::string &, uint64_t);

 public:
  static bool init(fflow::RouteSystemPtr roster = nullptr);
  static inline MuTelemetry &getInstance() { return instance_; }

 public:
  inline bool is_log_enabled() const { return with_local_log_; }
  inline bool is_net_enabled() const { return with_network_; }
  inline bool is_enabled() const { return with_network_ || with_local_log_; }

  inline const std::string &get_logname() const { return log_file_; }
  inline const std::string &get_logdir() const { return log_dir_; }

  bool register_param(const std::string &, int32_t);
  bool register_param(const std::string &, float);
  bool register_info(const std::string &, const std::string &);
  bool register_info_multi(const std::string &, const std::string &, bool);

  bool register_data_format(const std::string &, const std::string &);

  // v1: using Serializable interface
  bool store_data(std::shared_ptr<Serializable> s, const std::string &type_name,
                  const std::string &annotation = "") {
    std::vector<uint8_t> data = s->serialize();
    return store_data_intl(data, type_name, annotation, timestamp());
  }

  // v2: using lambda which provides serialization
  bool store_data(const SerializerFunc &s, const std::string &type_name,
                  const std::string &annotation = "") {
    std::vector<uint8_t> data = s();
    return store_data_intl(data, type_name, annotation, timestamp());
  }

  // v3: using data, which was already serialized by the caller
  bool store_data(const std::vector<uint8_t> &data,
                  const std::string &type_name,
                  const std::string &annotation = "") {
    return store_data_intl(data, type_name, annotation, timestamp());
  }

  bool store_message(const std::string &, mutelemetry_ulog::ULogLevel);
};

}  // namespace mutelemetry
