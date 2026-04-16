# Fault Policy - 2026-04-16

## Ziel

Die lokale Firmware soll Fehler eindeutig klassifizieren, ohne die autonome
Kuehlung von Netzwerkdiensten abhaengig zu machen. Die Policy trennt deshalb
zwischen:

- Fehlererkennung
- Alarmcode
- Schweregrad
- lokaler Reaktion
- Service-/Diagnosehinweis

## Implementierter Stand

Das Modul `fault_policy` bewertet den aktuellen `ControlSnapshot` zusammen mit
dem `FaultMonitorSnapshot`.

Ausgegeben werden:

- `Alarm code`
- `Fault severity`
- `Fault response`
- `Cooling degraded`
- `Service required`
- `Water sensor ok`
- `Air sensor ok`
- `Fan ok`

## Aktuelle Fehlerreaktionen

| Fehler | Alarmcode | Severity | Lokale Reaktion |
|---|---|---|---|
| Kein Fehler | `none` | `none` | `normal-control` |
| Wassersensor ungueltig | `water-sensor-fault` | `critical` | `water-fallback` mit `40%` PWM |
| Luftsensor ungueltig | `air-sensor-fault` | `warning` | `disable-air-assist`, Wasserregelung laeuft weiter |
| Fan-Plausibilitaetsfehler | `fan-fault` | `critical` | `report-fan-fault`, PWM-Befehl bleibt lokal berechnet |
| Wassersensor + Fanfehler | `water-sensor+fan-fault` | `critical` | `water-fallback+report-fan-fault` |
| Luftsensor + Fanfehler | `air-sensor+fan-fault` | `critical` | `report-fan-fault` |
| Mehrfachfehler | `multiple-faults` | `critical` | abhaengig von enthaltenem Wassersensor-/Fanfehler |

## Bewusste Entscheidung fuer Fanfehler

Ein bestaetigter Fan-Plausibilitaetsfehler wird aktuell nicht mit einem
zusaetzlichen PWM-Override beantwortet. Der Controller meldet den Fehler als
kritisch und setzt `serviceRequired`, laesst den lokal berechneten PWM-Befehl
aber unveraendert.

Grund: Bei einem Tach-/Fanfehler ist nicht sicher, ob ein pauschaler Override
auf `100%` die reale Kuehlung verbessert. Ein blockierter oder elektrisch
defekter Fan wird dadurch nicht repariert; bei reinem Tachproblem waere ein
Override nur lauter. Die Hardwaretests mit fehlendem Tachsignal und bewusst
abgebremstem Fan haben bestaetigt, dass die aktuelle Reaktion den Fehler
zuverlaessig meldet und Recovery sauber entprellt. Ein separater
"fan fault boost" wird deshalb aktuell nicht eingefuehrt.

## Service-Kommando

Das Kommando `faults` gibt die aktuellen Policy-Defaults aus:

- Wassersensor-Fallback-PWM
- Luftsensor-Fehlerreaktion
- Fan-Fehlerreaktion
- Mismatch-Zyklen bis Fan-Fault-Latch
- Match-Zyklen bis Recovery
- Settling-Zeit vor Plausibilitaetsbewertung

## Verifizierte Hardwaretests

Alle folgenden Tests wurden am realen ESP32-Aufbau mit zwei DS18B20-Sensoren
und dem Noctua NF-S12A PWM durchgefuehrt.

| Test | Erwartung | Ergebnis |
|---|---|---|
| Normalbetrieb | `Alarm code: none`, alle Komponenten ok | Bestanden |
| Luftsensor getrennt | `air-sensor-fault`, `warning`, `disable-air-assist`, Wasserregelung laeuft weiter | Bestanden |
| Luftsensor wieder verbunden | Rueckkehr auf `none` | Bestanden |
| Wassersensor getrennt | `water-sensor-fault`, `critical`, `water-fallback`, `Final target PWM: 40%` | Bestanden |
| Wassersensor wieder verbunden | Rueckkehr auf `none` | Bestanden |
| Tachsignal getrennt | `fan-fault`, `critical`, `report-fan-fault`, `Fan ok: no` | Bestanden |
| Tachsignal wieder verbunden | Rueckkehr auf `none` nach plausiblen Matches | Bestanden |
| Fan mechanisch abgebremst | RPM ausserhalb Toleranz erzeugt `fan-fault` nach Mismatch-Debounce | Bestanden |
| Fan wieder frei laufend | Rueckkehr auf `none` nach Recovery-Debounce | Bestanden |

Der RPM-Abweichungstest zeigte beispielhaft:

- `Measured RPM: 150`
- `Expected RPM: 661`
- `Tolerance RPM: +/-79`
- `RPM error: -511`
- `Plausible: no`
- anschliessend `Fault latched: yes` und `Alarm code: fan-fault`

Die beobachteten Zustandswechsel im RPM-Abweichungstest waren:

```text
none -> fan-fault -> none -> fan-fault -> none
```

## Offene Punkte

- Kombinierte Mehrfachfehler wie Wassersensor plus Fanfehler sind im
  Policy-Modell enthalten, aber nicht als eigener Hardwaretest blockierend.
- Die Fan-Plausibilitaet sollte im final installierten Luftstrom erneut
  beobachtet werden, falls die reale Montage die Drehzahlkurve merklich
  veraendert.
