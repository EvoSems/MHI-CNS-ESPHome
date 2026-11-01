# MHI-CNS-ESPHome

ESPHome component for Mitsubishi Heavy Industries air conditioners using the
CNS interface. The D1 Mini hardware build and CNS wiring are based on the
configuration documented by [ginkage/MHI-AC-Ctrl-ESPHome](https://github.com/ginkage/MHI-AC-Ctrl-ESPHome).
This project keeps the ESP8266 D1 Mini with SCK on D5/GPIO14, MOSI on
D7/GPIO13 and MISO on D6/GPIO12.

## Features

### Climate and vanes

- Climate modes: Off, Auto, Cool, Heat, Dry and Fan-only
- Target temperature control
- Fan modes: Quiet, Low, Medium, High and Auto
- Climate swing modes: Off, Vertical, Horizontal and Both
- Independent vertical and horizontal vane position selectors
- Separate 3D Auto control
- Home Assistant climate toggle-on restores the saved MHI mode and settings

### MHI live data

- Temperatures: indoor reported room, return air, outdoor air, indoor coil
  U-bend/capillary/header, outdoor heat exchanger and discharge pipe
- Discharge superheat
- AC current and compressor frequency
- Energy this run, as reported by the MHI unit
- Indoor/outdoor fan speeds, expansion valve position and protection code
- MHI error code and indoor/compressor run hours
- Defrost and compressor-running status
- Communication health, valid/frame counts, success rate, last valid frame age,
  clock timeouts, signature/checksum errors and command failures

Sensor visibility is controlled by `show_sensors` under `mhi_cns:`. Set it to
`false` to hide the component's numeric and binary sensor entities from Home
Assistant; climate control remains available. The component does not estimate
watts or daily energy use.

### Restore and setup

- Saves mode, target temperature, fan, vane settings and 3D Auto selection
- Does not power on the air conditioner after an ESP reboot
- Publishes the first room-temperature sample immediately, then reports a
  15-second average rounded to the nearest 0.5 °C
- Optional external room-temperature sensor input with a five-minute stale-value
  timeout, then fallback to the MHI-reported room temperature
- Restart button
- Reusable configuration substitutions for installing one YAML per air
  conditioner

## ESPHome configuration

Change `deviceid` and `devicename` for each physical air conditioner.
`mhi_bus_id` is only an internal reference and can stay the same in every
separate device YAML. Add this component configuration to each device YAML:

```yaml
substitutions:
  deviceid: "loungeroomaircon"
  devicename: "Lounge Room Aircon"
  mhi_bus_id: "mhi_bus"

esphome:
  name: ${deviceid}
  friendly_name: ${devicename}

external_components:
  - source: github://EvoSems/MHI-CNS-ESPHome@main
    components: [mhi_cns]

mhi_cns:
  id: ${mhi_bus_id}
  show_sensors: true
  frame_size: 33
  sck_pin: 14
  mosi_pin: 13
  miso_pin: 12

climate:
  - platform: mhi_cns
    mhi_cns_id: ${mhi_bus_id}
    name: ${devicename}
```

The complete D1 Mini example, including all supported sensors and climate
entities, is [examples/d1_mini.yaml](examples/d1_mini.yaml). It uses the local
component checkout for development; replace its local `external_components`
source with the GitHub source above when installing from GitHub.

## Protocol references

The implementation uses public CNS protocol research from
[MHI-AC-Ctrl](https://github.com/absalom-muc/MHI-AC-Ctrl) and
[MHI-AC-Trace](https://github.com/absalom-muc/MHI-AC-Trace). Available operating
data can differ between indoor/outdoor models; only decoded fields implemented
by this component are exposed.
