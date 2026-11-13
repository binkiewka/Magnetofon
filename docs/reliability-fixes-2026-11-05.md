# Reliability fixes — 2026-11-05

These changes repair the reviewed working tree without discarding pre-existing
edits. They are not a dependency-major upgrade or a published release.

## Behavior changes

| Review item | Resolution |
|---|---|
| H-01 capture shutdown | libpulse asynchronous context/stream on one owned worker; nonblocking iterations and cancellation-aware waiting replace blocking simple-API reads. |
| H-02 import responsiveness | UI dialogs, drops and file-open IPC use bounded background imports, model-thread batch insertion, progress and Cancel. Consecutive file-open selections queue FIFO. |
| M-01 empty queue replay | Clear/remove-last unloads identity and duration; Play and delayed native events cannot restore a removed track. Ordinary Stop retains the selected track for restart. |
| M-02 false meters | Removed synthetic energy. Linux recording is restricted to this process's sink-input before connecting; stale/unavailable samples are not measurements. UI labels LIVE/IDLE/METER UNAVAILABLE. |
| M-03/04 playlist persistence | Portable relative entries are prefixed safely; whitespace/newline-sensitive names use encoded local URLs. Actual save/load failures are visible, and invalid loads retain the existing queue. |
| M-05/06 pack failures | Timeout/Cancel/Retry, checked writes/flushes, separate staging and atomic active-version selection. Working packs/custom files are retained. See [preset storage](preset-packs.md). |
| M-07 visualizer Stop race | Desired-running state and generation checks invalidate queued restarts. |
| M-08 unsupported Windows visuals | Platform-aware runtime lookup and disabled launch controls with an explanation. Windows native projectM is not supplied or certified by this repair. |
| M-09 output changes | Nonblocking bounded default-sink queries refresh the projectM monitor and safely restart its owned helper. Own-stream metering independently follows the sink-input's output. |
| L-01 metadata cache | Return an implicitly shared owned value while locked; worker reads cannot outlive an unlocked hash reference. Embedded-art cache writes commit atomically. |
| L-02/03 tests/build | Shared warnings/sanitizer options instrument native test targets; CTest has an explicit resource working directory; QML interactions are registered. |
| L-04/05 vendor rebuilds | Core/frontend/Poco revisions and existing patch hashes are pinned; obsolete Electron/npm instructions removed. No vendor rebuild was performed. |
| L-06 performance claims | Removed unsupported CPU/RSS/GPU-painting claims. QPainter rasterization and scene-graph compositing are described separately. |
| L-07 PCM test timing | Removed an unsupported mpv option. PCM tests assert sample integrity; transport tests use timed null output. |

**MY PRESETS** selects loose legacy/custom files even when a newly verified full
pack is active. Managed versions live in a sibling directory, not inside that
tree: projectM recursively includes hidden folders, so hidden staging would not
be sufficient isolation.

## Regression checks

The native suites cover transport/gapless/DSP, real QML load/render, asynchronous
import/cancellation, storage errors, local HTTP download/extraction failures,
visualizer process races, and own-stream capture selection. Tests use synthetic
media, temporary settings and fake helper/server boundaries. Test AudioPlayer
builds use null/PCM output; no personal speakers or sound-server capture is used.

```bash
cmake -S src-cpp -B test-build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build test-build --parallel 2
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  ctest --test-dir test-build --output-on-failure
```

CI creates FLAC and multi-audio-track compressed-surround video fixtures. For a
manual complete run, set `MAGNETOFON_SURROUND_TEST_FILE`,
`MAGNETOFON_VIDEO_TEST_FILE`, `MAGNETOFON_MULTITRACK_VIDEO_TEST_FILE`, and
`MAGNETOFON_VIDEO_TEST_DIR` to corresponding synthetic fixtures. Otherwise the
optional external-media cases skip. `unzip` is needed for pack tests and the
QtTest QML module is needed by qmltestrunner.

## Qualification boundaries

- No major Qt/mpv/FFmpeg/projectM update, public release, system package install
  or Git push is implied. Those need separate compatibility qualification.
- Real speaker mapping, physical output hotplug, hardware video rendering,
  Windows extraction/runtime behavior and live preset-server transfers have not
  been qualified here. Local synthetic tests are not substitutes for those.
- Model destruction does not join import workers and FFmpeg probing observes a
  five-second deadline/cancellation. An uninterruptible kernel/filesystem call
  can still occupy a worker and delay process teardown on a broken filesystem.
- ProjectM intentionally analyzes an output monitor mix. The VU/spectrum meter
  is the separately restricted, app-specific capture path.
- Completion manifests prove a staged extraction completed and matches its
  recorded archive/count, not independent authenticity of an upstream release.

Local verification logs, screenshots, pre-fix source archive and launch-path
rollback receipt are kept outside the repository in the operator's dated
`magnetofon-fixes-2026-11-05` artifact folder.

## Local qualification result

Both the normal RelWithDebInfo build and the Debug ASan/UBSan/LeakSanitizer build
passed all eight CTest suites: **64 scenario executions plus 16 lifecycle hooks**,
zero failures/skips. No sanitizer suppressions or disabled leak checks were used.
A Qt 6.4 software-renderer image teardown leak was independently reproduced with
only Window/Image; the short-lived QML test fixture now retires image sources
while visible before destroying its window. Production image behavior is unchanged.
The original failing sanitizer evidence is retained separately from final results.

The verified native executable was atomically delivered to the existing user
Desktop launcher target, `src-cpp/build/MagnetofonNative`, with the previous binary
preserved. Startup and second-instance IPC from that exact delivered path passed
in an isolated offscreen profile. No existing app process was running or restarted.
The separate system `/usr/bin/MagnetofonNative` binary was not modified; the user
Desktop entry overrides that system entry. Physical playback remains unqualified.
