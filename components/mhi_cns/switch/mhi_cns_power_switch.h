#pragma once

#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"
#include "../mhi_cns.h"

namespace esphome::mhi_cns {

class MhiCnsPowerSwitch : public switch_::Switch,
                          public Component,
                          public Parented<MhiCnsComponent>,
                          public StateListener {
 public:
  void setup() override;
  void write_state(bool state) override;
  void on_mhi_state(const AcState &state) override;
};

class MhiCnsAuto3dSwitch : public switch_::Switch,
                           public Component,
                           public Parented<MhiCnsComponent>,
                           public StateListener {
 public:
  void setup() override;
  void write_state(bool state) override;
  void on_mhi_state(const AcState &state) override;
};

class MhiCnsSilentSwitch : public switch_::Switch,
                           public Component,
                           public Parented<MhiCnsComponent>,
                           public StateListener {
 public:
  void setup() override;
  void write_state(bool state) override;
  void on_mhi_state(const AcState &state) override;
};

}  // namespace esphome::mhi_cns
