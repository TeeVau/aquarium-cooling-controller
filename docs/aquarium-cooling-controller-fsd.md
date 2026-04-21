# Aquarium Cooling Controller - Functional Specification Document (FSD)

## 1. System Overview

### Purpose

The Aquarium Cooling Controller is an ESP32-based embedded system that reduces
aquarium water temperature through evaporative cooling with a 4-pin PWM fan.
The system shall keep all critical temperature-control functions local to the
ESP32 and use Wi-Fi/MQTT only for observability, OTA firmware delivery, and
non-critical remote configuration.

### Problem Statement

The covered aquarium accumulates warm air under the lid due to LED lighting.
This raises water temperature and requires controlled airflow that is quiet,
reliable, and autonomous even when the network is unavailable. Before the
production controller is finalized, the specific fan must be characterized so
that the firmware can use a measured PWM-to-RPM curve for low-noise operation
and plausibility-based fault detection. The electronics shall be centralized in
a compact enclosure mounted near the aquarium lid and lighting area so that fan
and sensor cabling can remain short and serviceable.

### Users / Stakeholders

- Aquarium owner / operator
- Firmware developer / maintainer
- Service technician during commissioning or diagnostics

### Goals

- Provide local autonomous cooling based on water temperature.
- Support secondary air-assist cooling when warm air accumulates under the lid.
- Characterize the selected fan automatically before production deployment.
- Persist target temperature and selected controller settings across reboot.
- Detect implausible fan behavior using measured curve data and tolerance logic.
- Expose telemetry and selected configuration over MQTT without making cooling
  dependent on MQTT or Wi-Fi.
- Support OTA firmware updates via the ESP32 WLAN client without compromising
  autonomous cooling behavior.
- Centralize the electronics in a 3D-printed enclosure with short cable runs to
  fan and sensors.

### Non-Goals

- Heating control
- Local display or button UI
- Cloud-first control logic
- MQTT participation during the fan-characterization sketch
- Complex cloud integrations beyond local MQTT messaging

### High-Level System Flow

1. The ESP32 boots, initializes hardware, and loads persisted configuration.
2. In characterization mode, the ESP32 runs an automatic fan test and prints
   reusable curve data via serial.
3. In production mode, the ESP32 reads DS18B20 water and air temperature.
4. The controller computes `final_pwm = max(water_based_pwm, air_based_min_pwm)`.
5. The ESP32 drives the fan, measures tachometer RPM, and evaluates
   plausibility above the stable operating range.
6. Wi-Fi and MQTT publish telemetry, and a manually enabled OTA maintenance
   mode can accept a local firmware binary upload when service is required.
7. Faults are latched and reported without stopping local autonomous control
   unless a defined fault policy requires a safe fallback state.

## 2. System Architecture

### 2.1 Logical Architecture

The system is partitioned into critical local functions and non-critical
networked functions.

Critical local functions:

- Temperature acquisition from DS18B20 sensors
- Loading and validating persisted configuration
- Water-temperature control logic
- Air-assist logic
- PWM fan actuation
- Tachometer-based RPM measurement
- Fan plausibility evaluation
- Fault-state handling

Non-critical functions:

- Wi-Fi connection management
- MQTT publish/subscribe
- Temporarily enabled OTA upload service
- Remote parameter updates
- Diagnostic telemetry

Runtime interactions:

1. Sensor manager provides water and air temperature samples.
2. Config manager provides validated persisted settings.
3. Control engine computes water-based and air-based demand.
4. Fan driver applies PWM and exposes commanded duty cycle.
5. RPM monitor measures actual fan speed.
6. Fault monitor compares measured RPM to the expected curve.
7. Telemetry layer publishes state and accepts approved remote settings.
8. The OTA upload service remains disabled during normal operation and, after
   explicit service activation, accepts a single local firmware binary upload
   for validation and activation.

### 2.2 Hardware / Platform Architecture

| Element | Selection | Role |
|---|---|---|
| MCU | ESP32-WROOM-32E on ESP32-DevKitC V4 | Main controller |
| Fan | 120 mm 4-pin PWM fan, e.g. Noctua NF-S12A PWM | Cooling actuator |
| Water sensor | DS18B20 | Primary control variable |
| Air sensor | DS18B20 | Secondary air-assist trigger |
| Power input | USB-C PD trigger requesting 12 V | Single-cable power source |
| 5 V PSU | Switched-mode 12 V to 5 V supply / buck converter | Controller supply |
| 1-Wire pull-up | 3.3 kOhm to 3.3 V | Bus biasing in the verified bench setup |
| Tach pull-up | 3.3 kOhm to 3.3 V | Open-collector tach input biasing in the verified bench setup |
| Enclosure | 3D-printed housing | Central mechanical integration and mounting |
| Terminal blocks | Fan, water sensor, air sensor connectors | Field wiring termination |

Mechanical and packaging concept:

- The electronics shall be housed centrally in one 3D-printed enclosure.
- The enclosure shall contain the USB-C PD board, the 5 V PSU, the ESP32, and
  terminal connections for fan, water sensor, and air sensor wiring.
- The enclosure shall be mounted above the aquarium frame, preferably on the
  rear side near the lighting position.
- The selected mounting position shall minimize cable length to the fan and both
  sensors while keeping the assembly serviceable.
- Cable entry, strain relief, and splash exposure shall be considered in the
  final enclosure design.

Pin assignment:

| Signal | ESP32 GPIO | Notes |
|---|---:|---|
| Fan PWM | 25 | 25 kHz PWM output |
| Fan TACH | 26 | Interrupt-capable input with 3.3 kOhm pull-up to 3.3 V |
| 1-Wire bus | 33 | Shared bus for both DS18B20 sensors |

Fan connector pinout:

| Fan Pin | Signal | Typical Color | Project Connection |
|---|---|---|---|
| 1 | GND | Black | Common GND |
| 2 | +12 V | Yellow | 12 V fan supply from PD rail |
| 3 | TACH | Green | ESP32 GPIO26 tach input |
| 4 | PWM | Blue | ESP32 GPIO25 PWM output via interface stage |

Hardware constraints:

