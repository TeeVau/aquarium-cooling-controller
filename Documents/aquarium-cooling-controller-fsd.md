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
and plausibility-based fault detection.

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
6. Wi-Fi and MQTT publish telemetry, perform OTA update checks/downloads, and
   optionally update non-critical settings.
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
- OTA update client
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
8. OTA client checks for approved updates and coordinates safe firmware
   download and activation.

### 2.2 Hardware / Platform Architecture

| Element | Selection | Role |
|---|---|---|
| MCU | ESP32-WROOM-32E on ESP32-DevKitC V4 | Main controller |
| Fan | 120 mm 4-pin PWM fan, e.g. Noctua NF-S12A PWM | Cooling actuator |
| Water sensor | DS18B20 | Primary control variable |
| Air sensor | DS18B20 | Secondary air-assist trigger |
| Power input | USB-C PD trigger requesting 12 V | Single-cable power source |
| ESP32 supply | Buck converter from 12 V to 5 V | Controller supply |
| 1-Wire pull-up | 4.7 kOhm to 3.3 V | Bus biasing |
| Tach pull-up | 4.7 kOhm to 3.3 V | Open-collector tach input biasing |

Pin assignment:

| Signal | ESP32 GPIO | Notes |
|---|---:|---|
| Fan PWM | 18 | 25 kHz PWM output |
| Fan TACH | 19 | Interrupt-capable input with pull-up |
| 1-Wire bus | 4 | Shared bus for both DS18B20 sensors |

Hardware constraints:

- Fan supply shall remain at 12 V while the ESP32 logic remains at 3.3 V/5 V.
- Common ground between fan power and ESP32 shall be mandatory.
- The final PWM output stage must be electrically compatible with 4-pin PC fan
  PWM input requirements.
- The tach signal shall be treated as open-collector/open-drain style feedback.

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
| `telemetry` | Wi-Fi, MQTT publish/subscribe, diagnostics |
| `ota_client` | OTA metadata lookup, download, validation, update handoff |

Boot sequence:

1. Initialize serial diagnostics and hardware GPIO.
2. Initialize PWM output and tach interrupt.
3. Load persisted configuration from Preferences/NVS.
4. Validate configuration and fall back to defaults if invalid.
5. Start local control and safety monitoring.
6. Attempt Wi-Fi/MQTT connectivity after local control is operational.
7. Enable OTA client functionality only after local control startup has
   completed.

Persistence model:

- Storage backend: ESP32 Preferences / NVS
- Persisted keys: target temperature, air-assist enable flag,
  air-assist minimum PWM, optional controller tuning values
- Sensor role mapping shall use DS18B20 ROM IDs rather than bus order

Update model:

- Characterization sketch is a standalone firmware artifact.
- Production firmware is a separate artifact that reuses measured fan-curve
  data.
- Production firmware shall support OTA updates over the WLAN client.
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

Exit criteria:

- Sketch compiles for ESP32 Arduino
- Sketch runs without manual serial commands
- Start PWM is reported
- Hold PWM is reported
- Up and down curves are printed
- Measured data is reusable in later firmware

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
- Finalized fault-reaction policy for confirmed fan fault

### 3.3 Phase 3 - Networked Observability and Remote Configuration

Scope:

- Add Wi-Fi connectivity
- Publish telemetry and fault state over MQTT
- Add OTA firmware update capability over the WLAN client
- Accept validated remote configuration updates
- Keep local control fully autonomous during network outages

Deliverables:

- MQTT topic implementation
- OTA update workflow
- Telemetry and status publishing
- Validated remote settings workflow

Exit criteria:

- MQTT publishes required state and diagnostics
- OTA update can be downloaded and applied over Wi-Fi
- Approved remote settings are persisted
- Network loss does not interrupt cooling autonomy

Dependencies:

- Phase 2 production controller complete
- MQTT broker, OTA endpoint, and Wi-Fi credentials available

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
  status over MQTT.
- FR-4.2 [Must]: The production controller shall accept validated remote
  updates for target temperature and selected non-critical control flags over
  MQTT.
- FR-4.3 [Should]: The production controller should support a manual override
  mode for service or testing with explicit validation and clear state
  reporting.
- FR-4.4 [Must]: The production controller shall support OTA firmware updates
  over the WLAN client.
- FR-4.5 [Must]: The production controller shall start OTA activity only after
  local cooling control is already active.
- FR-4.6 [Must]: The production controller shall validate OTA metadata and
  firmware image integrity before activating a downloaded firmware image.
