# Bundled projectM runtime

Magnetofon's native MilkDrop-compatible visualizer expects projectM to be bundled here:

```text
resources/projectm/<platform>-<arch>/bin/projectMSDL
resources/projectm/<platform>-<arch>/presets/*.milk
resources/projectm/<platform>-<arch>/textures/*
```

For this desktop, the development target is:

```text
resources/projectm/linux-x64/
```

Run `npm run build:projectm:linux` to build/fill that bundle. The Electron app packages this
folder via `build.extraResources`, so packaged apps resolve it from `process.resourcesPath/projectm`.

The old Butterchurn/Chromium visualizer remains as a dev fallback if the native bundle is missing.
