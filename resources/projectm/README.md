# Bundled projectM runtime

Magnetofon is a native C++/Qt application. There is no Electron, npm or Butterchurn fallback.

## Linux x86-64

From the repository root:

```bash
bash scripts/build-projectm-linux.sh
bash scripts/smoke-test-projectm-linux.sh
```

Install the dependencies printed by the build script first. The script fills:

```text
resources/projectm/linux-x64/bin/projectMSDL
resources/projectm/linux-x64/lib/
resources/projectm/linux-x64/presets/
resources/projectm/linux-x64/textures/
```

Linux DEB/AppImage packaging places the helper and libraries beneath `libexec/magnetofon/projectm` and the curated presets beneath `share/magnetofon/visuals/curated`. The launcher also recognizes the source-tree development layout.

## Source lock and local patches

The existing working bundle is based on these exact revisions:

- projectM core: `359bf7801e20b87712d6a89c8862f770c84ecdeb` (development metadata 4.2.0)
- SDL frontend: `49863dd3343f8fe926c0d50db2436aa52db2aec6`
- Poco: `96d182a99303fb068575294b36f0cc20da2e7b25` (1.14.2 release)

These are not a claim of upstream stable-release qualification. The build script fetches those revisions for new checkouts and rejects a different HEAD in an existing checkout without resetting developer changes. `patches/SHA256SUMS` pins the local patch bytes. A deliberate rebase must update the source pins, patch checksums and regression evidence together. Submodules remain pinned by their parent revisions.

## Other platforms

The current Windows package does **not** contain a supported native visualizer runtime or curated assets. The app exposes this unavailable state and its launch controls should be disabled. Do not ship the Linux helper as a Windows substitute. Other platforms need their own tested runtime, assets and clean-package smoke check before claiming support.

## Runtime audio source

On Linux the helper uses the default output monitor. Magnetofon rechecks it asynchronously while visuals are active and safely restarts the helper after an output change. This measures the output mix, which may include other applications. A failed source query retains the previous working source and is retried; it does not block the GUI.
