# Aquarium Live Test - 2026-04-16

## Scope

First real-aquarium FHEM/DB export after moving the controller from bench
verification to the installed aquarium test setup.

Raw export:

- `docs/aquarium-live-tests/2026-04-16-aquarium-2h-fhem-export.csv`

## Capture Window

| Item | Value |
|---|---:|
| First sample | `2026-04-16 18:04:13` |
| Last sample | `2026-04-16 20:10:44` |
| Duration | `126.5 min` |
| Export rows | `1951` |

## Logged Readings

The export is a FHEM long-format CSV with these columns:

```text
TIMESTAMP;DEVICE;TYPE;EVENT;READING;VALUE;UNIT
```

Observed readings in this export:

- `air_temp_c`
- `water_temp_c`
- `fan_pwm_percent`
- `fan_rpm`
- `expected_rpm`
- `rpm_tolerance`
- `rpm_error`
- `plausibility_active`
- `fan_plausible`
- `fan_fault`
- `alarm_code`
- `fault_severity`
- `fault_response`
- `cooling_degraded`
- `service_required`

## Key Results

| Signal | First | Last | Notes |
|---|---:|---:|---|
| Water temperature | `26.56 C` | `25.50 C` | About `-0.53 C/h` over the capture |
| Air temperature | `29.12 C` | `27.38 C` | Warm initial under-lid condition cooled down |
| Fan PWM | `86%` | `52%` | Controller reduced demand as water approached target |
| Fan RPM | `1230` | `810` | RPM followed the falling PWM demand |

The water-temperature and PWM relationship indicates an active target of about
`24.0 C` during this run.

## Fault / Plausibility Observation

One short fan-plausibility fault was recorded:

| Timestamp | Event |
|---|---|
| `2026-04-16 20:05:54` | `fan-fault`, `critical`, `report-fan-fault` |
| `2026-04-16 20:06:04` | recovered to `none`, `normal-control` |

The fault happened with measured RPM slightly above the expected curve plus
tolerance:

| Signal | Value |
|---|---:|
| Fan PWM | about `52%` |
| Expected RPM | `765` |
| RPM tolerance | `+/-92` |
| Measured RPM | `870` |
| RPM error | `+105` |

Interpretation: this looks more like an installed-airflow / tolerance mismatch
than a real cooling failure. The controller recovered automatically after the
next plausible match.

## Current Decision

Because the controller is already installed and OTA is not implemented yet, the
firmware will not be changed for this observation phase. Continue collecting
FHEM/DB logs with the current firmware and evaluate longer runs before changing
control or fault-monitor parameters.

Recommended next capture:

- at least `24 h`
- ideally `48-72 h`
- include target, controller mode, availability, fault status, PWM, RPM,
  expected RPM, RPM tolerance, water temperature, and air temperature
