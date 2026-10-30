#include "mhi_cns.h"

#include "esphome/core/log.h"

#include <cmath>

namespace esphome::mhi_cns {

static const char *const TAG = "mhi_cns";
static constexpr uint32_t SETTINGS_KEY = 0x4D434E53;
static constexpr uint32_t SAVE_DELAY_MS = 5000;
static constexpr uint32_t DIAGNOSTIC_INTERVAL_MS = 30000;
static constexpr uint32_t OPERATING_DATA_PUBLISH_MS = 5000;
static constexpr uint8_t SETTINGS_VERSION = 2;

struct OperatingDataRequest { uint8_t selector; uint8_t code; };
static constexpr OperatingDataRequest OPERATING_DATA[] = {
    {0xC0, 0x80}, {0x40, 0x80}, {0xC0, 0x81}, {0xC0, 0x87}, {0xC0, 0x1F},
    {0x40, 0x1F}, {0xC0, 0x1E}, {0x40, 0x1E}, {0x40, 0x11}, {0x40, 0x85},
    {0x40, 0x90}, {0x40, 0x7C}, {0x40, 0x0C}, {0x40, 0x13}, {0xC0, 0x94}, {0xC0, 0xDD},
};

enum SettingField : uint8_t {
  FIELD_MODE = 1 << 0,
  FIELD_FAN = 1 << 1,
  FIELD_TEMPERATURE = 1 << 2,
  FIELD_VERTICAL = 1 << 3,
  FIELD_HORIZONTAL = 1 << 4,
  FIELD_AUTO_3D = 1 << 5,
  FIELD_SILENT = 1 << 6,
};

void MhiCnsComponent::setup() {
  this->transport_.setup();
  this->preference_ = global_preferences->make_preference<PersistedSettings>(SETTINGS_KEY, true);
  PersistedSettings loaded{};
  if (this->preference_.load(&loaded) && loaded.version == SETTINGS_VERSION) {
    this->settings_ = loaded;
    ESP_LOGI(TAG, "Loaded saved operating settings (0x%02X); power remains off", loaded.valid);
  } else {
    auto legacy_preference = global_preferences->make_preference<LegacyPersistedSettingsV1>(SETTINGS_KEY, true);
    LegacyPersistedSettingsV1 legacy{};
    if (legacy_preference.load(&legacy) && legacy.version == 1) {
      this->settings_.valid = legacy.valid;
      this->settings_.mode = legacy.mode;
      this->settings_.fan = legacy.fan;
      this->settings_.vertical_vanes = legacy.vertical_vanes;
      this->settings_.horizontal_vanes = legacy.horizontal_vanes;
      this->settings_.auto_3d = legacy.auto_3d;
      this->settings_.target_temperature_half_degrees = legacy.target_temperature_half_degrees;
      this->settings_dirty_ = true;
      this->save_at_ = millis() + SAVE_DELAY_MS;
      ESP_LOGI(TAG, "Migrated saved operating settings from version 1; power remains off");
    } else {
      ESP_LOGI(TAG, "No saved operating settings; power remains off");
    }
  }
  this->settings_.version = SETTINGS_VERSION;
  if (this->external_temperature_sensor_ != nullptr) {
    this->external_temperature_sensor_->add_on_state_callback([this](float value) {
      this->external_temperature_ = value;
      this->external_temperature_at_ = millis();
    });
  }
  // No command is queued during setup. A reboot can never turn the AC on.
}

void MhiCnsComponent::loop() {
  this->save_settings_if_due_();
  this->publish_diagnostics_if_due_();

  const bool command_transmitted = this->alternate_frame_ && this->retries_remaining_ > 0;
  CommandSnapshot frame_command = this->command_;
  if (!std::isnan(this->external_temperature_) &&
      millis() - this->external_temperature_at_ <= this->external_temperature_timeout_ms_) {
    frame_command.set_room_temperature = true;
    frame_command.room_temperature = this->external_temperature_;
  }
  if (this->alternate_frame_ && this->retries_remaining_ == 0 && ++this->operating_data_index_ >= 20) {
    static uint8_t request_index = 0;
    this->operating_data_index_ = 0;
    frame_command.request_operating_data = true;
    frame_command.operating_data_selector = OPERATING_DATA[request_index].selector;
    frame_command.operating_data_code = OPERATING_DATA[request_index].code;
    request_index = (request_index + 1) % (sizeof(OPERATING_DATA) / sizeof(OPERATING_DATA[0]));
  }
  FrameCodec::build_response(this->tx_, this->frame_size_, this->alternate_frame_, frame_command);
  // A 33-byte frame plus the inter-frame idle period can approach 60 ms on
  // some units. Keep enough margin for ESP8266 Wi-Fi interrupt jitter.
  const auto result = this->transport_.exchange(this->tx_.data(), this->rx_.data(), this->frame_size_, 100000);
  if (result == TransportResult::NO_FRAME)
    return;
  if (result != TransportResult::OK) {
    this->frame_errors_++;
    this->clock_timeouts_++;
    return;
  }

  const auto frame_result = FrameCodec::validate(this->rx_.data(), this->frame_size_);
  if (frame_result != FrameError::OK) {
    this->frame_errors_++;
    if (frame_result == FrameError::INVALID_SIGNATURE)
      this->signature_errors_++;
    else if (frame_result == FrameError::INVALID_CHECKSUM)
      this->checksum_errors_++;
    return;
  }
  // Operating-data replies contain only the requested value. Start with the
  // accumulated state so a temperature/current reply does not erase all the
  // measurements learned from earlier replies in the request cycle.
  AcState decoded = this->state_;
  FrameCodec::decode_state(this->rx_.data(), this->frame_size_, decoded);

  this->valid_frames_++;
  this->last_valid_frame_at_ = millis();
  this->alternate_frame_ = !this->alternate_frame_;
  this->state_ = decoded;
  if (this->silent_ack_pending_) {
    if (decoded.silent_operation == this->silent_ack_value_) {
      this->silent_ack_pending_ = false;
    } else if (static_cast<int32_t>(millis() - this->silent_ack_deadline_) >= 0) {
      this->silent_ack_pending_ = false;
      this->command_failures_++;
    }
  }
  this->update_settings_from_state_(decoded);
  this->notify_state_();
  this->publish_operating_data_();

  if (command_transmitted && this->command_acknowledged_(decoded)) {
    this->retries_remaining_ = 0;
  } else if (command_transmitted && this->retries_remaining_ > 0) {
    this->retries_remaining_--;
    if (this->retries_remaining_ == 0)
      this->command_failures_++;
  }
  if (this->retries_remaining_ == 0)
    this->command_ = {};
}

void MhiCnsComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MHI CNS controller");
  ESP_LOGCONFIG(TAG, "  Frame size: %u", this->frame_size_);
  ESP_LOGCONFIG(TAG, "  Command retries: %u", this->command_retries_);
  ESP_LOGCONFIG(TAG, "  Safe boot: enabled (power is never restored)");
}

