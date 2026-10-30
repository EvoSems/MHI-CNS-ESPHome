#include "mhi_cns_climate.h"

namespace esphome::mhi_cns {

void MhiCnsClimate::setup() {
  this->parent_->add_listener(this);
}

climate::ClimateTraits MhiCnsClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_HEAT_COOL,
                              climate::CLIMATE_MODE_COOL, climate::CLIMATE_MODE_HEAT,
                              climate::CLIMATE_MODE_DRY, climate::CLIMATE_MODE_FAN_ONLY});
  traits.set_supported_fan_modes({climate::CLIMATE_FAN_QUIET, climate::CLIMATE_FAN_LOW,
                                  climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH,
                                  climate::CLIMATE_FAN_AUTO});
  traits.set_supported_swing_modes({climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL});
  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
  traits.set_visual_min_temperature(18.0f);
  traits.set_visual_max_temperature(30.0f);
  traits.set_visual_target_temperature_step(0.5f);
  return traits;
}

void MhiCnsClimate::control(const climate::ClimateCall &call) {
  if (call.get_target_temperature().has_value())
    this->parent_->set_target_temperature(*call.get_target_temperature());

  if (call.get_fan_mode().has_value()) {
    switch (*call.get_fan_mode()) {
      case climate::CLIMATE_FAN_QUIET: this->parent_->set_fan(0); break;
      case climate::CLIMATE_FAN_LOW: this->parent_->set_fan(1); break;
      case climate::CLIMATE_FAN_MEDIUM: this->parent_->set_fan(2); break;
      case climate::CLIMATE_FAN_HIGH: this->parent_->set_fan(6); break;
      default: this->parent_->set_fan(7); break;
    }
  }

  if (call.get_swing_mode().has_value())
    this->parent_->set_vertical_vanes(*call.get_swing_mode() == climate::CLIMATE_SWING_VERTICAL ? 5 : 1);

  if (call.get_mode().has_value()) {
    switch (*call.get_mode()) {
      case climate::CLIMATE_MODE_OFF:
        this->parent_->set_power(false);
        break;
      case climate::CLIMATE_MODE_COOL:
        this->parent_->set_mode(HvacMode::COOL);
        this->parent_->set_power(true);
        break;
      case climate::CLIMATE_MODE_HEAT:
        this->parent_->set_mode(HvacMode::HEAT);
        this->parent_->set_power(true);
        break;
      case climate::CLIMATE_MODE_DRY:
        this->parent_->set_mode(HvacMode::DRY);
        this->parent_->set_power(true);
        break;
      case climate::CLIMATE_MODE_FAN_ONLY:
        this->parent_->set_mode(HvacMode::FAN);
        this->parent_->set_power(true);
        break;
      default:
        this->parent_->set_mode(HvacMode::AUTO);
        this->parent_->set_power(true);
        break;
    }
  }
}

void MhiCnsClimate::on_mhi_state(const AcState &state) {
  if (!state.valid)
    return;
  if (!state.power) {
    this->mode = climate::CLIMATE_MODE_OFF;
  } else {
    switch (state.mode) {
      case HvacMode::COOL: this->mode = climate::CLIMATE_MODE_COOL; break;
      case HvacMode::HEAT: this->mode = climate::CLIMATE_MODE_HEAT; break;
      case HvacMode::DRY: this->mode = climate::CLIMATE_MODE_DRY; break;
      case HvacMode::FAN: this->mode = climate::CLIMATE_MODE_FAN_ONLY; break;
      default: this->mode = climate::CLIMATE_MODE_HEAT_COOL; break;
    }
  }
  this->target_temperature = state.target_temperature;
  // DB3 is the indoor unit's normal room-temperature source. Some models also
  // expose return-air temperature as operating data, so use it as a fallback.
  this->current_temperature = !std::isnan(state.room_temperature)
                                  ? state.room_temperature
                                  : state.return_air_temperature;
  switch (state.fan) {
    case 0: this->fan_mode = climate::CLIMATE_FAN_QUIET; break;
    case 1: this->fan_mode = climate::CLIMATE_FAN_LOW; break;
    case 2: this->fan_mode = climate::CLIMATE_FAN_MEDIUM; break;
    case 6: this->fan_mode = climate::CLIMATE_FAN_HIGH; break;
    default: this->fan_mode = climate::CLIMATE_FAN_AUTO; break;
  }
  this->swing_mode = state.vertical_vanes == 5 ? climate::CLIMATE_SWING_VERTICAL : climate::CLIMATE_SWING_OFF;
  this->publish_state();
}

}  // namespace esphome::mhi_cns
