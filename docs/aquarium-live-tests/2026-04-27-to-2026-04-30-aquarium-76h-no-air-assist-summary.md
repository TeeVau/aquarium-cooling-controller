# Aquarium Live Test - 2026-04-27 to 2026-04-30

## Scope

Dedicated no-air-assist comparison run after disabling the old air-assist
contribution on `2026-04-27 16:48:41`.

Raw export:

- `docs/aquarium-live-tests/2026-04-27-to-2026-04-30-aquarium-76h-no-air-assist-fhem-export.csv`

Known aquarium light schedule during this phase:

- light on: `15:00`
- light off: `22:00`

## Capture Window

| Item | Value |
|---|---:|
| First sample | `2026-04-27 16:48:51` |
| Last sample | `2026-04-30 20:33:01` |
| Duration | `75.7 h` |
| Export rows | `68122` |

## Key Results

This run shows that the aquarium water stayed within an acceptable range even
without air-assist, but the lid air became much warmer during the light phase.

### Water Stability Without Air-Assist

Daily water ranges stayed moderate:

| Date | Water range | Mean | Notes |
|---|---:|---:|---|
| `2026-04-28` | `24.2 .. 24.5 C` | `24.35 C` | Stable |
| `2026-04-29` | `24.2 .. 24.8 C` | `24.41 C` | Slightly warmer than the earlier air-assist phase |
| `2026-04-30` | `24.2 .. 24.6 C` | `24.33 C` | Still within an acceptable aquarium band |

Interpretation: air-assist was not required to keep the water generally under
control.

## Comparison Against the Earlier Air-Assist Phase

Compared with the earlier stable air-assist window (`2026-04-18` to
`2026-04-24`):

### Light-On Phase (`15:00-22:00`)

| Mode | Water mean | Air mean | Fan PWM mean |
|---|---:|---:|---:|
| With air-assist | `24.07 C` | `28.42 C` | `35.20%` |
| Without air-assist | `24.45 C` | `32.32 C` | `18.02%` |

Difference without air-assist:

- water: about `+0.38 C`
- air: about `+3.89 C`
- fan PWM: about `-17.18` percentage points

### Night / Light-Off Phase

| Mode | Water mean | Air mean | Fan PWM mean |
|---|---:|---:|---:|
| With air-assist | `24.20 C` | `24.20 C` | `6.82%` |
| Without air-assist | `24.30 C` | `24.64 C` | `9.68%` |

Interpretation: the meaningful difference is mainly in the light phase, not at
night.

## Lid Air Observation

Without air-assist, the under-lid air got clearly hotter:

| Signal | Value |
|---|---:|
| Light-on air mean | `32.32 C` |
| Maximum air temperature | `35.8 C` |

This means air exchange does have a real physical effect. It just influences
the water much less strongly than it influences the air itself.

## Fan / Plausibility Observation

The known `fan_fault` topic did not disappear in the no-air-assist phase.
Fault intervals were still concentrated in the low-PWM region, especially
during light-off / low-demand operation.

Interpretation: low-PWM fan plausibility is a separate diagnostics issue and is
not caused by air-assist.

## Current Interpretation

This comparison run led to a clearer control conclusion:

1. Air-assist is not necessary to keep the water broadly stable.
2. Air-assist is effective at lowering warm lid air.
3. The old implementation gave air temperature too much authority over fan
   runtime.

That made a simpler water-led strategy the better next step:

- water remains the only active control input
- wider hysteresis is acceptable for the aquarium
- a quiet fixed fan stage is preferred over constant PWM reshaping

## Follow-Up

The next controller phase begins later on:

- cutover to water-only hysteresis firmware: `2026-04-30 23:23:21`

Subsequent log analysis should treat this file as the final pre-cutover
reference for the no-air-assist experiment.