- Fan supply shall remain at 12 V while the ESP32 logic remains at 3.3 V/5 V.
- Common ground between fan power and ESP32 shall be mandatory.
- The final PWM output stage must be electrically compatible with 4-pin PC fan
  PWM input requirements.
- The tach signal shall be treated as open-collector/open-drain style feedback.
- The enclosure shall provide sufficient space and mounting features for the
  USB-C PD board, 5 V PSU, ESP32, and terminal blocks.
- Field wiring for fan and both sensors shall terminate at the central
  enclosure rather than directly on loose controller wiring.

### 2.3 Software Architecture

Planned software modules:

| Module | Responsibility |
|---|---|
| `boot/init` | Startup sequencing and hardware initialization |
| `config` | Preferences/NVS persistence and validation |
| `fan_characterization` | Automated standalone fan test sketch |
| `sensor_manager` | DS18B20 discovery, ROM-ID assignment, sample acquisition |
| `control_engine` | Water control, air-assist logic, PWM command selection |
| `fan_driver` | PWM output handling and start-boost support |
| `rpm_monitor` | Tach pulse counting and RPM calculation |
| `fault_monitor` | Plausibility checking, debounce, fault latching |
| `fault_policy` | Alarm classification, severity, and local fault response |
| `telemetry` | Wi-Fi, MQTT publish/subscribe, diagnostics |
| `ota_upload_server` | Temporary BIN-only OTA upload endpoint, image validation, update handoff |

Boot sequence:

1. Initialize serial diagnostics and hardware GPIO.
2. Initialize PWM output and tach interrupt.
3. Load persisted configuration from Preferences/NVS.
4. Validate configuration and fall back to defaults if invalid.
5. Start local control and safety monitoring.
6. Attempt Wi-Fi/MQTT connectivity after local control is operational.
7. Keep OTA upload functionality disabled until local control startup has
   completed and an operator explicitly enables the OTA maintenance window.

Persistence model:

- Storage backend: ESP32 Preferences / NVS
- Current implementation persists the target temperature
- Invalid persisted target values shall be cleared and replaced with the
  default target temperature
- Additional air-assist and tuning values may be added later
- Sensor role mapping shall use DS18B20 ROM IDs rather than bus order

Update model:

- Characterization sketch is a standalone firmware artifact.
- Production firmware is a separate artifact that reuses measured fan-curve
  data.
- Production firmware releases shall use Semantic Versioning 2.0.0
  (`MAJOR.MINOR.PATCH`) as defined by <https://semver.org/>.
- Firmware release history shall be maintained in a `CHANGELOG.md` file using
  the Keep a Changelog format defined by <https://keepachangelog.com/>.
- Production firmware shall support OTA updates through a manually enabled,
  temporary local upload endpoint on the ESP32.
- OTA shall use the compiled firmware `.bin` image directly. ZIP archives,
  manifest files, and external update polling are not part of the initial OTA
  workflow.
- The OTA upload endpoint shall not require a password or token. Protection is
  provided by keeping the endpoint disabled by default, limiting the active
  upload window, accepting one upload attempt per activation, and validating the
  received firmware image.
- OTA shall be treated as a non-critical service and shall not block local
  cooling startup.
- OTA failures shall leave the device on the previously working firmware.

## 3. Implementation Phases

### 3.1 Phase 1 - Fan Characterization Foundation

Scope:

- Implement standalone automatic fan test sketch
- Detect safe start PWM from standstill
- Measure increasing and decreasing PWM-to-RPM curves
- Detect minimum hold PWM while the fan is already spinning
- Print reusable serial output for later firmware integration

Deliverables:

- Compilable ESP32 Arduino characterization sketch
- Human-readable serial log
- Tabular PWM/RPM output
- C-array initializer for measured curve points
- Bench log and summarized characterization result stored in project docs

Exit criteria:

- Sketch compiles for ESP32 Arduino
- Sketch runs without manual serial commands
- Start PWM is reported
- Hold PWM is reported
- Up and down curves are printed
- Measured data is reusable in later firmware
- Latest verified characterization run is archived in project documentation

Dependencies:

- Selected physical fan available
- Tach signal electrically readable
- Stable 12 V power path

### 3.2 Phase 2 - Autonomous Local Cooling Controller

Scope:

- Integrate DS18B20 water and air temperature sensing
- Implement local cooling control using persisted configuration
- Apply quiet cooling policy and optional start-boost support
- Add fan plausibility checking using measured curve interpolation
- Introduce fault handling for fan and sensor failures

Deliverables:

- Production-oriented local control firmware
- Persistent configuration storage
- Deterministic control loop
- Fault and diagnostics model

Exit criteria:

- Controller cools without Wi-Fi or MQTT
- Target temperature survives reboot
- Sensor roles remain stable by ROM ID
- Fan fault detection works above stable operating range

Dependencies:

- Phase 1 fan characterization results
- Installed DS18B20 sensors
- Finalized local fault-reaction policy for sensor and fan faults

### 3.3 Phase 3 - Networked Observability and Remote Configuration

Scope:

- Add Wi-Fi connectivity
- Publish telemetry and fault state over MQTT
- Add manually enabled BIN-only OTA firmware upload capability over the WLAN
  client
- Accept validated remote configuration updates
- Keep local control fully autonomous during network outages

Current implementation note: The first Wi-Fi/MQTT increment implements
publish-only telemetry and network diagnostics and has been verified against
the local MQTT broker. MQTT remote configuration and OTA remain planned
follow-up work. A FHEM `MQTT2_DEVICE` integration is provided as a
monitoring-only consumer for the verified telemetry topics.

Deliverables:

- MQTT topic implementation
- BIN-only OTA upload workflow
- Telemetry and status publishing
- FHEM monitoring integration for current MQTT telemetry
- Validated remote settings workflow

Exit criteria:

- MQTT publishes required state and diagnostics
- OTA firmware image can be uploaded locally, validated, and applied over Wi-Fi
- Approved remote settings are persisted
- Network loss does not interrupt cooling autonomy

Dependencies:

- Phase 2 production controller complete
- MQTT broker and Wi-Fi credentials available

