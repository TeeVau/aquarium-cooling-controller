# Mermaid Rendering

The controller diagrams are plain Mermaid source files. GitHub renders the
embedded diagrams in `controller-diagrams.md` automatically, and Mermaid CLI can
render the standalone `.mmd` files to SVG or PNG artifacts.

## Install Mermaid CLI

Mermaid CLI provides the `mmdc` command. It requires Node.js and npm.

### Option 1: System install

Install Node.js LTS from the official Node.js download page, then open a new
PowerShell window and run:

```powershell
npm install -g @mermaid-js/mermaid-cli
mmdc --version
```

### Option 2: One-off use without global install

After Node.js is installed, render one diagram without installing `mmdc`
globally:

```powershell
npx -p @mermaid-js/mermaid-cli mmdc -i docs\design\controller-state-machine.mmd -o docs\design\rendered\controller-state-machine.svg
```

### Option 3: Docker

If Docker is available, use the official Mermaid CLI container image:

```powershell
docker run --rm -v "${PWD}:/data" ghcr.io/mermaid-js/mermaid-cli/mermaid-cli -i /data/docs/design/controller-state-machine.mmd -o /data/docs/design/rendered/controller-state-machine.svg
```

## Render Project Diagrams

Once `mmdc` is available in `PATH`, render all controller diagrams:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\render-mermaid.ps1
```

This creates SVG and PNG files in:

```text
docs\design\rendered\
```

Render only SVG:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\render-mermaid.ps1 -Format svg
```

## Render Online

If local Node.js or `mmdc` is not available, the project also includes an online
renderer helper that uses `mermaid.ink`:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\render-mermaid-online.ps1
```

This also creates SVG and PNG files in:

```text
docs\design\rendered\
```

## Validate by Rendering

Mermaid CLI exits with a nonzero status when a diagram cannot be parsed or
rendered. For these diagrams, a successful run of `tools/render-mermaid.ps1`
acts as the local render check.

The online helper similarly fails if a generated artifact is empty or the HTTP
request fails.

For PNG output, the helper downloads the online raster output and converts it to
a real PNG locally, so the saved file has the expected PNG signature.
