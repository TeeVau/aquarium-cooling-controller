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
Override nur lauter. Die naechste Hardwaretest-Runde soll zeigen, ob ein
separater "fan fault boost" spaeter sinnvoll ist.

## Service-Kommando

Das Kommando `faults` gibt die aktuellen Policy-Defaults aus:

- Wassersensor-Fallback-PWM
- Luftsensor-Fehlerreaktion
- Fan-Fehlerreaktion
- Mismatch-Zyklen bis Fan-Fault-Latch
- Match-Zyklen bis Recovery
- Settling-Zeit vor Plausibilitaetsbewertung

## Noch zu testen

- Wassersensor abziehen und `water-sensor-fault` plus `40%` PWM bestaetigen
- Luftsensor abziehen und `air-sensor-fault` ohne Unterbrechung der
  Wasserregelung bestaetigen
- Tach-/Fanfehler simulieren und `fan-fault` nach drei Mismatches bestaetigen
- Recovery nach drei plausiblen Matches verifizieren