## 4. Functional Requirements

### 4.1 Functional Requirements (FR)

#### Phase 1 - Characterization

- FR-1.1 [Must]: The system shall provide a standalone fan-characterization
  firmware artifact that does not require Wi-Fi or MQTT.
- FR-1.2 [Must]: The characterization sketch shall automatically search the
  minimum PWM at which the fan starts reliably from standstill.
- FR-1.3 [Must]: The characterization sketch shall measure the increasing
  PWM-to-RPM curve from the detected start PWM up to 100% PWM.
- FR-1.4 [Must]: The characterization sketch shall measure the decreasing
  PWM-to-RPM curve from 100% PWM down to 0% PWM.
- FR-1.5 [Must]: The characterization sketch shall determine the minimum hold
  PWM while the fan is already spinning.
- FR-1.6 [Must]: The characterization sketch shall print a progress log, a
  readable curve table, and a machine-usable C-array initializer.

#### Phase 2 - Local Autonomous Cooling

- FR-2.1 [Must]: The production controller shall read water temperature from a
  dedicated DS18B20 sensor identified by ROM ID.
- FR-2.2 [Must]: The production controller shall read air temperature from a
  dedicated DS18B20 sensor identified by ROM ID.
- FR-2.3 [Must]: The production controller shall compute the primary cooling
  demand from water temperature relative to target temperature.
- FR-2.4 [Must]: The production controller shall support an air-assist minimum
  PWM contribution when configured thresholds for under-lid air temperature are
  exceeded.
- FR-2.5 [Must]: The production controller shall calculate the commanded fan
  PWM as `max(water_based_pwm, air_based_min_pwm)`.
- FR-2.6 [Should]: The production controller should apply start-boost behavior
  when needed to improve reliable low-speed fan startup.
- FR-2.7 [Must]: The production controller shall drive the fan through a PWM
  output suitable for a 4-pin PWM fan.
- FR-2.8 [Must]: The production controller shall measure fan RPM from the tach
  signal using pulse counting.
- FR-2.9 [Must]: The production controller shall persist target temperature and
  selected control parameters in Preferences/NVS.
- FR-2.10 [Must]: The production controller shall load persisted configuration
  at boot, validate it, and apply defaults if stored values are invalid.
- FR-2.11 [Must]: The production controller shall continue local cooling
  operation when Wi-Fi or MQTT is unavailable.
- FR-2.12 [Must]: The system hardware shall be integrated into a central
  3D-printed enclosure containing the USB-C PD board, 5 V PSU, ESP32, and
  terminal blocks for fan, water sensor, and air sensor connections.
- FR-2.13 [Must]: The enclosure shall be mountable above the aquarium frame,
  preferably on the rear side near the lighting area, to minimize cable length.
- FR-2.14 [Should]: The enclosure design should support orderly cable routing,
  basic strain relief, and service access for wiring and maintenance.

#### Phase 2 - Fault Handling

- FR-3.1 [Must]: The production controller shall estimate expected fan RPM from
  the measured fan curve using linear interpolation between neighboring points.
- FR-3.2 [Must]: The production controller shall evaluate fan plausibility only
  above the minimum stable operating PWM and after a settling delay following
  PWM changes.
- FR-3.3 [Must]: The production controller shall confirm fan fault state only
  after multiple consecutive plausibility mismatches.
- FR-3.4 [Must]: The production controller shall expose diagnostics including
  expected RPM, measured RPM, RPM error, plausibility state, and fan-fault
  state.
- FR-3.5 [Must]: The production controller shall enter a defined fallback fault
  behavior when the water-temperature sensor is unavailable.
- FR-3.6 [Must]: The production controller shall continue water-based cooling
  control when the air-temperature sensor fails and shall disable or ignore
  air-assist logic in that condition.
- FR-3.7 [Should]: The production controller should support recovery from a fan
  plausibility fault only after multiple consecutive valid matches.

#### Phase 3 - MQTT Integration

- FR-4.1 [Must]: The production controller shall publish water temperature, air
  temperature, fan PWM, fan RPM, target temperature, controller mode, and fault
  status over MQTT. This is implemented and broker-verified as a publish-only
  telemetry foundation.
- FR-4.2 [Must]: The production controller shall accept validated remote
  updates for target temperature and selected non-critical control flags over
  MQTT. This remains planned after the verified publish-only telemetry
  foundation.
- FR-4.3 [Should]: The production controller should support a manual override
  mode for service or testing with explicit validation and clear state
  reporting.
- FR-4.4 [Must]: The production controller shall support manual BIN-only OTA
  firmware upload over the WLAN client through a temporary ESP32-hosted upload
  endpoint.
- FR-4.5 [Must]: The production controller shall start OTA upload activity only
  after local cooling control is already active and an operator explicitly
  enables the OTA maintenance window.
- FR-4.6 [Must]: The production controller shall validate the uploaded firmware
  image size, ESP32 image validity, firmware release identity, and update result
  before activating the uploaded firmware image.
- FR-4.7 [Must]: The production controller shall preserve the last working
  firmware if OTA upload, validation, or activation fails.
- FR-4.8 [Should]: The production controller should publish OTA state and last
  update result through diagnostics or MQTT status topics.
- FR-4.9 [Should]: The repository should provide a local home-automation
  monitoring definition for the current MQTT telemetry surface. The current
  FHEM `MQTT2_DEVICE` definition is monitoring-only until remote set topics are
  implemented in firmware.

### 4.2 Non-Functional Requirements (NFR)

- NFR-1.1 [Must]: Critical cooling logic must execute locally on the ESP32 and
  must not depend on network connectivity.
- NFR-1.2 [Must]: The system must prefer quiet fan behavior over maximum
  cooling performance whenever temperature safety is maintained.
- NFR-1.3 [Must]: The system must retain persisted configuration across reboot
  and across temporary Wi-Fi/MQTT outages.
- NFR-1.4 [Must]: The system must use deterministic control logic suitable for
  an embedded real-time-ish control loop.
- NFR-1.5 [Must]: The system must avoid unnecessary dynamic memory allocation in
  critical runtime paths.
