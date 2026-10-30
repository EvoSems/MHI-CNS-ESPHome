#include "mhi_cns_vanes_select.h"

namespace esphome::mhi_cns {

void MhiCnsVerticalVanesSelect::setup() { this->parent_->add_listener(this); }

void MhiCnsVerticalVanesSelect::control(const std::string &value) {
  const auto index = this->index_of(value);
  if (index.has_value())
    this->parent_->set_vertical_vanes(index.value() + 1);
}

void MhiCnsVerticalVanesSelect::on_mhi_state(const AcState &state) {
  if (!state.valid || state.vertical_vanes < 1 || state.vertical_vanes > 5)
    return;
  const auto option = this->at(state.vertical_vanes - 1);
  if (option.has_value())
    this->publish_state(option.value());
}

void MhiCnsHorizontalVanesSelect::setup() { this->parent_->add_listener(this); }

void MhiCnsHorizontalVanesSelect::control(const std::string &value) {
  const auto index = this->index_of(value);
  if (index.has_value())
    this->parent_->set_horizontal_vanes(index.value() + 1);
}

void MhiCnsHorizontalVanesSelect::on_mhi_state(const AcState &state) {
  if (!state.valid || state.horizontal_vanes < 1 || state.horizontal_vanes > 8)
    return;
  const auto option = this->at(state.horizontal_vanes - 1);
  if (option.has_value())
    this->publish_state(option.value());
}

}  // namespace esphome::mhi_cns