- FR-4.7 [Must]: The production controller shall preserve the last working
  firmware if OTA download, validation, or activation fails.
- FR-4.8 [Should]: The production controller should publish OTA state and last
  update result through diagnostics or MQTT status topics.

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
  firmware image is activated.

### 4.3 Constraints

- System only cools; it does not provide heating.
- No local display or button interface is planned.
- Water sensor role and air sensor role shall not depend on DS18B20 bus order.
- The final PWM electrical interface must be validated with the selected fan.
- Fan plausibility tolerance and confirmed fan-fault reaction are not yet fully
  finalized.
- Low-PWM regions may require wider tolerance or exclusion from plausibility
  checking.

## 5. Risks, Assumptions & Dependencies

### Technical Risks

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| Fan PWM electrical interface is not fully compatible with the selected fan | Medium | High | Validate prototype waveform and startup behavior before production |
| Low-PWM tach readings are unstable and trigger false faults | High | Medium | Exclude unstable region or widen tolerance below stable PWM |
| Under-lid airflow differs from free-air characterization | Medium | Medium | Verify curve once in free air and again in installed configuration |
| DS18B20 bus or role mapping errors swap water and air sensors | Low | High | Assign roles by ROM ID and verify mapping at commissioning |
| Fault reaction for confirmed fan fault remains underspecified | Medium | High | Finalize explicit reaction before production release |

### Assumptions

- The selected fan provides two tach pulses per revolution, matching the test
  sketch assumption.
- A fixed initial plausibility tolerance around +/-12% is a suitable starting
  point until measured tuning data is available (assumed).
- The aquarium lid has both a dedicated fan opening and a separate air outlet,
  allowing effective airflow (assumed from project notes).
- The OTA source will be reachable through the same Wi-Fi client connectivity
  used for telemetry and remote configuration (assumed).

### External Dependencies

- ESP32 Arduino framework
- Preferences library
- OneWire library
- DallasTemperature library
- WiFi library
- MQTT client library such as PubSubClient or equivalent
- OTA-capable update library or ESP32 OTA support compatible with the chosen
  Arduino framework
- USB-C PD trigger hardware
- Selected 4-pin PWM fan and DS18B20 sensors

### Environmental Constraints

- Covered aquarium environment with moisture and heat near the lid
- Single-cable compact power delivery preferred
- Low audible noise prioritized over maximum airflow

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
| OTA endpoint | ESP32 <- update service | Firmware metadata and image download over Wi-Fi |

#### MQTT Topic Proposal

| Topic | Direction | Purpose |
|---|---|---|
| `aquarium/cooling/state/water_temp_c` | publish | Water temperature |
| `aquarium/cooling/state/air_temp_c` | publish | Air temperature |
| `aquarium/cooling/state/fan_pwm_percent` | publish | Commanded fan PWM |
| `aquarium/cooling/state/fan_rpm` | publish | Measured fan RPM |
| `aquarium/cooling/state/target_temp_c` | publish | Active target temperature |
| `aquarium/cooling/state/controller_mode` | publish | Controller mode |
| `aquarium/cooling/status/fan_plausible` | publish | Plausibility state |
| `aquarium/cooling/status/fan_fault` | publish | Latched fan fault |
| `aquarium/cooling/status/water_sensor_ok` | publish | Water sensor health |
| `aquarium/cooling/status/air_sensor_ok` | publish | Air sensor health |
| `aquarium/cooling/status/alarm_code` | publish | Fault summary |
| `aquarium/cooling/set/target_temp_c` | subscribe | Remote target temperature |
| `aquarium/cooling/set/air_assist_enable` | subscribe | Air-assist enable flag |
| `aquarium/cooling/set/air_min_pwm_percent` | subscribe | Minimum air-assist PWM |

#### OTA Update Interface

| Element | Direction | Purpose |
|---|---|---|
| OTA manifest or version endpoint | ESP32 <- service | Check whether a newer approved firmware exists |
| OTA firmware image URL | ESP32 <- service | Download candidate firmware image |
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
| `fault_monitor` | `telemetry` | Diagnostic and fault state |

### 6.3 Data Models / Schemas

#### Fan Curve Point

| Field | Type | Description |
|---|---|---|
| `pwm_percent` | `uint8_t` | PWM command in percent |
| `rpm` | `uint16_t` | Measured RPM |

#### Persisted Configuration

| Key | Type | Description |
|---|---|---|
| `target_temperature_c` | float | Desired water temperature |
| `air_assist_enabled` | bool | Enable under-lid air-assist logic |
| `air_assist_min_pwm_percent` | uint8_t | Minimum PWM when air-assist is active |
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

