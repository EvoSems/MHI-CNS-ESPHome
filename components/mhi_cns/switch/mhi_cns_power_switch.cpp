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

}  // namespace esphome::mhi_cns
