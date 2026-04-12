# Aquarium Cooling Controller

ESP32-based aquarium cooling controller with automatic fan characterization,
local autonomous cooling logic, MQTT observability, and planned OTA updates via
Wi-Fi.

## Overview

This project is building a compact cooling controller for a covered aquarium.
The system uses a 4-pin PWM fan for evaporative cooling, two DS18B20 sensors
for water and air temperature, and keeps all critical control logic local on
the ESP32.

The current repo already includes:

- a first functional specification document (FSD)
- project definition and design notes
- a Mermaid-based block schematic sketch for draw.io
- an ESP32 Arduino fan-characterization sketch
- captured serial logs from the first successful fan test runs
- a PowerShell helper for serial log capture

## Current Status

The project is in an early architecture and characterization phase.

Current focus:

- characterize the selected 4-pin PWM fan
- define the production firmware architecture
- define the central 3D-printed enclosure concept
- prepare for local control, MQTT, and OTA support

## Repository Structure

```text
.
|- README.md
|- .gitignore
|- docs/
|  |- aquarium-cooling-controller-fsd.md
|  |- result fan test/
|  |  |- measurement-summary-2026-04-12.md
|  |  |- live-run.txt
|  |  `- live-run-part2.txt
|  `- design/
|     |- project-definition.json
|     |- fan-characterization-brief.json
|     |- schematic-sketch.md
|     `- schematic-sketch.mmd
|- firmware/
|  `- fan-test/
|     `- fan-test.ino
`- tools/
   `- serial-capture.ps1
```

## Documentation

- FSD: [`docs/aquarium-cooling-controller-fsd.md`](docs/aquarium-cooling-controller-fsd.md)
- Project definition: [`docs/design/project-definition.json`](docs/design/project-definition.json)
- Characterization brief: [`docs/design/fan-characterization-brief.json`](docs/design/fan-characterization-brief.json)
- Schematic sketch notes: [`docs/design/schematic-sketch.md`](docs/design/schematic-sketch.md)
- Mermaid source for draw.io: [`docs/design/schematic-sketch.mmd`](docs/design/schematic-sketch.mmd)
- Measurement summary: [`docs/result fan test/measurement-summary-2026-04-12.md`](docs/result%20fan%20test/measurement-summary-2026-04-12.md)

## Hardware Concept

The electronics are intended to be centralized in a 3D-printed enclosure
mounted above the aquarium frame, preferably on the rear side near the lighting
area.

Planned enclosure contents:

- USB-C PD trigger board
- 5 V switched-mode supply / buck converter
- ESP32 controller board
- terminal blocks for fan, water sensor, and air sensor

Key design intent:

- short cable runs
- centralized and serviceable electronics
- reliable local control independent of Wi-Fi/MQTT
- quiet fan operation based on measured fan behavior

## Current Pin Mapping

ESP32 pin usage in the current fan-characterization setup:

| Function | ESP32 GPIO | Notes |
|---|---:|---|
| Fan PWM | 25 | PWM output for 4-pin fan control |
| Fan TACH | 26 | Tachometer input with 3.3 kOhm pull-up to 3.3 V |
| 1-Wire bus | 4 | Shared DS18B20 bus for water and air sensors |

Standard 4-pin PWM fan connector pinout:

| Fan Pin | Signal | Typical Color | Connection in this project |
|---|---|---|---|
| 1 | GND | Black | Common GND |
| 2 | +12 V | Yellow | 12 V fan supply |
| 3 | TACH | Green | ESP32 GPIO26 via tach input |
| 4 | PWM | Blue | ESP32 GPIO25 via PWM interface stage |

Verified test setup notes:

- Noctua NF-S12A PWM
- tach pull-up: `3.3 kOhm` to `3.3 V`
- common GND between ESP32, fan, and 12 V supply is mandatory
- no pull-up on the PWM line

## Firmware Scope

### Characterization Sketch

The first firmware artifact is the standalone fan-characterization sketch:

- [`firmware/fan-test/fan-test.ino`](firmware/fan-test/fan-test.ino)

It is used to:

- detect safe fan start PWM
- measure the upward PWM-to-RPM curve
- measure the downward PWM-to-RPM curve
- detect minimum hold PWM
- print reusable curve data for the production controller

### Latest Measured Results

Confirmed on the current bench setup with the Noctua NF-S12A PWM:

- start PWM from standstill: `12%`
- minimum hold PWM while spinning: `10%`
- `0%` and `5%` PWM: `0 RPM`
- measured maximum at `100%`: `1252 RPM`

Latest captured artifacts:

- [`docs/result fan test/measurement-summary-2026-04-12.md`](docs/result%20fan%20test/measurement-summary-2026-04-12.md)
- [`docs/result fan test/fan-curve-chart.svg`](docs/result%20fan%20test/fan-curve-chart.svg)
- [`docs/result fan test/live-run.txt`](docs/result%20fan%20test/live-run.txt)
- [`docs/result fan test/live-run-part2.txt`](docs/result%20fan%20test/live-run-part2.txt)

Visualized fan curve:

![Measured fan curve](docs/result%20fan%20test/fan-curve-chart.svg)

### Planned Production Firmware

The production controller is intended to provide:

- water-temperature based cooling
- secondary air-assist logic
- NVS-persisted configuration
- fan plausibility checking using measured curve data
- MQTT telemetry and remote parameter updates
- OTA update support via Wi-Fi

## draw.io / Mermaid Workflow

The block schematic can be edited in draw.io:

1. Open draw.io / diagrams.net.
2. Choose `Arrange -> Insert -> Advanced -> Mermaid`.
3. Paste the contents of [`docs/design/schematic-sketch.mmd`](docs/design/schematic-sketch.mmd).
4. Adjust the layout and export as needed.

## Suggested Next Steps

1. Convert the measured fan data into a production-ready fan-curve table and interpolation helper.
2. Define the first plausibility window above the stable operating region.
3. Validate the final PWM electrical interface for the production hardware design.
4. Implement the first production firmware modules around sensors, control, and
   plausibility checking.
5. Refine the enclosure geometry and terminal layout for the real installation.
