# MHI-CNS-ESPHome

A clean ESPHome component for Mitsubishi Heavy Industries air conditioners
connected through the CNS SPI port. The first hardware target is the ESP8266
D1 Mini using the established SCK=D5, MOSI=D7 and MISO=D6 wiring.

## Design goals

- Never power the air conditioner on during boot.
- Keep protocol parsing independent from GPIO timing and ESPHome entities.
- Validate every frame before changing state.
- Coalesce commands and retry complete command snapshots.
- Restore operating settings only when an explicit power-on is requested.
- Expose communication health without adding bus traffic.
- Keep protocol code host-testable.

## Status

This is a new implementation. Frame validation and codec tests run on the host.
ESP8266 firmware compilation is required for every change. Physical hardware
testing is still required before replacing an existing installation.

## Features

- Safe-boot climate and dedicated power control
- Persistent mode, target, fan, vanes and 3D Auto
- Quiet fan mode through the climate fan-mode control
- Vertical and horizontal vane selectors
- External room-temperature injection with stale-data fallback
- Four-way climate swing plus direct vertical and horizontal vane positions
- Command acknowledgement and failure diagnostics
- Original MHI operating-data channels without calculated power estimates
- Extended 33-byte state decoding for LR vanes and 3D Auto
- Versioned persistent settings with migration
- Host protocol tests and automatic D1 Mini firmware compilation

## Example

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
    name: Lounge Room Aircon

switch:
  - platform: mhi_cns
    mhi_cns_id: lounge_mhi
    power:
      name: MHI Power
```

## Protocol references

The implementation is based on public CNS frame documentation and independently
verified protocol findings from MHI-AC-Ctrl, MHI2MQTT and the WF-RAC research.
No source code is copied from those projects.