void MhiCnsComponent::set_power(bool power) {
  if (power)
    this->queue_persisted_settings_();
  this->command_.set_power = true;
  this->command_.power = power;
  this->retries_remaining_ = this->command_retries_;
}

void MhiCnsComponent::set_mode(HvacMode mode) {
  this->command_.set_mode = true;
  this->command_.mode = mode;
  this->settings_.mode = static_cast<uint8_t>(mode);
  this->settings_.valid |= FIELD_MODE;
  this->mark_settings_dirty_();
  this->retries_remaining_ = this->command_retries_;
}

void MhiCnsComponent::set_fan(uint8_t fan) {
  this->command_.set_fan = true;
  this->command_.fan = fan;
  this->settings_.fan = fan;
  this->settings_.valid |= FIELD_FAN;
  this->mark_settings_dirty_();
  this->retries_remaining_ = this->command_retries_;
}

void MhiCnsComponent::set_target_temperature(float temperature) {
  this->command_.set_temperature = true;
  this->command_.target_temperature = temperature;
  this->settings_.target_temperature_half_degrees = static_cast<uint8_t>(std::lround(temperature * 2.0f));
  this->settings_.valid |= FIELD_TEMPERATURE;
  this->mark_settings_dirty_();
  this->retries_remaining_ = this->command_retries_;
}

void MhiCnsComponent::set_vertical_vanes(uint8_t value) {
  this->command_.set_vertical_vanes = true;
  this->command_.vertical_vanes = value;
  this->settings_.vertical_vanes = value;
  this->settings_.valid |= FIELD_VERTICAL;
  this->mark_settings_dirty_();
  this->retries_remaining_ = this->command_retries_;
}

void MhiCnsComponent::set_horizontal_vanes(uint8_t value) {
  if (this->frame_size_ != LONG_FRAME_SIZE)
    return;
  this->command_.set_horizontal_vanes = true;
  this->command_.horizontal_vanes = value;
  this->settings_.horizontal_vanes = value;
  this->settings_.valid |= FIELD_HORIZONTAL;
  this->mark_settings_dirty_();
  this->retries_remaining_ = this->command_retries_;
}

void MhiCnsComponent::set_auto_3d(bool value) {
  if (this->frame_size_ != LONG_FRAME_SIZE)
    return;
  this->command_.set_auto_3d = true;
  this->command_.auto_3d = value;
  this->settings_.auto_3d = value;
  this->settings_.valid |= FIELD_AUTO_3D;
  this->mark_settings_dirty_();
  this->retries_remaining_ = this->command_retries_;
}

