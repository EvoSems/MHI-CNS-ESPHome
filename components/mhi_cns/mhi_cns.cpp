#include "mhi_cns.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cmath>

namespace esphome::mhi_cns {

static const char *const TAG = "mhi_cns";
static constexpr uint32_t SETTINGS_KEY = 0x4D434E53;
static constexpr uint32_t SAVE_DELAY_MS = 5000;
static constexpr uint32_t DIAGNOSTIC_INTERVAL_MS = 30000;
static constexpr uint32_t OPERATING_DATA_PUBLISH_MS = 5000;
static constexpr uint32_t ROOM_TEMPERATURE_REPORT_MS = 15000;
static constexpr uint32_t DISCOVERY_REPLY_TIMEOUT_MS = 500;
static constexpr uint32_t DISCOVERY_REQUEST_GAP_MS = 100;
static constexpr uint8_t SETTINGS_VERSION = 2;

struct OperatingDataRequest { uint8_t selector; uint8_t code; };
static constexpr OperatingDataRequest OPERATING_DATA[] = {
    {0xC0, 0x80}, {0x40, 0x80}, {0xC0, 0x81}, {0x40, 0x81}, {0xC0, 0x87}, {0x40, 0x82},
    {0xC0, 0x1F},
    {0x40, 0x1F}, {0xC0, 0x1E}, {0x40, 0x1E}, {0x40, 0x11}, {0x40, 0x85},
    {0x40, 0x90}, {0x40, 0xB1}, {0x40, 0x7C}, {0x40, 0x0C}, {0x40, 0x13},
    {0xC0, 0x94}, {0xC0, 0xDD},
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
  if (this->discovery_active_ && this->discovery_pending_ &&
      static_cast<int32_t>(millis() - this->discovery_deadline_) >= 0) {
    this->discovery_timeouts_++;
    this->advance_discovery_scan_();
  }
  if (this->discovery_active_ && !this->discovery_pending_ && this->retries_remaining_ == 0 &&
      this->alternate_frame_ && static_cast<int32_t>(millis() - this->discovery_next_request_at_) >= 0) {
    static constexpr uint8_t SELECTORS[] = {0xC0, 0x40};
    frame_command.request_operating_data = true;
    frame_command.operating_data_selector = SELECTORS[this->discovery_selector_index_];
    frame_command.operating_data_code = static_cast<uint8_t>(this->discovery_code_);
    this->discovery_pending_ = true;
    this->discovery_pending_selector_ = frame_command.operating_data_selector;
    this->discovery_pending_code_ = frame_command.operating_data_code;
    this->discovery_deadline_ = millis() + DISCOVERY_REPLY_TIMEOUT_MS;
  } else if (!this->discovery_active_ && this->alternate_frame_ && this->retries_remaining_ == 0 &&
             ++this->operating_data_index_ >= 20) {
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
  if (this->trace_until_ != 0) {
    if (static_cast<int32_t>(millis() - this->trace_until_) < 0) {
      char miso[format_hex_pretty_size(LONG_FRAME_SIZE)];
      char mosi[format_hex_pretty_size(LONG_FRAME_SIZE)];
      format_hex_pretty_to(miso, this->tx_.data(), this->frame_size_, ' ');
      format_hex_pretty_to(mosi, this->rx_.data(), this->frame_size_, ' ');
      ESP_LOGI("mhi_cns.trace", "#%lu MISO %s | MOSI %s",
               static_cast<unsigned long>(++this->trace_frame_number_), miso, mosi);
    } else {
      ESP_LOGI("mhi_cns.trace", "Trace capture complete (%lu valid frame pairs)",
               static_cast<unsigned long>(this->trace_frame_number_));
      this->trace_until_ = 0;
    }
  }
  if (this->discovery_active_) {
    this->handle_discovery_reply_(this->rx_.data());
    if (this->frame_size_ == LONG_FRAME_SIZE)
      this->log_extended_state_if_changed_(this->rx_.data());
  }
  // Operating-data replies contain only the requested value. Start with the
  // accumulated state so a temperature/current reply does not erase all the
  // measurements learned from earlier replies in the request cycle.
  AcState decoded = this->state_;
  FrameCodec::decode_state(this->rx_.data(), this->frame_size_, decoded);

  // The built-in thermistor has 0.25 C resolution and naturally jitters around
  // a boundary. Average valid samples for 15 seconds, then report only the
  // nearest 0.5 C to Home Assistant. Publish the first sample immediately so
  // the climate entity is available as soon as the AC starts communicating.
  const float room_sample = !std::isnan(decoded.room_temperature)
                                ? decoded.room_temperature
                                : decoded.return_air_temperature;
  if (!std::isnan(room_sample) && room_sample >= -10.0f && room_sample <= 60.0f) {
    if (std::isnan(decoded.reported_room_temperature)) {
      decoded.reported_room_temperature = std::round(room_sample * 2.0f) / 2.0f;
      this->room_temperature_report_at_ = millis() + ROOM_TEMPERATURE_REPORT_MS;
    } else {
      this->room_temperature_sum_ += room_sample;
      this->room_temperature_samples_++;
      if (static_cast<int32_t>(millis() - this->room_temperature_report_at_) >= 0) {
        const float average = this->room_temperature_sum_ / this->room_temperature_samples_;
        decoded.reported_room_temperature = std::round(average * 2.0f) / 2.0f;
        this->room_temperature_sum_ = 0.0f;
        this->room_temperature_samples_ = 0;
        this->room_temperature_report_at_ = millis() + ROOM_TEMPERATURE_REPORT_MS;
      }
    }
  }

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
  if (this->retries_remaining_ == 0) {
    this->command_ = {};
    if (this->pending_silent_restore_) {
      this->pending_silent_restore_ = false;
      this->set_silent_operation(this->settings_.silent_operation != 0);
    }
  }
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

void MhiCnsComponent::start_trace_capture(uint32_t duration_ms) {
  this->trace_frame_number_ = 0;
  this->trace_until_ = millis() + duration_ms;
  ESP_LOGI("mhi_cns.trace", "Starting paired MISO/MOSI trace for %.1f seconds",
           duration_ms / 1000.0f);
}

void MhiCnsComponent::start_discovery_scan() {
  if (this->discovery_active_) {
    ESP_LOGI("mhi_cns.discovery", "Discovery scan is already running");
    return;
  }
  this->discovery_active_ = true;
  this->discovery_pending_ = false;
  this->discovery_selector_index_ = 0;
  this->discovery_code_ = 0;
  this->discovery_found_ = 0;
  this->discovery_timeouts_ = 0;
  this->discovery_next_request_at_ = millis();
  this->discovery_extended_state_valid_ = false;
  ESP_LOGI("mhi_cns.discovery", "Starting read-only CNS discovery scan (indoor C0, outdoor 40, codes 00-FF)");
  ESP_LOGI("mhi_cns.discovery", "CSV header: selector,code,reply_db6,op1,op2,op3");
}

void MhiCnsComponent::advance_discovery_scan_() {
  this->discovery_pending_ = false;
  this->discovery_next_request_at_ = millis() + DISCOVERY_REQUEST_GAP_MS;
  this->discovery_code_++;
  if (this->discovery_code_ <= 0xFF)
    return;
  this->discovery_code_ = 0;
  this->discovery_selector_index_++;
  if (this->discovery_selector_index_ < 2)
    return;
  this->discovery_active_ = false;
  ESP_LOGI("mhi_cns.discovery", "Discovery complete: %u replies, %u timeouts; routine polling resumed",
           this->discovery_found_, this->discovery_timeouts_);
}

void MhiCnsComponent::handle_discovery_reply_(const uint8_t *frame) {
  if (!this->discovery_pending_ || frame[12] != this->discovery_pending_code_)
    return;
  // OP1=FF is the request marker/no-value sentinel. OP1=00 is retained in the
  // log because a valid read may report zero; this scanner never transmits it.
  if (frame[13] == 0xFF && frame[14] == 0xFF && frame[15] == 0xFF)
    return;
  ESP_LOGI("mhi_cns.discovery", "%02X,%02X,%02X,%02X,%02X,%02X",
           this->discovery_pending_selector_, this->discovery_pending_code_, frame[9],
           frame[13], frame[14], frame[15]);
  this->discovery_found_++;
  this->advance_discovery_scan_();
}

void MhiCnsComponent::log_extended_state_if_changed_(const uint8_t *frame) {
  std::array<uint8_t, 8> next{};
  for (size_t i = 0; i < next.size(); i++)
    next[i] = frame[20 + i];
  if (this->discovery_extended_state_valid_ && next == this->discovery_extended_state_)
    return;
  this->discovery_extended_state_ = next;
  this->discovery_extended_state_valid_ = true;
  ESP_LOGI("mhi_cns.discovery", "EXT,raw20-27,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X",
           next[0], next[1], next[2], next[3], next[4], next[5], next[6], next[7]);
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
    // Silent operation uses a dedicated CNS request frame. Send it only after
    // the normal power/mode/fan/temperature/vane restoration is acknowledged.
    this->pending_silent_restore_ = true;
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
#define PUB(name, value) do { \
  const float next = (value); \
  if (this->name##_sensor_ != nullptr && !std::isnan(next) && \
      (!this->name##_sensor_->has_state() || std::fabs(this->name##_sensor_->state - next) > 0.01f)) \
    this->name##_sensor_->publish_state(next); \
} while (false)
  PUB(error_code, this->state_.error_code);
  if (this->indoor_temperature_sensor_) {
    const float indoor = this->state_.reported_room_temperature;
    if (!std::isnan(indoor) && (!this->indoor_temperature_sensor_->has_state() ||
        std::fabs(this->indoor_temperature_sensor_->state - indoor) > 0.01f))
      this->indoor_temperature_sensor_->publish_state(indoor);
  }
  PUB(outdoor_temperature, this->state_.outdoor_temperature);
  PUB(return_air_temperature, this->state_.return_air_temperature);
  PUB(current, this->state_.current_amps);
  PUB(compressor_frequency, this->state_.compressor_frequency);
  PUB(energy_this_run, this->state_.energy_this_run);
  PUB(indoor_coil_u_bend, this->state_.indoor_coil_u_bend);
  PUB(indoor_coil_capillary, this->state_.indoor_coil_capillary);
  PUB(indoor_coil_header, this->state_.indoor_coil_header);
  PUB(outdoor_heat_exchanger_temperature, this->state_.outdoor_heat_exchanger_temperature);
  PUB(discharge_pipe_temperature, this->state_.discharge_pipe_temperature);
  PUB(discharge_superheat, this->state_.discharge_superheat);
#undef PUB
#define PUB_DISCRETE(sensor_name, value) do { \
  const float next = (value); \
  if (this->sensor_name##_sensor_ && (!this->sensor_name##_sensor_->has_state() || \
      this->sensor_name##_sensor_->state != next)) \
    this->sensor_name##_sensor_->publish_state(next); \
} while (false)
  PUB_DISCRETE(expansion_valve, this->state_.expansion_valve);
  PUB_DISCRETE(indoor_fan_speed, this->state_.indoor_fan_speed);
  PUB_DISCRETE(outdoor_fan_speed, this->state_.outdoor_fan_speed);
  PUB_DISCRETE(protection_code, this->state_.protection_code);
  PUB_DISCRETE(indoor_run_hours, this->state_.indoor_run_hours);
  PUB_DISCRETE(compressor_run_hours, this->state_.compressor_run_hours);
#undef PUB_DISCRETE
  if (this->defrost_sensor_ && (!this->defrost_sensor_->has_state() || this->defrost_sensor_->state != this->state_.defrost))
    this->defrost_sensor_->publish_state(this->state_.defrost);
  const bool compressor_running = this->state_.compressor_running;
  if (this->compressor_running_sensor_ && (!this->compressor_running_sensor_->has_state() ||
      this->compressor_running_sensor_->state != compressor_running))
    this->compressor_running_sensor_->publish_state(compressor_running);
  if (this->occupancy_sensor_ && (!this->occupancy_sensor_->has_state() ||
      this->occupancy_sensor_->state != this->state_.occupancy_detected))
    this->occupancy_sensor_->publish_state(this->state_.occupancy_detected);
  if (this->self_clean_operation_sensor_ && (!this->self_clean_operation_sensor_->has_state() ||
      this->self_clean_operation_sensor_->state != this->state_.self_clean_operation))
    this->self_clean_operation_sensor_->publish_state(this->state_.self_clean_operation);
}

}  // namespace esphome::mhi_cns
