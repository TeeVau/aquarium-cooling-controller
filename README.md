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
|  `- design/
|     |- project-definition.json
|     |- fan-characterization-brief.json
|     |- schematic-sketch.md
|     `- schematic-sketch.mmd
`- firmware/
   `- fan-test/
      `- fan-test.ino
```

## Documentation

- FSD: [`docs/aquarium-cooling-controller-fsd.md`](docs/aquarium-cooling-controller-fsd.md)
- Project definition: [`docs/design/project-definition.json`](docs/design/project-definition.json)
- Characterization brief: [`docs/design/fan-characterization-brief.json`](docs/design/fan-characterization-brief.json)
- Schematic sketch notes: [`docs/design/schematic-sketch.md`](docs/design/schematic-sketch.md)
- Mermaid source for draw.io: [`docs/design/schematic-sketch.mmd`](docs/design/schematic-sketch.mmd)

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

1. Validate the fan PWM electrical interface with the selected fan.
2. Run and document the first fan-characterization measurements.
3. Convert measured fan data into a production-ready fan-curve table.
4. Implement the first production firmware modules around sensors, control, and
   plausibility checking.
5. Refine the enclosure geometry and terminal layout for the real installation.