- NFR-1.6 [Should]: The system should separate critical control logic from
  communication logic for maintainability and fault isolation.
- NFR-1.7 [Must]: The system must provide sufficient diagnostics to distinguish
  sensor fault, fan fault, and network fault conditions.
- NFR-1.8 [Should]: The system should boot into a valid local control state
  before attempting network services.
- NFR-1.9 [Must]: The characterization sketch must run fully automatically with
  no manual serial commands.
- NFR-1.10 [Must]: OTA update handling must not delay or block the start of
  local autonomous cooling beyond normal network-service initialization.
- NFR-1.11 [Must]: OTA update integrity must be verifiable before a new
  firmware image is activated through ESP32 image validation and release
  identity checks.
- NFR-1.12 [Must]: Production firmware version numbers shall follow Semantic
  Versioning 2.0.0 as `MAJOR.MINOR.PATCH`, with optional pre-release or build
  metadata only when it conforms to the SemVer specification.
- NFR-1.13 [Must]: The repository shall maintain release history in
  `CHANGELOG.md` using the Keep a Changelog structure, including an
  `Unreleased` section and grouped notable changes for each released version.

### 4.3 Constraints

- System only cools; it does not provide heating.
- No local display or button interface is planned.
- Water sensor role and air sensor role shall not depend on DS18B20 bus order.
- The final PWM electrical interface must be validated with the selected fan.
- Fan plausibility tolerance and confirmed fan-fault reaction are not yet fully
  finalized.
- Low-PWM regions may require wider tolerance or exclusion from plausibility
  checking.
- The final enclosure design must tolerate the installation position near warm,
  humid aquarium lighting surroundings.

## 5. Risks, Assumptions & Dependencies

### Technical Risks

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| Fan PWM electrical interface is not fully compatible with the selected fan | Medium | High | Validate prototype waveform and startup behavior before production |
| Low-PWM tach readings are unstable and trigger false faults | High | Medium | Exclude unstable region or widen tolerance below stable PWM |
| Under-lid airflow differs from free-air characterization | Medium | Medium | Verify curve once in free air and again in installed configuration |
| DS18B20 bus or role mapping errors swap water and air sensors | Low | High | Assign roles by ROM ID and verify mapping at commissioning |
| Fault reaction for confirmed fan fault remains underspecified | Medium | High | Finalize explicit reaction before production release |
| Enclosure placement near the aquarium lid increases heat, humidity, or splash exposure | Medium | High | Use protected mounting position, cable management, and enclosure design with environmental margin |
| Unauthenticated OTA upload window is reachable by other clients on the local network | Low | High | Keep OTA disabled by default, require explicit service activation, limit the upload window, accept one upload attempt, and validate firmware identity before activation |

### Assumptions

- The selected fan provides two tach pulses per revolution, matching the test
  sketch assumption.
- A fixed initial plausibility tolerance around +/-12% is a suitable starting
  point until measured tuning data is available (assumed).
- The aquarium lid has both a dedicated fan opening and a separate air outlet,
  allowing effective airflow (assumed from project notes).
- OTA uploads will be performed from a trusted client on the same local network
  during a short, explicitly enabled maintenance window (assumed).

### External Dependencies

- ESP32 Arduino framework
- Preferences library
- OneWire library
- DallasTemperature library
- WiFi library
- MQTT client library such as PubSubClient or equivalent
- OTA-capable update library or ESP32 OTA support compatible with the chosen
  Arduino framework
- Semantic Versioning 2.0.0 release numbering rules
- Keep a Changelog release-history format
- USB-C PD trigger hardware
- Selected 4-pin PWM fan and DS18B20 sensors

### Environmental Constraints

- Covered aquarium environment with moisture and heat near the lid
- Single-cable compact power delivery preferred
- Low audible noise prioritized over maximum airflow
- The enclosure mounting position is above the aquarium frame, preferably on the
  rear side near the lighting assembly
- Cable runs to fan and both DS18B20 sensors should be kept as short as
  practical

## 6. Interface Specifications

### 6.1 External Interfaces

#### Hardware Interfaces

| Interface | Direction | Description |
|---|---|---|
| 4-pin fan PWM | ESP32 -> fan | Speed command at nominal 25 kHz PWM |
| 4-pin fan tach | fan -> ESP32 | Pulse feedback for RPM measurement |
| 1-Wire bus | bidirectional | Shared DS18B20 bus for water and air sensors |
| USB serial | ESP32 -> host | Diagnostics and characterization output |
| Wi-Fi | ESP32 <-> LAN | Production telemetry transport |
| MQTT | ESP32 <-> broker | Production state publish and remote set commands |
| OTA upload endpoint | host -> ESP32 | Temporary local upload of one compiled firmware `.bin` image over Wi-Fi |

#### MQTT Topic Proposal

Committed defaults use the root `aquarium/cooling`. The verified local bench
setup used the override root `aquarium_cooling` from the ignored local network
configuration file.

| Topic | Direction | Purpose |
|---|---|---|
| `aquarium/cooling/state/water_temp_c` | publish | Water temperature |
| `aquarium/cooling/state/air_temp_c` | publish | Air temperature |
| `aquarium/cooling/state/fan_pwm_percent` | publish | Commanded fan PWM |
| `aquarium/cooling/state/fan_rpm` | publish | Measured fan RPM |
| `aquarium/cooling/state/target_temp_c` | publish | Active target temperature |
| `aquarium/cooling/state/controller_mode` | publish | Controller mode |
| `aquarium/cooling/diagnostic/expected_rpm` | publish | Interpolated expected RPM |
| `aquarium/cooling/diagnostic/rpm_tolerance` | publish | Current RPM tolerance |
| `aquarium/cooling/diagnostic/rpm_error` | publish | Measured minus expected RPM |
| `aquarium/cooling/diagnostic/plausibility_active` | publish | Whether fan plausibility is currently evaluated |
| `aquarium/cooling/status/fan_plausible` | publish | Plausibility state |
| `aquarium/cooling/status/fan_fault` | publish | Latched fan fault |
| `aquarium/cooling/status/water_sensor_ok` | publish | Water sensor health |
| `aquarium/cooling/status/air_sensor_ok` | publish | Air sensor health |
| `aquarium/cooling/status/cooling_degraded` | publish | Whether cooling effectiveness is degraded |
| `aquarium/cooling/status/service_required` | publish | Whether operator action is required |
| `aquarium/cooling/status/alarm_code` | publish | Fault summary |
| `aquarium/cooling/status/fault_severity` | publish | Fault severity |
| `aquarium/cooling/status/fault_response` | publish | Local fault response |
| `aquarium/cooling/status/availability` | publish | MQTT online/offline availability |
| `aquarium/cooling/set/target_temp_c` | subscribe | Remote target temperature |
| `aquarium/cooling/set/air_assist_enable` | subscribe | Air-assist enable flag |
| `aquarium/cooling/set/air_min_pwm_percent` | subscribe | Minimum air-assist PWM |

