# Controller Diagrams

These Mermaid diagrams document the current controller firmware behavior in
`firmware/controller`.

The Mermaid source files below are the current source of truth. The rendered
SVG/PNG artifacts may lag behind until they are regenerated.

The diagrams are also available as standalone `.mmd` files for draw.io /
diagrams.net imports:

- [controller-state-machine.mmd](controller-state-machine.mmd)
- [controller-system-architecture.mmd](controller-system-architecture.mmd)
- [controller-cycle-sequence.mmd](controller-cycle-sequence.mmd)

Rendering notes are available in [mermaid-rendering.md](mermaid-rendering.md).

Rendered artifacts:

- [controller-state-machine.svg](rendered/controller-state-machine.svg)
- [controller-state-machine.png](rendered/controller-state-machine.png)
- [controller-system-architecture.svg](rendered/controller-system-architecture.svg)
- [controller-system-architecture.png](rendered/controller-system-architecture.png)
- [controller-cycle-sequence.svg](rendered/controller-cycle-sequence.svg)
- [controller-cycle-sequence.png](rendered/controller-cycle-sequence.png)

## Control States

```mermaid
stateDiagram-v2
    direction LR

    state "Setup" as Setup
    state "Fan off" as FanOff
    state "Fan low" as FanLow
    state "Fan high" as FanHigh
    state "Water sensor fallback" as WaterFallback

    [*] --> Setup
    Setup --> FanOff: setup complete and water sample valid
    Setup --> WaterFallback: water sample invalid

    FanOff --> FanLow: water delta >= cooling_on_delta_c
    FanOff --> FanHigh: water delta >= high_cooling_delta_c
    FanLow --> FanOff: water delta <= cooling_off_delta_c
    FanLow --> FanHigh: water delta >= high_cooling_delta_c
    FanHigh --> FanLow: water delta < high_cooling_delta_c

    FanOff --> WaterFallback: water sample invalid
    FanLow --> WaterFallback: water sample invalid
    FanHigh --> WaterFallback: water sample invalid
    WaterFallback --> FanOff: water sample valid and water delta < cooling_on_delta_c
    WaterFallback --> FanLow: water sample valid and water delta >= cooling_on_delta_c and < high_cooling_delta_c
    WaterFallback --> FanHigh: water sample valid and water delta >= high_cooling_delta_c

    note right of FanOff
      ControlEngine mode: fan-off
      final_pwm = 0%
      target defaults to 23.0 C when invalid
    end note

    note right of FanLow
      ControlEngine mode: fan-low
      final_pwm = fan_low_pwm_percent
      default low stage = 45%
    end note

    note right of FanHigh
      ControlEngine mode: fan-high
      final_pwm = fan_high_pwm_percent
      default high stage = 60%
    end note

    note right of WaterFallback
      ControlEngine mode: water-sensor-fallback
      final_pwm = fallback PWM 40%
      fault response: water-fallback
    end note

    state "Fault policy overlay" as FaultOverlay {
        direction LR

        state "No alarm" as NoAlarm
        state "Water sensor fault" as WaterSensorFault
        state "Fan fault" as FanFault
        state "Water sensor + fan fault" as WaterAndFanFault

        [*] --> NoAlarm
        NoAlarm --> WaterSensorFault: water sample invalid
        NoAlarm --> FanFault: RPM mismatch x3

        WaterSensorFault --> NoAlarm: water sample valid
        FanFault --> NoAlarm: RPM plausible x3

        WaterSensorFault --> WaterAndFanFault: fan fault latched
        FanFault --> WaterAndFanFault: water sample invalid

        WaterAndFanFault --> FanFault: water sample valid
    }
```

## System Architecture

