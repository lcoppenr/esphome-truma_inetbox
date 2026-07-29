#include "TrumaRoomClimate.h"
#include "esphome/components/truma_inetbox/helpers.h"

namespace esphome {
namespace truma_inetbox {

static const char *const TAG = "truma_inetbox.room_climate";

// LOCAL PATCH: persist the saved setpoint across ESP reboots. Saved only on
// an actual value change (setpoint changes are rare) to spare flash wear.
void TrumaRoomClimate::set_saved_target_(float target) {
  if (!(target >= 5.0f && target <= 30.0f) || target == this->saved_target_) {
    return;
  }
  this->saved_target_ = target;
  this->saved_target_pref_.save(&this->saved_target_);
}

void TrumaRoomClimate::setup() {
  this->saved_target_pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
  float restored;
  if (this->saved_target_pref_.load(&restored) && restored >= 5.0f && restored <= 30.0f) {
    this->saved_target_ = restored;
  }

  this->parent_->get_heater()->add_on_message_callback([this](const StatusFrameHeater *status_heater) {
    // Publish updated state. While the heater is off the CP Plus reports no
    // target (TARGET_TEMP_OFF -> NaN); keep publishing the saved setpoint so
    // the entity always carries a target temperature like a real thermostat.
    float reported_target = temp_code_to_decimal(status_heater->target_temp_room);
    bool heater_off = std::isnan(reported_target);
    if (!heater_off) {
      this->set_saved_target_(reported_target);
    }
    this->target_temperature = heater_off ? this->saved_target_ : reported_target;
    this->current_temperature = temp_code_to_decimal(status_heater->current_temp_room);
    this->mode = heater_off ? climate::CLIMATE_MODE_OFF : climate::CLIMATE_MODE_HEAT;

    switch (status_heater->heating_mode) {
      case HeatingMode::HEATING_MODE_ECO:
        this->fan_mode = climate::CLIMATE_FAN_LOW;
        break;
      case HeatingMode::HEATING_MODE_HIGH:
        this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
        break;
      case HeatingMode::HEATING_MODE_BOOST:
        this->fan_mode = climate::CLIMATE_FAN_HIGH;
        break;
      default:
        this->fan_mode = climate::CLIMATE_FAN_OFF;
        break;
    }

    // switch (status_heater->heating_mode) {
    //   case HeatingMode::HEATING_MODE_ECO:
    //     this->preset = climate::CLIMATE_PRESET_ECO;
    //     break;
    //   case HeatingMode::HEATING_MODE_HIGH:
    //     this->preset = climate::CLIMATE_PRESET_COMFORT;
    //     break;
    //   case HeatingMode::HEATING_MODE_BOOST:
    //     this->preset = climate::CLIMATE_PRESET_BOOST;
    //     break;
    //   default:
    //     this->preset = climate::CLIMATE_PRESET_NONE;
    //     break;
    // }
    this->publish_state();
  });
}

void TrumaRoomClimate::dump_config() { LOG_CLIMATE(TAG, "Truma Room Climate", this); }

void TrumaRoomClimate::control(const climate::ClimateCall &call) {
  if (call.get_target_temperature().has_value() && !call.get_fan_mode().has_value()) {
    float temp = *call.get_target_temperature();
    this->set_saved_target_(temp);
    auto status_heater = this->parent_->get_heater()->get_status();
    bool heater_off = status_heater->target_temp_room == TargetTemp::TARGET_TEMP_OFF;
    bool wants_heat = call.get_mode().has_value() && *call.get_mode() == climate::CLIMATE_MODE_HEAT;
    if (!heater_off || wants_heat) {
      this->parent_->get_heater()->action_heater_room(static_cast<uint8_t>(temp));
    } else {
      // Heater is off and this call doesn't turn it on: store the setpoint
      // only (real-thermostat behavior) — adjusting the target must never
      // ignite the furnace.
      this->target_temperature = temp;
      this->publish_state();
    }
  }

  if (call.get_mode().has_value()) {
    // User requested mode change
    climate::ClimateMode mode = *call.get_mode();
    auto status_heater = this->parent_->get_heater()->get_status();
    switch (mode) {
      case climate::CLIMATE_MODE_HEAT:
        // Resume at the saved setpoint instead of the 5°C minimum. Skip when
        // the call also carried a target temperature — the branch above
        // already sent the action (sending twice would race on the LIN bus).
        if (status_heater->target_temp_room == TargetTemp::TARGET_TEMP_OFF &&
            !call.get_target_temperature().has_value()) {
          this->parent_->get_heater()->action_heater_room(static_cast<uint8_t>(this->saved_target_));
        }
        break;
      default:
        this->parent_->get_heater()->action_heater_room(0);
        break;
    }
  }

  if (call.get_fan_mode().has_value()) {
    auto fan_mode = *call.get_fan_mode();
    auto status_heater = this->parent_->get_heater()->get_status();
    float temp = temp_code_to_decimal(status_heater->target_temp_room, 0);
    if (temp <= 0) {
      // Heater currently off: resume at the saved setpoint, not the minimum.
      temp = this->saved_target_;
    }
    if (call.get_target_temperature().has_value()) {
      temp = *call.get_target_temperature();
      this->set_saved_target_(temp);
    }
    switch (fan_mode) {
      case climate::CLIMATE_FAN_LOW:
      case climate::CLIMATE_FAN_MEDIUM:
      case climate::CLIMATE_FAN_HIGH:
        if (temp < 5) {
          temp = 5;
        }
        break;
      default:
        break;
    }
    switch (fan_mode) {
      case climate::CLIMATE_FAN_LOW:
        this->parent_->get_heater()->action_heater_room(static_cast<uint8_t>(temp), HeatingMode::HEATING_MODE_ECO);
        break;
      case climate::CLIMATE_FAN_MEDIUM:
        this->parent_->get_heater()->action_heater_room(static_cast<uint8_t>(temp), HeatingMode::HEATING_MODE_HIGH);
        break;
      case climate::CLIMATE_FAN_HIGH:
        this->parent_->get_heater()->action_heater_room(static_cast<uint8_t>(temp), HeatingMode::HEATING_MODE_BOOST);
        break;
      default:
        this->parent_->get_heater()->action_heater_room(0);
        break;
    }
  }

  // if (call.get_preset().has_value()) {
  //   climate::ClimatePreset pres = *call.get_preset();
  //   auto status_heater = this->parent_->get_heater()->get_status();
  //   auto current_target_temp = temp_code_to_decimal(status_heater->target_temp_room);
  //   if (call.get_target_temperature().has_value()) {
  //     current_target_temp = *call.get_target_temperature();
  //   }
  //   switch (pres) {
  //     case climate::CLIMATE_PRESET_ECO:
  //       this->parent_->get_heater()->action_heater_room(current_target_temp, HeatingMode::HEATING_MODE_ECO);
  //       break;
  //     case climate::CLIMATE_PRESET_COMFORT:
  //       this->parent_->get_heater()->action_heater_room(current_target_temp, HeatingMode::HEATING_MODE_HIGH);
  //       break;
  //     case climate::CLIMATE_PRESET_BOOST:
  //       this->parent_->get_heater()->action_heater_room(current_target_temp, HeatingMode::HEATING_MODE_BOOST);
  //       break;
  //     default:
  //       this->parent_->get_heater()->action_heater_room(0);
  //       break;
  //   }
  // }
}

climate::ClimateTraits TrumaRoomClimate::traits() {
  // The capabilities of the climate device
  auto traits = climate::ClimateTraits();
  //traits.set_supports_current_temperature(true);
  traits.add_feature_flags(esphome::climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
  //traits.set_supported_modes({this->supported_modes_});
  for (auto mode : this->supported_modes_) {
    traits.add_supported_mode(mode);
  }

  traits.set_supported_fan_modes({{
      climate::CLIMATE_FAN_OFF,
      climate::CLIMATE_FAN_LOW,
      climate::CLIMATE_FAN_MEDIUM,
      climate::CLIMATE_FAN_HIGH,
  }});
  
  // traits.set_supported_presets({{
  //     climate::CLIMATE_PRESET_NONE,
  //     climate::CLIMATE_PRESET_ECO,
  //     climate::CLIMATE_PRESET_COMFORT,
  //     climate::CLIMATE_PRESET_BOOST,
  // }});
  traits.set_visual_min_temperature(5);
  traits.set_visual_max_temperature(30);
  traits.set_visual_temperature_step(1);
  return traits;
}
  
void TrumaRoomClimate::set_supported_modes(const std::set<climate::ClimateMode> &modes) {
  this->supported_modes_ = modes;
}

}  // namespace truma_inetbox
}  // namespace esphome
