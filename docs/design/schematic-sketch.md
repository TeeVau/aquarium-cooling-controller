# Schematic Sketch

This is a first block-level schematic sketch for the aquarium cooling
controller.

The updated version shows the central 3D-printed enclosure, the preferred rear
upper mounting position near the lighting area, and the terminal-block based
wiring concept for fan, water sensor, and air sensor.

## Use in draw.io

1. Open draw.io / diagrams.net.
2. Select `Arrange -> Insert -> Advanced -> Mermaid`.
3. Paste the contents of `docs/design/schematic-sketch.mmd`.
4. Adjust layout, colors, and labels as needed.

## Notes

- This is a wiring/block sketch, not yet a production-ready electrical
  schematic.
- The fan PWM interface stage is intentionally marked as `TBD` because the
  exact electrical compatibility with the selected 4-pin fan still needs
  prototype validation.
- DS18B20 water and air sensors share one 1-Wire bus on GPIO4.
- Tach input on GPIO19 uses a pull-up to 3.3 V.
- Fan power stays on 12 V, while the ESP32 is supplied through a buck
  converter.
- The enclosure is modeled as the central integration point for USB-C PD board,
  5 V PSU, ESP32, and field terminal blocks.
- The mounting position is shown as above the aquarium frame on the rear side,
  near the lighting area, to keep field wiring short.