#### FHEM MQTT2 Monitoring Integration

The repository includes a FHEM `MQTT2_DEVICE` definition at:

```text
integrations/fhem/aquarium-cooling-mqtt2-device.cfg
```

It maps all currently published telemetry topics to explicit FHEM readings.
The file uses the verified bench root topic `aquarium_cooling`; deployments
using the committed default root `aquarium/cooling` must adjust the root topic
before importing it into FHEM.

The FHEM integration must remain monitoring-only for the current firmware
revision. The planned FHEM `setList` is documented but commented out because
the firmware does not yet subscribe to or validate remote `/set/...` topics.

#### OTA Update Interface

| Element | Direction | Purpose |
|---|---|---|
| OTA enable command | operator -> ESP32 | Open a short maintenance window for local firmware upload |
| OTA upload endpoint | host -> ESP32 | Accept one compiled firmware `.bin` image; no ZIP, manifest, password, or token |
| OTA result status | publish/log | Report update progress, success, or failure |

### 6.2 Internal Interfaces

| Producer | Consumer | Interface |
|---|---|---|
| `sensor_manager` | `control_engine` | Water and air temperature samples |
| `config` | `control_engine` | Validated target and feature settings |
| `control_engine` | `fan_driver` | Commanded PWM percentage |
| `fan_driver` | `rpm_monitor` | Active PWM state for settling logic |
| `rpm_monitor` | `fault_monitor` | Measured RPM |
| `fan_curve` | `fault_monitor` | Expected RPM interpolation lookup |
| `fault_monitor` | `fault_policy` | Fan plausibility state and latched fan fault |
| `fault_policy` | `telemetry` | Alarm code, severity, response, and service state |

### 6.3 Data Models / Schemas

#### Fan Curve Point

| Field | Type | Description |
|---|---|---|
| `pwm_percent` | `uint8_t` | PWM command in percent |
| `rpm` | `uint16_t` | Measured RPM |

#### Persisted Configuration

| Key | Type | Description |
|---|---|---|
| `target_c` | float | Desired water temperature |
| `target_set` | bool | Marks whether a custom target temperature is stored |
| `air_assist_enabled` | bool | Planned later: enable under-lid air-assist logic |
| `air_assist_min_pwm_percent` | uint8_t | Planned later: minimum PWM when air-assist is active |
| `control_mode` | enum | `auto`, optional `manual_override`, `fault_mode` |

#### Diagnostics Payload

| Field | Type | Description |
|---|---|---|
| `expected_rpm` | integer | Interpolated expected RPM |
| `measured_rpm` | integer | Measured fan RPM |
| `rpm_error_absolute` | integer | Absolute RPM deviation |
| `rpm_error_percent` | float | Relative deviation |
| `fan_plausible` | bool | Current plausibility result |
| `fan_fault` | bool | Latched fault state |
| `alarm_code` | string | Current summarized fault code |
| `fault_severity` | string | `none`, `warning`, or `critical` |
| `fault_response` | string | Current local fault response |
| `cooling_degraded` | bool | Whether local cooling effectiveness is degraded |
| `service_required` | bool | Whether operator/service action is required |

#### Firmware Release Metadata

| Field | Type | Description |
|---|---|---|
| `firmware_version` | SemVer string | Production firmware version in `MAJOR.MINOR.PATCH` form, with optional SemVer-compliant pre-release or build metadata |
| `release_date` | ISO date | Date assigned to the released firmware version in `CHANGELOG.md` |
| `changelog_section` | Markdown section | Keep a Changelog release section containing grouped notable changes for the version |
| `release_state` | enum/string | `unreleased`, `pre-release`, or `released` |

### 6.4 Commands / Opcodes

The system does not define a custom binary command protocol. The current local
controller exposes a small text-based USB serial service interface for bench
operation and diagnostics:

| Command | Purpose |
|---|---|
| `status` | Print an immediate diagnostics block |
| `target <c>` | Set and persist a custom water target temperature |
| `default` | Clear persisted target and return to the default `23.0 C` |
| `airassist` | Print the current air-assist defaults |
| `faults` | Print the current local fault-policy defaults |
| `network` | Print Wi-Fi/MQTT configuration and connection status |
| `publish` | Publish telemetry immediately when MQTT is connected |
| `help` | Print the supported command list |

Future production command inputs may additionally be represented by validated
MQTT set topics and an explicitly enabled OTA firmware upload maintenance
window over Wi-Fi. Characterization mode remains fully automatic with no
runtime command input.

## 7. Operational Procedures

### Deployment / Flashing

1. Assemble the ESP32, fan power path, tach pull-up, PWM interface, and 1-Wire
   bus.
2. Verify common ground between the fan supply and ESP32.
3. Install the USB-C PD board, 5 V PSU, ESP32, and terminal blocks into the
   3D-printed enclosure.
4. Mount the enclosure above the aquarium frame, preferably at the rear near the
   lighting area.
5. Route and terminate fan, water-sensor, and air-sensor cables at the
   enclosure.
6. Flash the characterization sketch for first article testing.
7. Record the measured fan curve and minimum stable PWM data.
8. Integrate measured values into production firmware.
9. Assign or update the production firmware SemVer version and update
   `CHANGELOG.md` before building a release candidate.
