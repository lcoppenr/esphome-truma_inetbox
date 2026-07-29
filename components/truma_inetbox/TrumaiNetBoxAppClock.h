#pragma once

#include "TrumaStausFrameResponseStorage.h"
#include "TrumaStructs.h"

namespace esphome {
namespace truma_inetbox {

class TrumaiNetBoxApp;

class TrumaiNetBoxAppClock : public TrumaStausFrameStorage<StatusFrameClock>, public Parented<TrumaiNetBoxApp> {
 public:
  void dump_data() const override;
#ifdef USE_TIME
  // LOCAL PATCH: VarioHeat-family CP Plus never sends a StatusFrameClock in
  // its init burst, so data_valid_ can never become true on this setup and
  // clock.set would always refuse ("Cannot update Truma."). The outgoing
  // frame is built entirely from the ESP's own time source; the only field
  // borrowed from received data is clock_mode, which defaults to 0 (24h
  // display). Allow the update unconditionally; if CP Plus rejects it, a
  // ResponseAck error appears in the log.
  bool can_update() { return true; }
  void update_submit() { this->update_status_unsubmitted_ = true; }
  bool has_update() const { return this->update_status_unsubmitted_; }
  bool action_write_time();
  void create_update_data(StatusFrame *response, uint8_t *response_len, uint8_t command_counter);

 protected:
  // The behaviour of `update_status_clock_unsubmitted_` is special.
  // Just an update is marked. The actual package is prepared when CP Plus asks for the data in the
  // `lin_multiframe_recieved` method.
  bool update_status_unsubmitted_ = false;
#else
  constexpr bool has_update() const { return false; }
#endif  // USE_TIME
};

}  // namespace truma_inetbox
}  // namespace esphome
