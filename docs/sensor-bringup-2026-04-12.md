# Sensor Bring-Up - 2026-04-12

## Ergebnis

Erster DS18B20 erfolgreich am Controller erkannt.

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

## Firmware-Stand

- `sensor_manager` auf `OneWire` + `DallasTemperature` umgestellt
- Wassersensor per fester ROM-ID im Controller hinterlegt
- Aktuell noch Bench-/Diagnostikpfad mit manueller PWM-Steuerung

## Lokale Artefakte

Die zugehoerigen Serienmitschnitte liegen lokal im ignorierten `build/`-Ordner:

- `build/sensor-bringup-serial-after-rewire.txt`
- `build/water-sensor-fixed-rom.txt`