10. Flash production firmware after control and fault parameters are finalized.
11. Verify the temporary BIN-only OTA upload path after Wi-Fi integration is
   available.

### Provisioning / Configuration

1. Assign DS18B20 water and air roles by ROM ID.
2. Store the default target temperature and air-assist settings in NVS.
3. Validate persisted values at boot.
4. In production mode, connect to Wi-Fi and MQTT only after local control is
   active.
5. Configure the OTA upload policy before enabling OTA in production. The
   initial OTA design shall not require a password or token.
6. Verify terminal labeling and cable assignment for fan, water sensor, and air
   sensor during commissioning.

### Normal Operation

1. Sample water and air temperature at the configured interval.
2. Compute water-based cooling demand.
3. Compute air-assist minimum PWM if enabled.
4. Apply the maximum of both demands.
5. Allow settling time after PWM changes before plausibility evaluation.
6. Measure fan RPM and evaluate plausibility where valid.
7. Publish telemetry and accept validated remote settings when connected.
8. Keep OTA upload disabled unless an operator explicitly opens the maintenance
   window while local control remains active.

### Maintenance Procedures

1. Re-run fan characterization if the fan model changes.
2. Re-run characterization if the installed airflow path changes materially.
3. Inspect diagnostics for drift between expected and measured RPM.
4. Verify sensor ROM-ID assignment after replacing a DS18B20 sensor.
5. Trigger and verify OTA uploads only when the system is in a stable operating
   condition and the local network client is trusted.
6. Before publishing a firmware release, verify that the firmware version is a
   valid SemVer value and that `CHANGELOG.md` contains a corresponding Keep a
   Changelog release entry.
7. Inspect enclosure mounting, cable strain relief, and terminal tightness
   during maintenance intervals.

### Recovery Procedures

1. On invalid persisted configuration, reset affected keys to defaults and log
   the recovery.
2. On water-sensor failure, enter `water-fallback` at `40%` PWM and raise a
   critical alarm.
3. On air-sensor failure, continue water-based control, suppress air-assist,
   and raise a warning.
4. On network failure, continue local control with the last valid persisted
   settings.
5. On confirmed fan fault, keep the locally computed PWM command, raise a
   critical `fan-fault`, and require service. Hardware tests with missing tach
   feedback and a deliberately slowed fan verified this response; no automatic
   fan-fault boost is currently applied.
6. On OTA upload or validation failure, remain on the currently working
   firmware and report the failed update state.

## 8. Verification & Validation

### 8.1 Phase 1 Verification

| Test ID | Feature | Procedure | Success Criteria |
|---|---|---|---|
| TC-P1-01 | Standalone characterization | Flash `firmware/fan-test/fan-test.ino` and boot with connected fan | Sketch starts without requiring serial input |
| TC-P1-02 | Start PWM detection | Observe automatic search from stop to running state | Safe start PWM is reported |
| TC-P1-03 | Increasing curve | Run full upward sweep from detected start PWM | PWM/RPM points are printed up to 100% |
| TC-P1-04 | Decreasing curve | Run full downward sweep from 100% to 0% | PWM/RPM points are printed down to 0% |
| TC-P1-05 | Hold PWM detection | Observe running fan while PWM is decreased stepwise | Minimum hold PWM is reported |
| TC-P1-06 | Reusable output | Inspect serial summary and code block | Readable table and C-array initializer are printed |

### 8.2 Phase 2 Verification

| Test ID | Feature | Procedure | Success Criteria |
|---|---|---|---|
| TC-P2-01 | DS18B20 role assignment | Boot with both sensors connected and known ROM IDs | Water and air roles map to correct sensors |
| TC-P2-02 | Water-based control | Simulate water temperature below, near, and above target | PWM responds according to control mapping |
| TC-P2-03 | Air-assist logic | Raise simulated air temperature beyond threshold | Air-assist minimum PWM is applied |
| TC-P2-04 | Max-selection logic | Force simultaneous water and air demand | Final PWM equals the higher of the two demands |
| TC-P2-05 | NVS persistence | Change target and air-assist settings, reboot device | Values survive reboot and reload correctly |
| TC-P2-06 | RPM measurement | Drive fan at stable PWM and compare with tach observations | RPM measurement is plausible and repeatable |
| TC-P2-07 | Fault debounce | Inject repeated RPM mismatches above stable PWM | Fault latches only after configured consecutive mismatches |
| TC-P2-08 | Water sensor failure | Disconnect or invalidate water sensor input | Controller enters defined fallback behavior and raises fault |
| TC-P2-09 | Air sensor failure | Disconnect or invalidate air sensor input | Water-based cooling continues and air-assist is disabled |
| TC-P2-10 | Recovery debounce | Restore valid RPM after induced mismatch series | Recovery occurs only after configured consecutive matches |
| TC-P2-11 | Critical/runtime memory discipline | Inspect critical control path under normal operation and review implementation for avoidable heap use | No unnecessary dynamic allocation is present in critical runtime paths |
| TC-P2-12 | Control/communication separation | Review module boundaries and disable network stack during runtime tests | Local control remains operational with communication logic absent or inactive |
| TC-P2-13 | Boot sequencing | Boot with valid persisted config and delayed/unavailable network | Local control becomes valid before network initialization is required |
| TC-P2-14 | OTA non-blocking startup | Boot with OTA support compiled in and no active OTA maintenance window | Local cooling starts before any OTA upload service is available |
| TC-P2-15 | Enclosure integration | Install USB-C PD board, 5 V PSU, ESP32, and terminal blocks into the printed enclosure | All required hardware fits and remains mechanically serviceable |
| TC-P2-16 | Mounting and cable routing | Mount the enclosure above the aquarium frame near the lighting area and route all field wiring | Fan and sensor cable runs are short, orderly, and terminate correctly at the enclosure |

### 8.3 Acceptance Tests

