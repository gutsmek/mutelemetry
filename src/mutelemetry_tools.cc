#include "mutelemetry/mutelemetry_tools.h"
#include "mutelemetry/mutelemetry_parse.h"
#include "mutelemetry/mutelemetry_ulog.h"

using namespace mutelemetry_tools;
using namespace mutelemetry_ulog;
using namespace mutelemetry_parse;

bool mutelemetry_tools::check_ulog_data_begin(const uint8_t *buffer) {
  const ULogMessageHeader *hdr =
      reinterpret_cast<const ULogMessageHeader *>(buffer);
  return ULogMessageType(hdr->type_) == ULogMessageType::A;
}

bool mutelemetry_tools::check_ulog_valid(const uint8_t *buffer) {
  bool valid = MutelemetryParser::getInstance().parse(buffer);

  if (valid && check_ulog_data_begin(buffer)) {
    const ULogMessageHeader *hdr =
        reinterpret_cast<const ULogMessageHeader *>(buffer);
    const uint8_t *data_buffer = buffer + sizeof(*hdr) + hdr->size_;
    valid = MutelemetryParser::getInstance().parse(data_buffer);
  }

  return valid;
}
