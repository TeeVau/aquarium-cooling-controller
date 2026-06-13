# Aquarium Live Test - 2026-05-14 to 2026-06-13

## Scope

First longer installed run after the cutover to firmware `0.1.6`.

This export does not represent one static tuning point. It captures the first
month of the staged water-only controller while the active PWM stages were tuned
live over MQTT.

Raw export:

- `docs/aquarium-live-tests/2026-05-14-to-2026-06-13-aquarium-30d-staged-control-tuning-fhem-export.csv`

Known aquarium light schedule during this phase:

- light on: `15:00`
- light off: `22:00`

## Capture Window

| Item | Value |
|---|---:|
| First sample | `2026-05-14 11:03:27` |
| Last sample | `2026-06-13 11:12:23` |
| Window length | `30.0 d` |
| Export rows | `52896` |
| Firmware transition in log | `0.1.5 -> 0.1.6` on `2026-05-14 11:53:40` |
| Logging gaps > 1 h | `0` |

## Important Context

This log contains a real tuning period, not just a passive observation run.

The active live controller at the end of the capture confirmed:

- `target_temp_c = 24.0`
- `cooling_on_delta_c = +0.5 C`
- `cooling_off_delta_c = -0.5 C`
- `high_cooling_delta_c = +1.0 C`
- `fan_low_pwm_percent = 45`
- `fan_high_pwm_percent = 60`
- `firmware_version = 0.1.6`

The export also still contains a few legacy MQTT status/state topics such as
`air_assist_enable`, `air_min_pwm_percent`, and `air_sensor_ok`, even though the
active control strategy is already water-only.

## Overall Observation

As one aggregate dataset, the month is too mixed to judge one single operating
point:

| Signal | Value |
|---|---:|
| Water temperature min / mean / max | `19.2 / 24.02 / 27.4 C` |
| Fan PWM sample mean | `22.06%` |
| Observed fan runtime | `43.32%` |
| Fan starts / stops | `26 / 26` |

The extreme water values are not representative of normal control behavior and
almost certainly include maintenance or disturbance events. The useful
conclusion comes from separating the month into tuning phases.

## Phase Breakdown

### Phase A - Initial `0.1.6` defaults (`22 / 35`)

Window:

- `2026-05-14 11:53:40` to `2026-05-27 17:09:14`

Behavior:

- observed PWM levels: `0`, `22`, `35`, rare short spikes to `60/80/100`
- control modes: `fan-off`, `fan-low`, `fan-high`
- no observed fan faults

Results:

| Signal | Value |
|---|---:|
| Water mean (filtered `>= 22 C`) | `24.00 C` |
| Water band `23.5 .. 24.5 C` | `47.22%` |
| Water above `24.5 C` | `26.71%` |
| Observed runtime | `37.28%` |
| Longest continuous run | `96.99 h` |
| Fan-fault events | `0` |

Interpretation:

- the staged architecture itself worked
- thermals were still too warm too often
- `22 / 35` was robust but too conservative

### Phase B - Intermediate `40%` tuning window

Window:

- `2026-05-27 17:09:15` to `2026-05-31 21:34:10`

Behavior:

- observed PWM levels: `0`, `40`
- only `fan-off` and `fan-low` were meaningfully active

Results:

| Signal | Value |
|---|---:|
| Water mean | `24.05 C` |
| Water band `23.5 .. 24.5 C` | `100%` |
| Observed runtime | `71.55%` |
| Longest continuous run | `53.72 h` |
| Fan-fault events | `37` |

Interpretation:

- thermally excellent
- diagnostically noisy
- too much runtime and too many plausibility faults

### Phase C - Stabilized `45 / 60` field tuning

Window:

- `2026-05-31 21:34:11` to `2026-06-13 11:12:23`

Behavior:

- observed PWM levels: mainly `0` and `45`, one short `60`
- almost all active cooling happened in `fan-low`

Results:

| Signal | Value |
|---|---:|
| Water mean (filtered `>= 22 C`) | `24.05 C` |
| Water band `23.5 .. 24.5 C` | `99.59%` |
| Water below `23.5 C` | `0.24%` |
| Water above `24.5 C` | `0.17%` |
| Observed runtime | `40.35%` |
| Longest continuous run | `13.67 h` |
| Fan-fault events | `1` |

Interpretation:

- this was the best overall operating point of the month
- water stayed almost perfectly in the desired band
- runtime was far lower than the old `0.1.5` `22%` phase
- the remaining fault behavior was negligible in practice

## Comparison to the Last Archived `0.1.5` Reference

Reference run:

- `docs/aquarium-live-tests/2026-05-02-to-2026-05-13-aquarium-11d-water-only-22pct-summary.md`

Comparison against the best `0.1.6` phase (`45 / 60`):

| Signal | `0.1.5` fixed `22%` | `0.1.6` tuned `45 / 60` |
|---|---:|---:|
| Water mean | `24.11 C` | `24.05 C` |
| Water band `23.5 .. 24.5 C` | `93.5%` | `99.59%` |
| Observed runtime | `85.4%` | `40.35%` |
| Fan faults | `0` | `1` |

This was a major field improvement:

- tighter water control
- far less fan runtime
- still practically fault-free

## Conclusions

This month-long export supported four key conclusions:

1. The staged `0.1.6` water-only architecture was the right direction.
2. The original `22 / 35` defaults were still too weak for the installed
   aquarium.
3. The intermediate `40%` experiment was too aggressive and triggered too many
   plausibility faults.
4. The later live-tuned `45 / 60` combination was the best observed operating
   point and should be treated as the current field baseline unless later runs
   prove otherwise.
