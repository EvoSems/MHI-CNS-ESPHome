#pragma once

#include "esphome/components/climate/climate.h"
#include "esphome/core/component.h"
#include "../mhi_cns.h"

namespace esphome::mhi_cns {

class MhiCnsClimate : public climate::Climate,
                      public Component,
                      public Parented<MhiCnsComponent>,
                      public StateListener {
 public:
  void setup() override;
  void control(const climate::ClimateCall &call) override;
  climate::ClimateTraits traits() override;
  void on_mhi_state(const AcState &state) override;

 protected:
  bool has_published_state_{false};
  uint8_t last_vertical_position_{1};
  uint8_t last_horizontal_position_{1};
};

}  // namespace esphome::mhi_cns
