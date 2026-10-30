#include "mhi_cns.h"

#include "esphome/core/log.h"

#include <cmath>

namespace esphome::mhi_cns {

static const char *const TAG = "mhi_cns";
static constexpr uint32_t SETTINGS_KEY = 0x4D434E53;
static constexpr uint32_t SAVE_DELAY_MS = 5000;
static constexpr uint32_t DIAGNOSTIC_INTERVAL_MS = 30000;

enum SettingField : uint8_t {
  FIELD_MODE = 1 << 0,
  FIELD_FAN = 1 << 1,
  FIELD_TEMPERATURE = 1 << 2,
  FIELD_VERTICAL = 1 << 3,
  FIELD_HORIZONTAL = 1 << 4,
  FIELD_AUTO_3D = 1 << 5,
};

void MhiCnsComponent::setup() {
  this->transport_.setup();
  this->preference_ = global_preferences->make_preference<PersistedSettings>(SETTINGS_KEY, true);
  PersistedSettings loaded{};
  if (this->preference_.load(&loaded) && loaded.version == 1) {
    this->settings_ = loaded;
    ESP_LOGI(TAG, "Loaded saved operating settings (0x%02X); power remains off", loaded.valid);
  } else {
    ESP_LOGI(TAG, "No saved operating settings; power remains off");
  }
  // No command is queued during setup. A reboot can never turn the AC on.
}

void MhiCnsComponent::loop() {
  this->save_settings_if_due_();
  this->publish_diagnostics_if_due_();

  const bool command_transmitted = this->alternate_frame_ && this->retries_remaining_ > 0;
  FrameCodec::build_response(this->tx_, this->frame_size_, this->alternate_frame_, this->command_);
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
  AcState decoded{};
  FrameCodec::decode_state(this->rx_.data(), this->frame_size_, decoded);

  this->valid_frames_++;
  this->last_valid_frame_at_ = millis();
  this->alternate_frame_ = !this->alternate_frame_;
  this->state_ = decoded;
  this->update_settings_from_state_(decoded);
  this->notify_state_();

  if (command_transmitted && this->retries_remaining_ > 0)
    this->retries_remaining_--;
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
  if (changed)
    this->mark_settings_dirty_();
}

}  // namespace esphome::mhi_cns
