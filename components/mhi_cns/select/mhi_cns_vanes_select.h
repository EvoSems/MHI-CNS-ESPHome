#pragma once

#include "esphome/components/select/select.h"
#include "esphome/core/component.h"
#include "../mhi_cns.h"

namespace esphome::mhi_cns {

class MhiCnsVerticalVanesSelect : public select::Select,
                                  public Component,
                                  public Parented<MhiCnsComponent>,
                                  public StateListener {
 public:
  void setup() override;
  void on_mhi_state(const AcState &state) override;
 protected:
  void control(const std::string &value) override;
};

class MhiCnsHorizontalVanesSelect : public select::Select,
                                    public Component,
                                    public Parented<MhiCnsComponent>,
                                    public StateListener {
 public:
  void setup() override;
  void on_mhi_state(const AcState &state) override;
 protected:
  void control(const std::string &value) override;
};

}  // namespace esphome::mhi_cns
