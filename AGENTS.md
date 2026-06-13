# Arduino Build Workflow

For Arduino builds in this repository, do not call `arduino-cli compile`
directly unless you are explicitly troubleshooting the raw CLI.

Use the repository build script from the repo root instead:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\build.ps1
```

The script is the canonical Codex build entrypoint. It resolves the Arduino CLI
path, emits heartbeat lines during longer compiles, stores full stdout/stderr
logs under the board-specific `.arduino-build/.../logs/` directory until the
build succeeds, keeps raw compiler artifacts under `.arduino-build/.../output/`,
removes sketch-local ESP32 duplicate exports after successful builds, and
creates a versioned firmware BIN for release and OTA workflows.

When no USB-connected ESP is available and firmware should be flashed over the
network, prefer the repository OTA script:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\ota-upload.ps1
```

That OTA script is the canonical Codex entrypoint for broker-driven OTA enable,
published upload URL discovery, BIN upload, and MQTT-based post-upload
verification.

For this repository, do not use manual OTA upload commands or pass `-UploadUrl`
for normal flashing. Let `tools/ota-upload.ps1` resolve the active OTA window
and upload URL from MQTT automatically. Only use `-UploadUrl` when explicitly
troubleshooting the raw OTA HTTP endpoint.

For MQTT interaction in this repository, use the repository helper script:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\mqtt-client.ps1
```

Do not call `mosquitto_pub.exe` or `mosquitto_sub.exe` directly unless you are
explicitly troubleshooting the raw MQTT client behavior. For normal work,
credential handling and MQTT reads or writes should go through
`tools/mqtt-client.ps1`, and OTA flashing should go through
`tools/ota-upload.ps1`.
