#include "mhi_cns_power_switch.h"

namespace esphome::mhi_cns {

void MhiCnsPowerSwitch::setup() {
  this->parent_->add_listener(this);
}

void MhiCnsPowerSwitch::write_state(bool state) {
  this->parent_->set_power(state);
}

void MhiCnsPowerSwitch::on_mhi_state(const AcState &state) {
  if (state.valid)
    this->publish_state(state.power);
}

void MhiCnsAuto3dSwitch::setup() {
  this->parent_->add_listener(this);
}

void MhiCnsAuto3dSwitch::write_state(bool state) {
  this->parent_->set_auto_3d(state);
}

void MhiCnsAuto3dSwitch::on_mhi_state(const AcState &state) {
  if (state.valid)
    this->publish_state(state.auto_3d);
}

void MhiCnsSilentSwitch::setup() { this->parent_->add_listener(this); }
void MhiCnsSilentSwitch::write_state(bool state) { this->parent_->set_silent_operation(state); }
void MhiCnsSilentSwitch::on_mhi_state(const AcState &state) {
  if (state.valid) this->publish_state(state.silent_operation);
}

}  // namespace esphome::mhi_cns