### 6.4 Commands / Opcodes

The system does not define a custom binary command protocol. Production command
inputs are represented by validated MQTT set topics, OTA metadata requests, and
firmware-image download transactions over Wi-Fi. Characterization mode is fully
automatic with no runtime command input.

## 7. Operational Procedures

### Deployment / Flashing

1. Assemble the ESP32, fan power path, tach pull-up, PWM interface, and 1-Wire
   bus.
2. Verify common ground between the fan supply and ESP32.
3. Flash the characterization sketch for first article testing.
4. Record the measured fan curve and minimum stable PWM data.
5. Integrate measured values into production firmware.
6. Flash production firmware after control and fault parameters are finalized.
7. Configure the OTA endpoint and verify the OTA path after Wi-Fi integration is
   available.

### Provisioning / Configuration

1. Assign DS18B20 water and air roles by ROM ID.
2. Store the default target temperature and air-assist settings in NVS.
3. Validate persisted values at boot.
4. In production mode, connect to Wi-Fi and MQTT only after local control is
   active.
5. Configure OTA endpoint, update policy, and any required credentials before
   enabling OTA in production.

### Normal Operation

1. Sample water and air temperature at the configured interval.
2. Compute water-based cooling demand.
3. Compute air-assist minimum PWM if enabled.
4. Apply the maximum of both demands.
5. Allow settling time after PWM changes before plausibility evaluation.
6. Measure fan RPM and evaluate plausibility where valid.
7. Publish telemetry and accept validated remote settings when connected.
8. Check for OTA updates only as a non-critical background activity while local
   control remains active.

### Maintenance Procedures

1. Re-run fan characterization if the fan model changes.
2. Re-run characterization if the installed airflow path changes materially.
3. Inspect diagnostics for drift between expected and measured RPM.
4. Verify sensor ROM-ID assignment after replacing a DS18B20 sensor.
5. Trigger and verify OTA updates only when the system is in a stable operating
   condition and the update source is trusted.

### Recovery Procedures

1. On invalid persisted configuration, reset affected keys to defaults and log
   the recovery.
2. On water-sensor failure, enter the defined safe fallback PWM and raise an
   alarm.
3. On air-sensor failure, continue water-based control and suppress air-assist.
4. On network failure, continue local control with the last valid persisted
   settings.
5. On confirmed fan fault, enter the project-defined fault response and report
   the condition locally and over MQTT when available.
6. On OTA download or validation failure, remain on the currently working
   firmware and report the failed update state.

## 8. Verification & Validation

### 8.1 Phase 1 Verification

| Test ID | Feature | Procedure | Success Criteria |
|---|---|---|---|
| TC-P1-01 | Standalone characterization | Flash `fan-test.ino` and boot with connected fan | Sketch starts without requiring serial input |
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
| TC-P2-14 | OTA non-blocking startup | Boot with OTA enabled and reachable or unreachable OTA endpoint | Local cooling starts before OTA activity begins |

### 8.3 Acceptance Tests

| Test ID | Scenario | Procedure | Success Criteria |
|---|---|---|---|
| AT-01 | Autonomous cooling without network | Run production firmware with Wi-Fi and MQTT unavailable | Cooling control remains active locally |
| AT-02 | Persisted configuration resilience | Store target temperature, reboot, then boot without network | Target remains active and cooling still works |
| AT-03 | MQTT observability | Connect broker and inspect published topics | Required state and status topics are published |
| AT-04 | Remote configuration safety | Publish valid and invalid set commands | Valid values apply and persist; invalid values are rejected |
| AT-05 | Installed fan plausibility | Run controller in actual aquarium installation | Fan plausibility behaves correctly in the real airflow path |
| AT-06 | OTA success path | Offer a newer valid firmware image via Wi-Fi OTA endpoint | Firmware downloads, validates, activates, and reports success |
| AT-07 | OTA failure rollback | Interrupt or invalidate OTA image during update test | Device preserves current working firmware and reports failure |

### 8.4 Traceability Matrix

