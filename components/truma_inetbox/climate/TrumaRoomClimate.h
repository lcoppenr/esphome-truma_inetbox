#pragma once

#include <set>
#include "esphome/components/climate/climate.h"
#include "esphome/core/component.h"
#include "esphome/components/truma_inetbox/TrumaiNetBoxApp.h"

namespace esphome {
  namespace truma_inetbox {
  
  class TrumaRoomClimate : public Component, public climate::Climate, public Parented<TrumaiNetBoxApp> {
   public:
    void loop() override {}
    void dump_config() override;
    void control(const climate::ClimateCall &call) override;
    void setup() override;
    climate::ClimateTraits traits() override;
  
  
    void set_visual_min_temperature(float value) { this->visual_min_temperature_ = value; }
    void set_visual_max_temperature(float value) { this->visual_max_temperature_ = value; }
    void set_visual_temperature_step(float value) { this->visual_temperature_step_ = value; }

    void set_supported_modes(const std::set<climate::ClimateMode> &modes);
  
   protected:
    std::set<esphome::climate::ClimateMode> supported_modes_;
    float visual_min_temperature_{5.0};
    float visual_max_temperature_{30.0};
    float visual_temperature_step_{0.5};
    // Last known/requested room setpoint (°C). Published while the heater is
    // off so HA always has a target (real-thermostat behavior); used to resume
    // heating at the previous setpoint instead of the 5°C minimum. Not
    // persisted: resets to 21°C after an ESP reboot until the next heat cycle.
    float saved_target_{21.0f};
  };
  
  }  // namespace truma_inetbox
  }  // namespace esphome
