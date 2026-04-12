# Aquarium Cooling Controller

![Status](https://img.shields.io/badge/status-experimental-orange)
![Platform](https://img.shields.io/badge/platform-ESP32-blue)
![Firmware](https://img.shields.io/badge/firmware-Arduino-green)

ESP32-based aquarium cooling controller for a covered tank with local autonomous fan control, measured PWM-to-RPM characterization, tach-based plausibility monitoring, and planned MQTT and OTA support.

## Table of Contents

- [Overview](#overview)
- [Current Status](#current-status)
- [Features](#features)
- [Hardware](#hardware)
- [Pin Mapping](#pin-mapping)
- [Repository Layout](#repository-layout)
- [Software Dependencies](#software-dependencies)
- [Installation](#installation)
- [Usage](#usage)
- [Bench Results](#bench-results)
- [Troubleshooting](#troubleshooting)
- [Roadmap](#roadmap)
- [Contributing](#contributing)
- [License](#license)
- [Project Links](#project-links)

## Overview

This project is building a compact cooling controller for an aquarium with a lid-mounted PWM fan and two DS18B20 temperature sensors. The core design goal is to keep cooling logic local on the ESP32 so the system remains useful even when Wi-Fi, MQTT, or update infrastructure is unavailable.

The repo currently covers both the characterization phase and the first production-controller bring-up phase. The selected fan has already been measured on real hardware, and the measured curve is now reused in the production firmware scaffold for expected RPM interpolation and plausibility diagnostics.

## Current Status

The project is in the first production-firmware bring-up stage.

Already done:

- Characterized the selected 4-pin PWM fan on an ESP32 bench setup
- Measured usable start, hold, and full-scale RPM points
- Added a reusable fan-curve module with interpolation
- Added a first controller scaffold with:
  - PWM output on the real fan pin
  - tachometer RPM measurement
  - start-boost behavior
  - plausibility diagnostics against the measured curve
  - serial commands for safe bench testing

Current focus:

- Add the first DS18B20 sensor manager
- Replace manual PWM test commands with local temperature-based control
- Refine plausibility thresholds and timing with more bench data
- Prepare the path for MQTT telemetry and OTA updates

## Features

Implemented now:

- Real fan characterization sketch for a 4-pin PWM fan
- Production controller scaffold with live hardware PWM and tach feedback
- Linear interpolation from measured PWM-to-RPM curve data
- Safe start-boost from standstill
- Serial diagnostics for bench bring-up

Planned next:

- Water-temperature based control loop
- Air-assist logic with second DS18B20
- NVS-persisted configuration
- MQTT telemetry and remote parameter updates
- OTA updates over Wi-Fi

## Hardware

Bench hardware used so far:

| Component | Quantity | Purpose | Notes |
|---|---:|---|---|
| ESP32 Dev Board | 1 | Main controller | Bench-tested with `esp32:esp32:esp32` target |
| Noctua NF-S12A PWM | 1 | Cooling actuator | 120 mm 4-pin PWM fan |
| DS18B20 | 2 | Water and air temperature | Planned production sensors |
| 12 V supply | 1 | Fan power | Common ground with ESP32 is mandatory |
| Tach pull-up resistor | 1 | Tach input biasing | Bench-verified at `3.3 kOhm` to `3.3 V` |

Planned production hardware concept:

| Component | Role |
|---|---|
| ESP32-WROOM-32E / DevKitC-class board | Main controller |
| USB-C PD trigger board | Single-cable 12 V input |
| 12 V to 5 V buck converter | ESP32 supply |
| 3D-printed enclosure | Centralized electronics housing |
| Terminal blocks | Fan and sensor wiring termination |

## Pin Mapping

Current bench mapping:

| Function | ESP32 GPIO | Notes |
|---|---:|---|
| Fan PWM | 25 | 25 kHz PWM output |
| Fan TACH | 26 | Tach input with pull-up |
| 1-Wire bus | 4 | Planned shared DS18B20 bus |

Standard 4-pin PWM fan connector:

| Fan Pin | Signal | Typical Color | Project Connection |
|---|---|---|---|
| 1 | GND | Black | Common GND |
| 2 | +12 V | Yellow | 12 V fan supply |
| 3 | TACH | Green | ESP32 tach input |
| 4 | PWM | Blue | ESP32 PWM output stage |

Important bench note:

- `ESP32 GND`, fan ground, and the 12 V supply ground must share a common reference or the measurements become invalid.

## Repository Layout

```text
.
|- README.md
|- docs/
|  |- aquarium-cooling-controller-fsd.md
|  |- design/
|  `- result fan test/
|- firmware/
|  |- controller/
|  `- fan-test/
`- tools/
```

Key files:

- Functional specification: [`docs/aquarium-cooling-controller-fsd.md`](docs/aquarium-cooling-controller-fsd.md)
- Fan characterization summary: [`docs/result fan test/measurement-summary-2026-04-12.md`](docs/result%20fan%20test/measurement-summary-2026-04-12.md)
- Controller smoke test: [`docs/result fan test/controller-smoke-test-2026-04-12.md`](docs/result%20fan%20test/controller-smoke-test-2026-04-12.md)
- Characterization sketch: [`firmware/fan-test/fan-test.ino`](firmware/fan-test/fan-test.ino)
- Production controller scaffold: [`firmware/controller/controller.ino`](firmware/controller/controller.ino)
- Serial capture helper: [`tools/serial-capture.ps1`](tools/serial-capture.ps1)

## Software Dependencies

Required tools:

- [Arduino IDE](https://www.arduino.cc/en/software) 2.x or compatible `arduino-cli`
- ESP32 board package for Arduino
- PowerShell on Windows for the serial capture helper

Recommended Arduino board package URL:

```text
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

Observed during the latest successful bench run:

- FQBN: `esp32:esp32:esp32`
- Running firmware reported active ESP32 Arduino core: `3.3.7`

If your Arduino IDE claims a newer ESP32 package is installed, verify what `arduino-cli` is actually using before debugging firmware behavior.

## Installation

### Option 1: Arduino IDE

1. Install Arduino IDE 2.x.
2. Add the ESP32 board manager URL in `File -> Preferences`.
3. Install the `esp32` board package from `Boards Manager`.
4. Clone this repository:

```bash
git clone https://github.com/TeeVau/aquarium-cooling-controller.git
cd aquarium-cooling-controller
```

5. Open either:
   - `firmware/fan-test/fan-test.ino` for characterization work
   - `firmware/controller/controller.ino` for the current production scaffold

### Option 2: Arduino CLI

Example compile for the controller scaffold:

```powershell
& 'C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe' compile --fqbn esp32:esp32:esp32 firmware/controller
```

Example upload:

```powershell
& 'C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe' upload -p COM3 --fqbn esp32:esp32:esp32 firmware/controller
```

## Usage

### 1. Fan Characterization Sketch

Use [`firmware/fan-test/fan-test.ino`](firmware/fan-test/fan-test.ino) to:

- detect start PWM from standstill
- detect minimum hold PWM
- measure the upward and downward PWM-to-RPM curves
- print reusable curve data for the controller

### 2. Production Controller Scaffold

Use [`firmware/controller/controller.ino`](firmware/controller/controller.ino) for current bring-up work.

What it does today:

- starts safely at `0%` PWM
- initializes PWM output and tach measurement
- prints the measured fan curve
- accepts serial commands for controlled bench testing

Serial commands:

| Command | Effect |
|---|---|
| `0..100` | Set target PWM percentage |
| `stop` | Force fan PWM to `0%` |
| `status` | Print immediate diagnostics |
| `help` | Show command list |

Expected serial monitor speed:

```text
115200 baud
```

Example session:

```text
15
stop
status
```

## Bench Results

Measured fan behavior on the current bench setup:

- start PWM from standstill: `12%`
- minimum hold PWM while spinning: `10%`
- `0%` PWM: `0 RPM`
- `5%` PWM: `0 RPM`
- `100%` PWM: `1252 RPM`

Controller scaffold smoke test on hardware:

- compile and flash successful
- fan driver initialization successful
- RPM monitor initialization successful
- `15%` target PWM started the fan with boost and settled near the measured curve
- `stop` returned the fan cleanly to `0 RPM`

Reference artifacts:

- [`docs/result fan test/fan-curve-chart.svg`](docs/result%20fan%20test/fan-curve-chart.svg)
- [`docs/result fan test/controller-startup-2026-04-12.txt`](docs/result%20fan%20test/controller-startup-2026-04-12.txt)
- [`docs/result fan test/controller-smoke-test-2026-04-12.md`](docs/result%20fan%20test/controller-smoke-test-2026-04-12.md)

## Troubleshooting

### Fan does not react to PWM

- Verify common ground between ESP32, fan, and 12 V supply
- Check that PWM is on the real fan PWM pin, not tach or 12 V
- Confirm the board really flashed the intended sketch

### RPM stays at zero

- Check tach pull-up to `3.3 V`
- Verify tach is wired to `GPIO26`
- Confirm the fan model actually provides tach pulses

### Upload fails

- Re-check the selected COM port
- On ESP32 boards, retry while observing reset/boot timing
- Verify the Arduino IDE and `arduino-cli` are using the same ESP32 core installation

### Arduino IDE says one ESP32 version, but runtime shows another

- Run `arduino-cli core list`
- Rebuild and reflash after the board package update
- Confirm which installation `arduino-cli` is resolving

## Roadmap

Next likely steps:

1. Add `sensor_manager` for both DS18B20 sensors
2. Implement a minimal local water-temperature control loop
3. Refine plausibility timing and tolerance windows
4. Add persistence, MQTT telemetry, and OTA support
5. Finalize enclosure and wiring layout for the real aquarium installation

## Contributing

Issues, ideas, measurements, and implementation feedback are welcome.

If you want to contribute code:

1. Open an issue or describe the intended change first.
2. Keep hardware assumptions explicit.
3. Prefer changes that preserve local autonomous operation.
4. Test on real hardware when a change touches PWM, tach, or sensor handling.

## License

This repository is licensed under the GNU General Public License v3.0.

See [LICENSE](LICENSE) for the full license text.

## Project Links

- Repository: [github.com/TeeVau/aquarium-cooling-controller](https://github.com/TeeVau/aquarium-cooling-controller)
- Functional specification: [docs/aquarium-cooling-controller-fsd.md](docs/aquarium-cooling-controller-fsd.md)
