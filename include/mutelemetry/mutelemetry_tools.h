#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>
#include <stack>
#include <vector>

namespace mutelemetry_tools {

using SerializedData = std::vector<uint8_t>;
using SerializedDataPtr = std::shared_ptr<SerializedData>;

bool check_ulog_data_begin(const uint8_t *);
bool check_ulog_valid(const uint8_t *);

}  // namespace mutelemetry_tools
