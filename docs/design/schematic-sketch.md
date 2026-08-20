# Schematic Sketch

This is the block-level wiring sketch for the aquarium cooling controller.

The updated version shows the central 3D-printed enclosure, the preferred rear
upper mounting position near the lighting area, and the terminal-block based
wiring concept for the fan, the detachable metal circular connector for the
water sensor, and the optional local OLED status display.

## Use in draw.io

1. Open draw.io / diagrams.net.
2. Select `Arrange -> Insert -> Advanced -> Mermaid`.
3. Paste the contents of `docs/design/schematic-sketch.mmd`.
4. Adjust layout, colors, and labels as needed.

## Notes

- This is a wiring/block sketch, not a detailed PCB schematic.
- The DS18B20 water sensor uses the 1-Wire bus on GPIO33 with a 3.3 kOhm
  pull-up to 3.3 V.
- The 3-pin metal circular water-sensor connector uses the fixed project
  pinout `1 = 3.3 V/VCC`, `2 = GND`, and `3 = DATA/GPIO33`.
- The connector depiction remains provisional until the real plug and socket
  have been rechecked and the documentation includes a correct, clearly
  oriented photo of their numbered contacts.
- Connector numbering follows the contact numbers marked on each insert; plug
  and socket views can appear mirrored. The metal shell remains unconnected.
- Fan PWM is currently assigned to GPIO25.
- Tach input is currently assigned to GPIO26 and uses a 3.3 kOhm pull-up to
  3.3 V.
- The optional local OLED status display uses `I2C` on GPIO32 (`SDA`) and
  GPIO27 (`SCL`).
- The OLED is intentionally non-critical; missing or failed display hardware
  must not affect cooling, MQTT, OTA, or fault handling.
- The installed fan wiring uses `pin 1 = black/GND`, `pin 2 = red/+12 V`,
  `pin 3 = yellow/TACH with pull-up`, and `pin 4 = blue/PWM`.
- Common ground between ESP32, fan, and 12 V fan supply is required for
  correct PWM and TACH behavior.
- Fan power stays on 12 V, while the ESP32 is supplied through a buck
  converter.
- The enclosure is modeled as the central integration point for USB-C PD board,
  5 V PSU, ESP32, the fan wiring termination, the water-sensor circular
  connector, and the optional OLED wiring.
- The mounting position is shown as above the aquarium frame on the rear side,
  near the lighting area, to keep field wiring short.