void MhiCnsComponent::set_silent_operation(bool value) {
  this->command_.set_silent = true;
  this->command_.silent = value;
  this->settings_.silent_operation = value;
  this->settings_.valid |= FIELD_SILENT;
  this->mark_settings_dirty_();
  this->retries_remaining_ = this->command_retries_;
  this->silent_ack_pending_ = true;
  this->silent_ack_value_ = value;
  this->silent_ack_deadline_ = millis() + 45000;
}

void MhiCnsComponent::start_self_clean(uint32_t duration_ms) {
  this->set_mode(HvacMode::FAN);
  this->set_fan(1);
  this->set_power(true);
  this->set_timeout("self_clean", duration_ms, [this]() { this->set_power(false); });
}

void MhiCnsComponent::queue_persisted_settings_() {
  const auto valid = this->settings_.valid;
  if (valid & FIELD_MODE) {
    this->command_.set_mode = true;
    this->command_.mode = static_cast<HvacMode>(this->settings_.mode);
  }
  if (valid & FIELD_FAN) {
    this->command_.set_fan = true;
    this->command_.fan = this->settings_.fan;
  }
  if (valid & FIELD_TEMPERATURE) {
    this->command_.set_temperature = true;
    this->command_.target_temperature = this->settings_.target_temperature_half_degrees / 2.0f;
  }
  if (valid & FIELD_VERTICAL) {
    this->command_.set_vertical_vanes = true;
    this->command_.vertical_vanes = this->settings_.vertical_vanes;
  }
  if (this->frame_size_ == LONG_FRAME_SIZE && (valid & FIELD_HORIZONTAL)) {
    this->command_.set_horizontal_vanes = true;
    this->command_.horizontal_vanes = this->settings_.horizontal_vanes;
  }
  if (this->frame_size_ == LONG_FRAME_SIZE && (valid & FIELD_AUTO_3D)) {
    this->command_.set_auto_3d = true;
    this->command_.auto_3d = this->settings_.auto_3d;
  }
  if (valid & FIELD_SILENT) {
    this->command_.set_silent = true;
    this->command_.silent = this->settings_.silent_operation;
  }
}

void MhiCnsComponent::mark_settings_dirty_() {
  this->settings_dirty_ = true;
  this->save_at_ = millis() + SAVE_DELAY_MS;
}

void MhiCnsComponent::save_settings_if_due_() {
  if (!this->settings_dirty_ || static_cast<int32_t>(millis() - this->save_at_) < 0)
    return;
  if (this->preference_.save(&this->settings_) && global_preferences->sync())
    this->settings_dirty_ = false;
}

void MhiCnsComponent::publish_diagnostics_if_due_() {
  if (static_cast<int32_t>(millis() - this->diagnostics_at_) < 0)
    return;
  this->diagnostics_at_ = millis() + DIAGNOSTIC_INTERVAL_MS;
  if (this->frame_errors_sensor_ != nullptr)
    this->frame_errors_sensor_->publish_state(this->frame_errors_);
  if (this->valid_frames_sensor_ != nullptr)
    this->valid_frames_sensor_->publish_state(this->valid_frames_);
  const uint32_t total = this->frame_errors_ + this->valid_frames_;
  if (this->success_rate_sensor_ != nullptr)
    this->success_rate_sensor_->publish_state(total == 0 ? NAN : 100.0f * this->valid_frames_ / total);
  if (this->last_valid_frame_age_sensor_ != nullptr)
    this->last_valid_frame_age_sensor_->publish_state(
        this->last_valid_frame_at_ == 0 ? NAN : (millis() - this->last_valid_frame_at_) / 1000.0f);
  if (this->clock_timeouts_sensor_ != nullptr)
    this->clock_timeouts_sensor_->publish_state(this->clock_timeouts_);
  if (this->signature_errors_sensor_ != nullptr)
    this->signature_errors_sensor_->publish_state(this->signature_errors_);
  if (this->checksum_errors_sensor_ != nullptr)
    this->checksum_errors_sensor_->publish_state(this->checksum_errors_);
  if (this->command_failures_sensor_ != nullptr)
    this->command_failures_sensor_->publish_state(this->command_failures_);
  if (this->healthy_sensor_ != nullptr) {
    const float rate = total == 0 ? 0.0f : 100.0f * this->valid_frames_ / total;
    this->healthy_sensor_->publish_state(this->last_valid_frame_at_ != 0 &&
                                         millis() - this->last_valid_frame_at_ < 5000 && rate >= 95.0f);
  }
}

void MhiCnsComponent::notify_state_() {
  for (auto *listener : this->listeners_)
    listener->on_mhi_state(this->state_);
}