| Requirement | Priority | Test Case(s) | Status |
|---|---|---|---|
| FR-1.1 | Must | TC-P1-01 | Covered |
| FR-1.2 | Must | TC-P1-02 | Covered |
| FR-1.3 | Must | TC-P1-03 | Covered |
| FR-1.4 | Must | TC-P1-04 | Covered |
| FR-1.5 | Must | TC-P1-05 | Covered |
| FR-1.6 | Must | TC-P1-06 | Covered |
| FR-2.1 | Must | TC-P2-01 | Covered |
| FR-2.2 | Must | TC-P2-01 | Covered |
| FR-2.3 | Must | TC-P2-02 | Covered |
| FR-2.4 | Must | TC-P2-03 | Covered |
| FR-2.5 | Must | TC-P2-04 | Covered |
| FR-2.6 | Should | TC-P2-02 | Covered |
| FR-2.7 | Must | TC-P2-02, TC-P2-06 | Covered |
| FR-2.8 | Must | TC-P2-06 | Covered |
| FR-2.9 | Must | TC-P2-05, AT-02 | Covered |
| FR-2.10 | Must | TC-P2-05 | Covered |
| FR-2.11 | Must | AT-01, AT-02 | Covered |
| FR-3.1 | Must | TC-P2-07, AT-05 | Covered |
| FR-3.2 | Must | TC-P2-07, AT-05 | Covered |
| FR-3.3 | Must | TC-P2-07 | Covered |
| FR-3.4 | Must | TC-P2-07, AT-03 | Covered |
| FR-3.5 | Must | TC-P2-08 | Covered |
| FR-3.6 | Must | TC-P2-09 | Covered |
| FR-3.7 | Should | TC-P2-10 | Covered |
| FR-4.1 | Must | AT-03 | Covered |
| FR-4.2 | Must | AT-04 | Covered |
| FR-4.3 | Should | AT-04 | Covered |
| FR-4.4 | Must | AT-06, AT-07 | Covered |
| FR-4.5 | Must | TC-P2-13, TC-P2-14, AT-06 | Covered |
| FR-4.6 | Must | AT-06, AT-07 | Covered |
| FR-4.7 | Must | AT-07 | Covered |
| FR-4.8 | Should | AT-06, AT-07 | Covered |
| NFR-1.1 | Must | AT-01 | Covered |
| NFR-1.2 | Must | TC-P2-02, AT-05 | Covered |
| NFR-1.3 | Must | TC-P2-05, AT-02 | Covered |
| NFR-1.4 | Must | TC-P2-02, TC-P2-07 | Covered |
| NFR-1.5 | Must | TC-P2-11 | Covered |
| NFR-1.6 | Should | TC-P2-12 | Covered |
| NFR-1.7 | Must | TC-P2-07, TC-P2-08, TC-P2-09, AT-03 | Covered |
| NFR-1.8 | Should | TC-P2-13, AT-01, AT-02 | Covered |
| NFR-1.9 | Must | TC-P1-01 | Covered |
| NFR-1.10 | Must | TC-P2-14, AT-01 | Covered |
| NFR-1.11 | Must | AT-06, AT-07 | Covered |

## 9. Troubleshooting Guide

| Symptom | Likely Cause | Diagnostic Steps | Corrective Action |
|---|---|---|---|
| Fan does not start at low PWM | Start threshold too low or PWM interface incompatible | Run characterization and inspect start PWM result | Increase start-boost or validate PWM electrical stage |
| RPM reads zero while fan spins | Tach wiring or pull-up problem | Check GPIO 19 signal, pull-up, and common ground | Correct wiring and confirm pulse measurement |
| Water and air temperature appear swapped | DS18B20 roles assigned by bus order instead of ROM ID | Print detected ROM IDs and compare to configured mapping | Reassign and persist correct ROM-ID mapping |
| False fan faults near low PWM | Unstable tach region below stable operating range | Compare measured RPM to stable hold PWM region | Exclude low-PWM region or widen tolerance there |
| Cooling stops after water sensor issue | Fallback behavior not configured or not applied correctly | Inspect fault logs and safe fallback branch | Implement and verify defined safe fallback PWM |
| MQTT updates seem ignored | Network unavailable or payload invalid | Check broker connection and validation logs | Restore connectivity or send valid payload |
| OTA update does not start | OTA endpoint unreachable or update policy not satisfied | Check Wi-Fi state, OTA endpoint, and OTA diagnostics | Restore connectivity or correct OTA configuration |
| OTA update fails validation | Corrupt image, metadata mismatch, or unsupported image | Inspect OTA result code and firmware metadata | Rebuild or republish a valid firmware image |

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

### C. Open Points Requiring Finalization

- Final plausibility tolerance percentage after measured tuning
- Final reaction to confirmed fan fault in production mode
- Final hardware implementation of PWM electrical compatibility
- Whether manual override remains enabled in production firmware
- Whether installed airflow requires a separate in-situ fan curve
- Exact OTA manifest format, approval policy, and authentication details
