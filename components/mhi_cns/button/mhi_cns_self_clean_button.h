#pragma once
#include "esphome/components/button/button.h"
#include "esphome/core/component.h"
#include "../mhi_cns.h"
namespace esphome::mhi_cns {
class MhiCnsSelfCleanButton : public button::Button, public Component, public Parented<MhiCnsComponent> {
 public:
  void set_duration(uint32_t value) { this->duration_ms_ = value; }
 protected:
  void press_action() override { this->parent_->start_self_clean(this->duration_ms_); }
  uint32_t duration_ms_{7200000};
};
}
