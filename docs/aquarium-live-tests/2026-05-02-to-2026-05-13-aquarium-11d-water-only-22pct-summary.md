# Aquarium Live Test - 2026-05-02 to 2026-05-13

## Scope

Longer installed run after raising the fixed water-only cooling stage from
`18%` to `22% PWM`.

Raw export:

- `docs/aquarium-live-tests/2026-05-02-to-2026-05-13-aquarium-11d-water-only-22pct-fhem-export.csv`

Known aquarium light schedule during this phase:

- light on: `15:00`
- light off: `22:00`

## Capture Window

| Item | Value |
|---|---:|
| First sample | `2026-05-02 21:56:23` |
| Last sample | `2026-05-13 22:14:47` |
| Window length | `264.3 h` |
| Export rows | `80232` |
| Firmware observed in log | `0.1.5` |
| Largest logging gap | `175340 s` (`48.7 h`) |

## Known Data Limitation

This export contains several FHEM-side logging interruptions. The thermals are
still interpretable, but runtime percentages should be read as observed sampled
behavior, not as perfectly gap-free wall-clock runtime.

## Key Results

This run was the first one that looked broadly right in practice:

- water stayed in the desired aquarium band
- the stronger stage reduced the need for near-permanent running
- observed fan faults disappeared

### Water and Air

| Signal | Min | Max | Mean |
|---|---:|---:|---:|
| Water temperature | `23.5 C` | `24.8 C` | `24.11 C` |
| Air temperature | `22.0 C` | `35.6 C` | `26.87 C` |

Phase split:

| Phase | Water mean | Air mean | Fan PWM sample mean |
|---|---:|---:|---:|
| Light on (`15:00-22:00`) | `24.20 C` | `31.48 C` | `18.05%` |
| Light off | `24.07 C` | `24.48 C` | `17.86%` |

## Fan Runtime Observation

Observed PWM values remained effectively binary in this phase:

- `0%`
- `22%`

Estimated runtime from sampled PWM intervals:

| Signal | Value |
|---|---:|
| Observed fan runtime | `85.4%` |
| Starts | `5` |
| Stops | `4` |

Compared with the previous `18%` phase, this was the more balanced operating
point.

## Fault Observation

The low-PWM plausibility issue was effectively gone in this observed run:

| Signal | Value |
|---|---:|
| Observed fan-fault time | `0.0 min` |
| Observed fan-fault share | `0.00%` |

## Interpretation

This export supported three important conclusions:

1. `22% PWM` is clearly the better fixed low stage than `18%`.
2. The water-only hysteresis concept was confirmed again.
3. A next tuning step should focus on staged control flexibility rather than
   bringing the old air-assist path back.

Those conclusions directly motivated the later `0.1.6` architecture update with
remote-configurable staged control parameters.

