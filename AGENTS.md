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
