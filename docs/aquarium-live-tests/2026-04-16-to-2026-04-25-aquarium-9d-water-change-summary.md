# Aquarium Live Test - 2026-04-16 to 2026-04-25

## Scope

Longer multi-day aquarium FHEM/DB export covering the first installed control
phase, including the later water-change event on `2026-04-25`.

Raw export:

- `docs/aquarium-live-tests/2026-04-16-to-2026-04-25-aquarium-9d-water-change-fhem-export.csv`

Known aquarium light schedule during this phase:

- light on: `15:00`
- light off: `22:00`

Water change on `2026-04-25`:

- lid opened
- filter switched off
- water drained until the temperature probe was eventually no longer submerged
- cooler tap water added
- filter switched on again
- lid closed

## Capture Window

| Item | Value |
|---|---:|
| First sample | `2026-04-16 18:04:13` |
| Last sample | `2026-04-25 19:45:55` |
| Duration | `217.7 h` |
| Export rows | `188497` |

## Key Results

The long run shows that the controller kept the aquarium water stable over
multiple days, but also confirmed that the original air-assist path could keep
the fan running even when water was already below the intended target band.

### Stable Multi-Day Window

For the regular operating window before the water-change disturbance
(`2026-04-18` to `2026-04-24`), the water signal stayed tightly grouped:

| Signal | Min | Max | Mean | Median | Notes |
|---|---:|---:|---:|---:|---|
| Water temperature | `23.44 C` | `24.38 C` | `24.17 C` | `24.19 C` | Stable around the expected `24 C` target |
| Air temperature | `23.31 C` | `29.38 C` | `25.56 C` | n/a | Strongly follows lid heating and room conditions |
| Fan PWM | `0%` | n/a | `12.53%` | `12%` | Fan active often, even in otherwise stable operation |

### Light-On vs Light-Off Behavior

Within that same stable window:

| Phase | Water mean | Air mean | Fan PWM mean | Notes |
|---|---:|---:|---:|---|
| Light on (`15:00-22:00`) | `24.07 C` | `28.42 C` | `35.20%` | Air-assist kept airflow active against warm lid air |
| Light off | `24.20 C` | `24.20 C` | `6.82%` | Water was actually slightly warmer overnight |

Interpretation: the original control strategy clearly reduced warm lid air, but
it also tended to cool the water harder during the light phase than was really
necessary for the aquarium.

## Water Change Observation

The water change on `2026-04-25` appears very clearly in the export and should
be treated as an intentional disturbance, not as a normal control segment.

Key milestones:

| Event | Timestamp |
|---|---|
| Water falls below `24.0 C` | `2026-04-25 13:59:15` |
| Water falls below `23.0 C` | `2026-04-25 13:59:55` |
| Water falls below `22.0 C` | `2026-04-25 14:01:05` |
| Minimum water temperature | `2026-04-25 14:08:15` |

Minimum observed water temperature:

- `20.94 C`

This drop is too fast to represent the full `120 L` water mass alone. It is
consistent with the known maintenance workflow:

- sensor temporarily exposed or partially exposed to air
- very cold refill water locally near the probe
- reduced mixing while the filter was off

## Air-Assist Observation

The most important control observation from this export happened *after* the
water change:

| Timestamp | Observation |
|---|---|
| `2026-04-25 15:09:45` | `controller_mode = water-control+air-assist`, `fan_pwm_percent = 20`, `water_temp_c = 21.81 C` |

Interpretation: the fan resumed because warm lid air returned, even though the
water was still far below the intended target region. This strongly confirmed
that air temperature should not remain an equal control driver next to water
temperature.

## Fan / Plausibility Observation

The long run also reinforced the earlier plausibility finding: repeated
`fan_fault` intervals were mostly a low-PWM diagnostics problem, not evidence
of real repeated cooling failure.

The water stayed stable over days while these events occurred, which indicates a
plausibility/tolerance issue rather than a persistent mechanical fan fault.

## MQTT / Availability

Only short reconnect events were observed. No long network outage was seen that
would challenge local autonomous control.

## Current Interpretation

This export led to two important decisions:

1. Water temperature should remain the main control variable.
2. Air temperature may still be useful as telemetry, but not as a direct fan
   demand source in the main regulation path.

These findings motivated the later no-air-assist comparison run and, after that,
the switch to the newer water-only hysteresis firmware generation.