```mermaid
flowchart LR
  subgraph AQUA["Aquarium hardware"]
    WATER["DS18B20 water sensor<br/>ROM 28333844050000CB"]
    FAN["Noctua NF-S12A PWM fan<br/>12 V power"]
  end

  subgraph IO["ESP32 pins and electrical interfaces"]
    ONEWIRE["Shared 1-Wire bus<br/>GPIO33, 3.3 kOhm pull-up"]
    PWM["Fan PWM output<br/>GPIO25, 25 kHz"]
    TACH["Fan tach input<br/>GPIO26, 3.3 kOhm pull-up"]
  end

  subgraph LOCAL["ESP32 local autonomous control"]
    SENSOR["SensorManager<br/>2 s sample interval<br/>non-blocking DS18B20 conversion"]
    CONTROL["ControlEngine<br/>target validation<br/>water-only staged control"]
    DRIVER["FanDriver<br/>PWM command<br/>40% start boost for 2 s"]
    RPM["RpmMonitor<br/>1 s tach sample window"]
    CURVE["FanCurve<br/>expected RPM interpolation<br/>plausibility tolerance"]
    FAULTMON["FaultMonitor<br/>5 s settling<br/>3 mismatches to latch<br/>3 matches to recover"]
    POLICY["FaultPolicy<br/>alarm code<br/>severity<br/>local response"]
    PREFS["Preferences / NVS<br/>persisted target + staged control config"]
    SERIAL["Serial service console<br/>status, target, default,<br/>control, faults, network, publish"]
  end

  subgraph NET["Optional network telemetry"]
    WIFI["Wi-Fi station<br/>10 s reconnect interval"]
    MQTT["MqttTelemetry<br/>60 s full snapshot +<br/>immediate event publish<br/>availability last will"]
    BROKER["MQTT broker / FHEM<br/>state, diagnostics, status topics"]
  end

  WATER --- ONEWIRE
  ONEWIRE --> SENSOR

  SENSOR -->|ControlInputs| CONTROL
  PREFS -->|stored target + staged config| CONTROL
  SERIAL -->|target/default commands| PREFS
  SERIAL -->|diagnostic commands| CONTROL

  CONTROL -->|final PWM percent| DRIVER
  DRIVER --> PWM
  PWM --> FAN
  FAN --> TACH
  TACH --> RPM

  DRIVER -->|applied PWM| FAULTMON
  RPM -->|measured RPM| FAULTMON
  CURVE -->|expected RPM + tolerance| FAULTMON
  CONTROL -->|sensor validity + mode| POLICY
  FAULTMON -->|fan plausibility + latch| POLICY

  CONTROL --> MQTT
  FAULTMON --> MQTT
  POLICY --> MQTT
  PREFS --> MQTT
  MQTT --> WIFI
  WIFI --> BROKER

  POLICY -->|alarm, degraded cooling,<br/>service required| SERIAL
  SENSOR -->|ROMs, samples,<br/>presence pulse| SERIAL
  FAULTMON -->|RPM diagnostics| SERIAL
```

## Control Cycle Timing

```mermaid
sequenceDiagram
    autonumber
    participant Main as Controller loop
    participant Serial as Serial console
    participant Sensors as SensorManager
    participant Control as ControlEngine
    participant Fan as FanDriver
    participant RPM as RpmMonitor
    participant Fault as FaultMonitor
    participant Policy as FaultPolicy
    participant MQTT as MqttTelemetry
    participant Broker as MQTT broker

    Note over Main,MQTT: Local sensing, control, fan drive, RPM checks, and fault policy keep running without network connectivity.

    loop each Arduino loop iteration
        Main->>Serial: processSerialInput()
        Serial-->>Main: optional command effects

        Main->>Sensors: update(nowMs)
        alt first boot or no known sensors
            Sensors->>Sensors: discover ROM IDs on shared 1-Wire bus
            Sensors->>Sensors: request temperature conversion
        else conversion pending and ready
            Sensors->>Sensors: read water temperature
        else sample interval elapsed, 2000 ms
            Sensors->>Sensors: request next conversion
        end

        Sensors-->>Main: SensorSnapshot
        Main->>Control: compute(ControlInputs)
        Control-->>Main: ControlSnapshot with fan-off/fan-low/fan-high and final PWM

        Main->>Fan: setCommandedPwmPercent(final PWM)
        Main->>Fan: update(nowMs)
        alt fan starts from 0% to nonzero PWM
            Fan-->>Main: apply 40% start boost for 2000 ms
        else steady command
            Fan-->>Main: apply commanded PWM
        end

        Main->>RPM: update(nowMs)
        RPM-->>Main: latest RPM from 1000 ms tach window

        Main->>MQTT: update(nowMs)
        alt Wi-Fi or MQTT disconnected
            MQTT->>MQTT: retry connection every 10000 ms
        else MQTT connected
            MQTT->>MQTT: mqttClient.loop()
        end

        alt diagnostics interval elapsed, 2000 ms
            Main->>Fault: evaluate(applied PWM, measured RPM, nowMs)
            Fault->>Fault: wait 5000 ms after PWM change before plausibility
            Fault-->>Main: FaultMonitorSnapshot

            Main->>Policy: evaluate(ControlSnapshot, FaultMonitorSnapshot)
            Policy-->>Main: alarm, severity, response, degraded cooling

            Main->>Serial: printDiagnostics()
            Main->>MQTT: publishTelemetry(force=false)
            alt MQTT connected and publish interval elapsed, 60000 ms
                MQTT->>Broker: publish state, diagnostics, and status topics
            else MQTT connected and event state changed
                Main->>MQTT: publishTelemetry(force=true)
                MQTT->>Broker: publish immediate controller/fault update
            else not connected or interval not elapsed
                MQTT-->>Main: skip telemetry publish
            end
        else diagnostics interval not elapsed
            Main-->>Main: return quickly to keep loop responsive
        end
    end
```
