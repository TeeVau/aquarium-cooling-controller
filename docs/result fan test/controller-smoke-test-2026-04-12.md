# Controller Smoke Test

Date: `2026-04-12`

## Scope

First bench smoke test of the production-controller scaffold in
[`firmware/controller/`](../../firmware/controller/).

Validated areas:

- ESP32 compile and flash
- PWM output bring-up on `GPIO25`
- tachometer RPM measurement on `GPIO26`
- fan start-boost behavior
- fan-curve interpolation and plausibility diagnostics
- serial command handling for safe bench control

## Bench Setup

- Controller board: `ESP32`
- Fan: `Noctua NF-S12A PWM`
- PWM pin: `GPIO25`
- Tach pin: `GPIO26`
- Tach pull-up: `3.3 kOhm` to `3.3 V`
- PWM frequency: `25 kHz`
- Upload port: `COM3`

## Toolchain Observed During Test

- `arduino-cli` target: `esp32:esp32:esp32`
- active ESP32 Arduino core reported by the running firmware: `3.3.7`

Note:

- The Arduino IDE had reportedly been updated, but the actual core seen by
  `arduino-cli` and the device during this test was still `3.3.7`.

## Firmware Behavior Confirmed

- Controller booted successfully and printed the curve summary.
- Fan driver initialized successfully.
- RPM monitor initialized successfully.
- The controller started in a safe `0%` PWM state.
- Serial commands were available: `0..100`, `stop`, `status`, `help`.

## Interactive Fan Test

Test sequence:

1. Flash the controller scaffold.
2. Open the serial connection.
3. Send `15`.
4. Observe start-boost and tach feedback.
5. Send `stop`.

Observed results:

- `15%` target PWM was accepted.
- Start-boost activated at `40%` PWM.
- After boost, applied PWM settled to `15%`.
- Measured RPM settled near the characterized curve:
  - transient readings included `540 RPM` and `270 RPM`
  - stable reading converged to about `240 RPM`
- The expected interpolated RPM at `15%` was reported as `241 RPM`.
- `stop` returned the fan cleanly to `0 RPM`.

## Captured Serial Artifacts

- startup log:
  - [`controller-startup-2026-04-12.txt`](./controller-startup-2026-04-12.txt)

## Outcome

The first production-controller scaffold is confirmed to:

- compile and flash successfully
- control the fan safely from a stopped state
- measure tach RPM on real hardware
- produce useful plausibility diagnostics against the measured fan curve

## Recommended Next Step

Add the DS18B20 `sensor_manager` and replace manual serial PWM commands with the
first simple local temperature-based control path.