| Test ID | Scenario | Procedure | Success Criteria |
|---|---|---|---|
| AT-01 | Autonomous cooling without network | Run production firmware with Wi-Fi and MQTT unavailable | Cooling control remains active locally |
| AT-02 | Persisted configuration resilience | Store target temperature, reboot, then boot without network | Target remains active and cooling still works |
| AT-03 | MQTT observability | Connect broker and inspect published topics | Required state and status topics are published, including normal operation and fault-policy state |
| AT-04 | Remote configuration safety | Publish valid and invalid set commands | Valid values apply and persist; invalid values are rejected |
| AT-05 | FHEM MQTT2 monitoring | Import the FHEM definition against the configured broker/root topic | FHEM receives the expected telemetry readings without issuing control commands |
| AT-06 | Installed fan plausibility | Run controller in actual aquarium installation | Fan plausibility behaves correctly in the real airflow path |
| AT-07 | OTA success path | Enable the OTA maintenance window and upload a newer valid `.bin` firmware image from the local network | Firmware uploads, validates, activates, and reports success |
| AT-08 | OTA failure rollback | Interrupt upload or upload an invalid `.bin` image during update test | Device preserves current working firmware and reports failure |
| AT-09 | Installed enclosure concept | Install the full system in the intended rear upper mounting position | Electronics are centralized, accessible, and cable routing is practical |
| AT-10 | Release versioning and changelog | Inspect a release candidate before publication | Firmware version follows SemVer 2.0.0 and `CHANGELOG.md` follows Keep a Changelog with a matching release entry |

### 8.4 Traceability Matrix

Status interpretation in this matrix:

- `Covered`: completed and verified in the current project phase
- `Bench-verified`: implemented and verified on the current bench setup
- `Implemented`: present in firmware but not yet fully validated in the final installation context
- `Planned`: specified but not yet implemented or verified

| Requirement | Priority | Test Case(s) | Status |
|---|---|---|---|
| FR-1.1 | Must | TC-P1-01 | Covered |
| FR-1.2 | Must | TC-P1-02 | Covered |
| FR-1.3 | Must | TC-P1-03 | Covered |
| FR-1.4 | Must | TC-P1-04 | Covered |
| FR-1.5 | Must | TC-P1-05 | Covered |
| FR-1.6 | Must | TC-P1-06 | Covered |
| FR-2.1 | Must | TC-P2-01 | Bench-verified |
| FR-2.2 | Must | TC-P2-01 | Bench-verified |
| FR-2.3 | Must | TC-P2-02 | Bench-verified |
| FR-2.4 | Must | TC-P2-03 | Bench-verified |
| FR-2.5 | Must | TC-P2-04 | Bench-verified |
| FR-2.6 | Should | TC-P2-02 | Bench-verified |
| FR-2.7 | Must | TC-P2-02, TC-P2-06 | Bench-verified |
| FR-2.8 | Must | TC-P2-06 | Bench-verified |
| FR-2.9 | Must | TC-P2-05, AT-02 | Bench-verified |
| FR-2.10 | Must | TC-P2-05 | Bench-verified |
| FR-2.11 | Must | AT-01, AT-02 | Bench-verified |
| FR-2.12 | Must | TC-P2-15, AT-09 | Planned |
| FR-2.13 | Must | TC-P2-16, AT-09 | Planned |
| FR-2.14 | Should | TC-P2-15, TC-P2-16, AT-09 | Planned |
| FR-3.1 | Must | TC-P2-07, AT-06 | Bench-verified |
| FR-3.2 | Must | TC-P2-07, AT-06 | Bench-verified |
| FR-3.3 | Must | TC-P2-07 | Bench-verified |
| FR-3.4 | Must | TC-P2-07, AT-03 | Bench-verified |
| FR-3.5 | Must | TC-P2-08 | Implemented |
| FR-3.6 | Must | TC-P2-09 | Implemented |
| FR-3.7 | Should | TC-P2-10 | Implemented |
| FR-4.1 | Must | AT-03 | Bench-verified |
| FR-4.2 | Must | AT-04 | Planned |
| FR-4.3 | Should | AT-04 | Planned |
| FR-4.4 | Must | AT-07, AT-08 | Planned |
| FR-4.5 | Must | TC-P2-13, TC-P2-14, AT-07 | Planned |
| FR-4.6 | Must | AT-07, AT-08 | Planned |
| FR-4.7 | Must | AT-08 | Planned |
| FR-4.8 | Should | AT-07, AT-08 | Planned |
| FR-4.9 | Should | AT-05 | Implemented |
| NFR-1.1 | Must | AT-01 | Bench-verified |
| NFR-1.2 | Must | TC-P2-02, AT-06 | Bench-verified |
| NFR-1.3 | Must | TC-P2-05, AT-02 | Bench-verified |
| NFR-1.4 | Must | TC-P2-02, TC-P2-07 | Bench-verified |
| NFR-1.5 | Must | TC-P2-11 | Implemented |
| NFR-1.6 | Should | TC-P2-12 | Implemented |
| NFR-1.7 | Must | TC-P2-07, TC-P2-08, TC-P2-09, AT-03 | Bench-verified |
| NFR-1.8 | Should | TC-P2-13, AT-01, AT-02 | Implemented |
| NFR-1.9 | Must | TC-P1-01 | Covered |
| NFR-1.10 | Must | TC-P2-14, AT-01 | Planned |
| NFR-1.11 | Must | AT-07, AT-08 | Planned |
| NFR-1.12 | Must | AT-10 | Planned |
| NFR-1.13 | Must | AT-10 | Planned |

## 9. Troubleshooting Guide

