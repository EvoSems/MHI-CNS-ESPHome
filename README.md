# MHI-CNS-ESPHome

ESPHome component for Mitsubishi Heavy Industries air conditioners over the CNS
SPI interface. The current hardware target is the ESP8266 D1 Mini using SCK=D5,
MOSI=D7 and MISO=D6.

## Current build features

- Native climate control for off, auto, cool, heat, dry and fan-only modes
- Target temperature and fan modes: Quiet, Low, Medium, High and Auto
- Native climate swing modes: Off, Vertical, Horizontal and Both
- Separate vertical and horizontal vane position selectors using MHI's option
  names
- 3D Auto control
- Current room temperature, smoothed over 15 seconds and rounded to 0.5 °C
- MHI operation sensors: error code, indoor/outdoor/return-air temperatures,
  current, compressor frequency, run energy, coil temperatures, discharge
  temperature and superheat, expansion valve, indoor/outdoor fan speeds,
  protection code and run hours
- Communication diagnostics, defrost status and compressor-running status
- Saved operating settings applied only after an explicit climate power-on
- Safe boot: the controller does not turn the air conditioner on after restart
- Optional external room-temperature input with stale-value timeout

The component reports MHI operation data; it does not estimate electrical power.

## ESPHome example

```yaml
external_components:
  - source: github://EvoSems/MHI-CNS-ESPHome@main
    components: [mhi_cns]

mhi_cns:
  id: lounge_mhi
  frame_size: 33
  sck_pin: 14
  mosi_pin: 13
  miso_pin: 12

climate:
  - platform: mhi_cns
    mhi_cns_id: lounge_mhi
    name: Lounge Room Air Conditioner

switch:
  - platform: mhi_cns
    mhi_cns_id: lounge_mhi
    auto_3d:
      name: 3D Auto

select:
  - platform: mhi_cns
    mhi_cns_id: lounge_mhi
    vertical_vanes:
      name: Vertical Vanes
    horizontal_vanes:
      name: Horizontal Vanes
```

## Protocol basis

The implementation follows public CNS documentation and protocol findings from
MHI-AC-Ctrl, MHI2MQTT and WF-RAC research. No source code is copied from those
projects.
