# Sensor Bring-Up - 2026-04-12

## Ergebnis

Zwei DS18B20 erfolgreich am Controller erkannt und fest per ROM-ID zugeordnet.

## Testaufbau

- ESP32 GPIO: `33`
- Pull-up: `3.3 kOhm` nach `3.3 V`
- Versorgung: `3.3 V`

## Ermittelte Leitungsbelegung

- `rot` -> `3.3 V`
- `gelb` -> `GND`
- `gruen` -> `DATA`

## Erkannte Sensordaten

- ROM-ID: `28333844050000CB`
- Gemessene Temperatur beim Test: ca. `25.69 C` bis `25.75 C`

## Zweiter Bus-Test am 2026-04-14

- Zweiter DS18B20 erfolgreich am selben 1-Wire-Bus auf `GPIO33` erkannt
- Luftsensor-ROM-ID: `28244644050000DA`
- Busdiagnostik zeigte zwei gefundene Sensoren
- Wassersensor-ROM `28333844050000CB` blieb stabil zugeordnet
- Gemessene Temperaturen beim Zwei-Sensor-Test: ca. `24.44 C` fuer Wasser und `24.31 C` bis `24.44 C` fuer Luft

## Firmware-Stand

- `sensor_manager` auf `OneWire` + `DallasTemperature` umgestellt
- Wassersensor per fester ROM-ID im Controller hinterlegt
- Luftsensor per fester ROM-ID im Controller hinterlegt
- Beide Sensoren laufen am gemeinsamen 1-Wire-Bus auf `GPIO33`

## Folge-Stand vom 2026-04-14

- Lokale Wasserregelung erfolgreich auf echter Hardware verifiziert
- Luft-Assist erfolgreich auf echter Hardware verifiziert
- Default-Wassertemperatur ist `23.0 C`
- `23.0 C` bleibt auch der Fehler- und Rueckfallwert bei ungueltigem Zielwert
- Benutzerdefinierte Zieltemperatur wird in ESP32 `Preferences` / NVS gespeichert
- Nach Reboot wird ein gueltiger gespeicherter Zielwert wieder geladen
- Nach ungueltigem Zielwert wird der gespeicherte Wert geloescht und beim naechsten Boot wieder `23.0 C` verwendet

## Aktuelle Service-Kommandos

- `status`
- `target <c>`
- `default`
- `airassist`
- `help`

## Lokale Artefakte

Die zugehoerigen Serienmitschnitte liegen lokal im ignorierten `build/`-Ordner:

- `build/sensor-bringup-serial-after-rewire.txt`
- `build/water-sensor-fixed-rom.txt`
- `build/two-sensor-scan-v3.txt`
- `build/air-sensor-assigned.txt`
- `build/persistence-and-faults.txt`
- `build/persistence-after-reboot.txt`
- `build/fallback-after-invalid-target.txt`