| Symptom | Likely Cause | Diagnostic Steps | Corrective Action |
|---|---|---|---|
| Fan does not start at low PWM | Start threshold too low or PWM interface incompatible | Run characterization and inspect start PWM result | Increase start-boost or validate PWM electrical stage |
| RPM reads zero while fan spins | Tach wiring or pull-up problem | Check GPIO26 signal, pull-up, and common ground | Correct wiring and confirm pulse measurement |
| Water and air temperature appear swapped | DS18B20 roles assigned by bus order instead of ROM ID | Print detected ROM IDs and compare to configured mapping | Reassign and persist correct ROM-ID mapping |
| False fan faults near low PWM | Unstable tach region below stable operating range | Compare measured RPM to stable hold PWM region | Exclude low-PWM region or widen tolerance there |
| Cooling stops after water sensor issue | Fallback behavior not configured or not applied correctly | Inspect fault logs and safe fallback branch | Implement and verify defined safe fallback PWM |
| MQTT updates seem ignored | Network unavailable or payload invalid | Check broker connection and validation logs | Restore connectivity or send valid payload |
| FHEM readings do not update | FHEM IODev or topic root does not match the broker traffic | Compare the serial `network` root topic with the FHEM `readingList`; inspect broker traffic for `<root>/status/availability` | Correct the IODev, subscription, credentials, or root topic |
| OTA upload page is not reachable | OTA maintenance window is not enabled, Wi-Fi is unavailable, or the window timed out | Check Wi-Fi state, OTA state, and serial/MQTT diagnostics | Enable OTA maintenance mode again or restore Wi-Fi connectivity |
| OTA update fails validation | Corrupt image, wrong firmware image, unsupported image, or image too large for the OTA slot | Inspect OTA result code and firmware release identity diagnostics | Rebuild and upload a valid firmware `.bin` image |
| Cable routing is awkward or too long | Enclosure position or terminal layout is poorly chosen | Inspect mounted enclosure position and wiring paths | Move enclosure or revise terminal placement in the printed design |
| Moisture or heat exposure threatens electronics | Mounting location too close to splash or trapped warm air | Inspect rear upper mounting environment during operation | Add shielding, revise enclosure geometry, or adjust placement |

## 10. Appendix

### A. Current Characterization Parameters

| Constant | Value |
|---|---:|
| PWM frequency | 25000 Hz |
| PWM resolution | 8 bit |
| Tach pulses per revolution | 2 |
| Start search step | 1 % |
| Curve step | 5 % |
| Settle time | 3000 ms |
| Samples per point | 4 |
| RPM start threshold | 150 RPM |
| Stable samples required | 3 |
| Maximum stored curve points | 32 |

### B. Default Naming

| Item | Value |
|---|---|
| Repository name | `aquarium-cooling-controller` |
| Device name | `aquarium-cooling-controller` |
| Hostname | `aq-cooling` |
| MQTT root topic | `aquarium/cooling` |
| FHEM integration | `integrations/fhem/aquarium-cooling-mqtt2-device.cfg` |
| Changelog file | `CHANGELOG.md` |

Local verified MQTT root topic override on `2026-04-16`: `aquarium_cooling`.

### C. Versioning and Changelog Policy

- Production firmware versions shall follow Semantic Versioning 2.0.0:
  `MAJOR.MINOR.PATCH`.
- During initial development, `0.y.z` versions may be used to indicate that
  compatibility can still change before a stable `1.0.0` release.
- Patch versions shall be used for backward-compatible bug fixes.
- Minor versions shall be used for backward-compatible feature additions,
  deprecations, or substantial compatible improvements.
- Major versions shall be used for backward-incompatible changes to the
  documented project surface, including serial commands, MQTT topics,
  persisted configuration semantics, OTA compatibility, or documented control
  behavior.
- Pre-release and build metadata may be used only when the resulting version
  string remains SemVer-compliant.
- `CHANGELOG.md` shall follow Keep a Changelog and keep an `Unreleased`
  section above released versions.
- Changelog entries shall be grouped under the standard headings `Added`,
  `Changed`, `Deprecated`, `Removed`, `Fixed`, and `Security` when those
  categories apply.
- Each published firmware version shall have a corresponding changelog release
  section with the release date.

### D. Mechanical Integration Summary

| Item | Requirement |
|---|---|
| Enclosure type | 3D-printed central electronics housing |
| Internal contents | USB-C PD board, 5 V PSU, ESP32, field terminal blocks |
| External terminations | Fan, water sensor, air sensor |
| Preferred mounting position | Above aquarium frame, rear side, near lighting |
| Design intent | Short cable runs, centralized electronics, serviceable installation |

### E. Wiring Summary

| Signal | Mapping |
|---|---|
| Fan PWM | ESP32 GPIO25 -> fan pin 4 (PWM) |
| Fan TACH | fan pin 3 (TACH) -> ESP32 GPIO26 |
| Fan Power | fan pin 2 -> +12 V, fan pin 1 -> GND |
| DS18B20 bus | ESP32 GPIO33 shared 1-Wire bus with 3.3 kOhm pull-up to 3.3 V |

### F. Verified Fan Characterization Result

Latest verified bench result on `2026-04-12` with `Noctua NF-S12A PWM`:

| Metric | Value |
|---|---:|
| Start PWM from standstill | 12 % |
| Minimum hold PWM while spinning | 10 % |
| RPM at 0 % PWM | 0 |
| RPM at 5 % PWM | 0 |
| RPM at 100 % PWM | 1252 |

Reference artifacts:

- `docs/result fan test/measurement-summary-2026-04-12.md`
- `docs/result fan test/live-run.txt`
- `docs/result fan test/live-run-part2.txt`

Verified upward curve:

```cpp
FanCurvePoint curve[] = {
  {12, 187},
  {17, 277},
  {22, 352},
  {27, 435},
  {32, 517},
  {37, 570},
  {42, 637},
  {47, 697},
  {52, 765},
  {57, 832},
  {62, 885},
  {67, 930},
  {72, 997},
  {77, 1050},
  {82, 1102},
  {87, 1147},
  {92, 1185},
  {97, 1230},
  {100, 1252},
};
```

### G. Open Points Requiring Finalization

- Final plausibility tolerance percentage after measured tuning
- Final hardware implementation of PWM electrical compatibility
- Whether manual override remains enabled in production firmware
- Whether installed airflow requires a separate in-situ fan curve
- Exact OTA upload window duration, enable command surface, and validation
  policy details
- Exact enclosure geometry, mounting method, and splash-protection details

### H. Draft Schematic Sketch

- Mermaid source: `docs/design/schematic-sketch.mmd`
- Usage: paste the Mermaid source into draw.io via `Arrange -> Insert ->
  Advanced -> Mermaid`
- Scope: block-level sketch for architecture and wiring orientation, not yet a
  final electrical schematic
