#include "TrumaCpPlusBinarySensor.h"
#include "esphome/core/log.h"
#include "esphome/components/truma_inetbox/helpers.h"

namespace esphome {
namespace truma_inetbox {

static const char *const TAG = "truma_inetbox.cpplus_binary_sensor";

void TrumaCpPlusBinarySensor::update() {
  if (this->parent_->get_lin_bus_fault() || (this->parent_->get_last_cp_plus_request() == 0)) {
    this->publish_state(false);
    return;
  }
  // LOCAL PATCH #7: unsigned delta so the micros() uint32 wrap (every
  // ~71.6 min) can't produce a false "alive"; 150 s window because CP Plus
  // heartbeat gaps routinely reach 121 s in its active state.
  const uint32_t since_last = micros() - (uint32_t) this->parent_->get_last_cp_plus_request();
  this->publish_state(since_last < 150 * 1000 * 1000 /* 150 seconds*/);
}

void TrumaCpPlusBinarySensor::dump_config() { LOG_BINARY_SENSOR("", "Truma CP Plus Binary Sensor", this); }
}  // namespace truma_inetbox
}  // namespace esphome