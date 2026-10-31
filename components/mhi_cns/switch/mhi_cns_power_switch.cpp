#include "mhi_cns_power_switch.h"

namespace esphome::mhi_cns {

void MhiCnsPowerSwitch::setup() {
  this->parent_->add_listener(this);
}

void MhiCnsPowerSwitch::write_state(bool state) {
  this->parent_->set_power(state);
}

void MhiCnsPowerSwitch::on_mhi_state(const AcState &state) {
  if (state.valid && (!this->has_state() || this->state != state.power))
    this->publish_state(state.power);
}

void MhiCnsAuto3dSwitch::setup() {
  this->parent_->add_listener(this);
}

void MhiCnsAuto3dSwitch::write_state(bool state) {
  this->parent_->set_auto_3d(state);
}

void MhiCnsAuto3dSwitch::on_mhi_state(const AcState &state) {
  if (state.valid && (!this->has_state() || this->state != state.auto_3d))
    this->publish_state(state.auto_3d);
}

}  // namespace esphome::mhi_cns
