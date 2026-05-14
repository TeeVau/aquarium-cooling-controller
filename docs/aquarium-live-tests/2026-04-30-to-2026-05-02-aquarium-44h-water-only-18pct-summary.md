# Aquarium Live Test - 2026-04-30 to 2026-05-02

## Scope

First installed run of the simplified water-only hysteresis firmware after the
cutover on `2026-04-30 23:23:21`.

This phase still used a single active cooling stage of `18% PWM`.

Raw export:

- `docs/aquarium-live-tests/2026-04-30-to-2026-05-02-aquarium-44h-water-only-18pct-fhem-export.csv`

Known aquarium light schedule during this phase:

- light on: `15:00`
- light off: `22:00`

## Capture Window

| Item | Value |
|---|---:|
| First sample | `2026-04-30 23:24:31` |
| Last sample | `2026-05-02 19:35:01` |
| Duration | `44.2 h` |
| Export rows | `23366` |
| Firmware observed in log | `0.1.4` |

## Key Results

This run confirmed that the first water-only hysteresis controller was much
calmer than the earlier air-assist generation, but also showed that `18% PWM`
was too weak as the only active cooling stage.

### Water and Air

| Signal | Min | Max | Mean |
|---|---:|---:|---:|
| Water temperature | `23.8 C` | `24.6 C` | `24.18 C` |
| Air temperature | `23.1 C` | `34.1 C` | `27.29 C` |

Phase split:

| Phase | Water mean | Air mean | Fan PWM sample mean |
|---|---:|---:|---:|
| Light on (`15:00-22:00`) | `24.31 C` | `32.52 C` | `18.00%` |
| Light off | `24.14 C` | `24.49 C` | `17.67%` |

## Fan Runtime Observation

Observed PWM values were effectively binary in this phase:

- `0%`
- `18%`

Estimated runtime from sampled PWM intervals:

| Signal | Value |
|---|---:|
| Observed fan runtime | `92.0%` |
| Starts | `2` |
| Stops | `1` |
| Longest continuous run | `1716.2 min` |

This was the decisive result of the run: the controller was quiet and stable,
but the fan stayed active far too long for the available cooling reserve.

## Fault Observation

The known low-PWM plausibility issue was already much smaller than before, but
not yet fully gone:

| Signal | Value |
|---|---:|
| Observed fan-fault time | `8.3 min` |
| Observed fan-fault share | `0.32%` |

## Interpretation

This log led directly to the next tuning step:

1. The water-only strategy itself was confirmed.
2. The fixed low stage of `18%` was judged too weak.
3. The next firmware iteration should keep the same hysteresis but raise the
   active cooling stage.

## Follow-Up

This phase ends with the later cutover to the stronger fixed cooling stage:

- cutover to `22%` water-only low stage: `2026-05-02 21:56:23`