void MhiCnsComponent::update_settings_from_state_(const AcState &state) {
  // Do not let the AC's pre-command echo overwrite a newly requested value.
  // Once retries finish, the confirmed frame becomes authoritative again.
  if (this->retries_remaining_ > 0)
    return;
  bool changed = false;
  const uint8_t mode = static_cast<uint8_t>(state.mode);
  const uint8_t target = static_cast<uint8_t>(std::lround(state.target_temperature * 2.0f));
  auto update = [&](uint8_t field, uint8_t &stored, uint8_t value) {
    if ((this->settings_.valid & field) == 0 || stored != value) {
      stored = value;
      this->settings_.valid |= field;
      changed = true;
    }
  };
  update(FIELD_MODE, this->settings_.mode, mode);
  update(FIELD_FAN, this->settings_.fan, state.fan);
  update(FIELD_TEMPERATURE, this->settings_.target_temperature_half_degrees, target);
  update(FIELD_VERTICAL, this->settings_.vertical_vanes, state.vertical_vanes);
  if (this->frame_size_ == LONG_FRAME_SIZE) {
    update(FIELD_HORIZONTAL, this->settings_.horizontal_vanes, state.horizontal_vanes);
    update(FIELD_AUTO_3D, this->settings_.auto_3d, state.auto_3d ? 1 : 0);
  }
  update(FIELD_SILENT, this->settings_.silent_operation, state.silent_operation ? 1 : 0);
  if (changed)
    this->mark_settings_dirty_();
}

bool MhiCnsComponent::command_acknowledged_(const AcState &state) const {
  if (this->command_.set_power && state.power != this->command_.power) return false;
  if (this->command_.set_mode && state.mode != this->command_.mode) return false;
  if (this->command_.set_fan && state.fan != this->command_.fan) return false;
  if (this->command_.set_temperature && std::fabs(state.target_temperature - this->command_.target_temperature) > 0.51f) return false;
  if (this->command_.set_vertical_vanes && state.vertical_vanes != this->command_.vertical_vanes) return false;
  if (this->command_.set_horizontal_vanes && state.horizontal_vanes != this->command_.horizontal_vanes) return false;
  if (this->command_.set_auto_3d && state.auto_3d != this->command_.auto_3d) return false;
  return true;
}

void MhiCnsComponent::publish_operating_data_() {
  if (static_cast<int32_t>(millis() - this->operating_data_at_) < 0) return;
  this->operating_data_at_ = millis() + OPERATING_DATA_PUBLISH_MS;
#define PUB(name, value) if (this->name##_sensor_ != nullptr && !std::isnan(value)) this->name##_sensor_->publish_state(value)
  PUB(outdoor_temperature, this->state_.outdoor_temperature);
  PUB(return_air_temperature, this->state_.return_air_temperature);
  PUB(current, this->state_.current_amps);
  PUB(compressor_frequency, this->state_.compressor_frequency);
  PUB(energy_this_run, this->state_.energy_this_run);
  PUB(indoor_coil_u_bend, this->state_.indoor_coil_u_bend);
  PUB(indoor_coil_header, this->state_.indoor_coil_header);
  PUB(discharge_pipe_temperature, this->state_.discharge_pipe_temperature);
#undef PUB
  if (this->expansion_valve_sensor_) this->expansion_valve_sensor_->publish_state(this->state_.expansion_valve);
  if (this->indoor_fan_speed_sensor_) this->indoor_fan_speed_sensor_->publish_state(this->state_.indoor_fan_speed);
  if (this->outdoor_fan_speed_sensor_) this->outdoor_fan_speed_sensor_->publish_state(this->state_.outdoor_fan_speed);
  if (this->protection_code_sensor_) this->protection_code_sensor_->publish_state(this->state_.protection_code);
  if (this->indoor_run_hours_sensor_) this->indoor_run_hours_sensor_->publish_state(this->state_.indoor_run_hours);
  if (this->compressor_run_hours_sensor_) this->compressor_run_hours_sensor_->publish_state(this->state_.compressor_run_hours);
  if (this->defrost_sensor_) this->defrost_sensor_->publish_state(this->state_.defrost);
  if (this->compressor_running_sensor_) this->compressor_running_sensor_->publish_state(!std::isnan(this->state_.compressor_frequency) && this->state_.compressor_frequency > 0.0f);
  if (this->error_text_sensor_) this->error_text_sensor_->publish_state(this->state_.error_code == 0 ? "No error" : "AC error " + std::to_string(this->state_.error_code));
  if (this->protection_text_sensor_) this->protection_text_sensor_->publish_state(this->state_.protection_code == 0 ? "Normal" : "Protection " + std::to_string(this->state_.protection_code));
}

}  // namespace esphome::mhi_cns
