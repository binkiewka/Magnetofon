# v2.2.0 integration and qualification

## Transport identity

`loadfile` is accepted as a synchronous playlist-control operation; opening/decoding remains asynchronous in libmpv. Immediately snapshotting native entry IDs is then ordered. Async load followed by a sync property query is not ordered by the mpv API. Every START_FILE ID is recorded (even a retired start); FILE_LOADED and END_FILE must belong to the current accepted native map. Stop/unload retire all IDs. This includes same-path reloads and delayed EOF/error events.

A corrupt gapless entry can fail before FILE_LOADED. Failure therefore follows its native entry ID to the model identity, stops the remaining native queue and offers Retry/Skip for the correct file. Only genuine accepted EOF advances the album.

## Sound contract

Auto/no EQ uses no enhancement/limiter graph. Explicit Stereo requests downmix.2.1 and Surround apply their intended bass routing; native7.1 remains passthrough rather than being run through a six-channel crossover. Enhanced processing defaults to a linked look-ahead limiter without make-up gain, not album normalization. Tests cover quiet unity, inter-channel ratios, correlated bass plus sub gain and boosted signal. A listener can explicitly bypass protection. RMS meters are not speaker-watt or clip measurements.

## Session storage

Full queue snapshots use QSaveFile atomically, with a queue UUID. Structural changes are debounced and large imports finish before full checkpoints. The10second position/selection checkpoint is a separate small file; restore accepts it only with a matching queue UUID and an existing queued selected file. A new full save invalidates older position checkpoints even if their refresh fails. Restores stay paused; volume deliberately starts50%. Missing/unreadable media is reported. These are local profile files, not uploaded library data.

## Visual integration

The Linux helper compiles the same PulseAudioAnalyzer source with an explicit parent PID and44.1kHz capture. Recording is restricted to that sink-input before connecting, follows its actual sink and never falls back to microphone/system mix. The bounded finite-sample PCM mailbox carries sequence/freshness data. Structured live/silent/unavailable heartbeats are independent of rendering state. `scripts/verify-projectm-runtime.py` prevents a stale helper/source/patch bundle from shipping; the build script records a new receipt only after rebuilding.

AppImage helper launches explicitly include the main bundle library directory as well as the libexec runtime: a parent executable RUNPATH is not inherited by child processes. Downloaded-artifact testing verified actual loaded SDL/Pulse paths and live capture, not only that the main window opens.

The pinned vendor sources plus committed idempotent patches reproduce the modified frontend. Original third-party source/licensing information and patches are shipped with the repository.

## Local qualification

- Current normal and ASan/UBSan/LeakSanitizer builds:8suites,91scenario executions+18lifecycle hooks each,0failures/skips, no suppressions.
- Independent deterministic old-EOF and corrupt-middle gapless reproductions failed before correction and pass afterwards; both are retained as application regressions.
- Real production app under private Xvfb/Pulse/null6channel output: active unmuted stream, live meters, keyboard pause/resume, orderly close and exact selected position restored paused. Actual UI captures cover1060×800,960×660,640×480.
- Actual rebuilt projectM helper/private null outputs: target tone, digital silence while another application plays, target migration to the same output as unrelated sound, resumed tone and source removal. SDL F/F11/Escape events and timed preset rotation also pass.
- Local DEB metadata/version and tested-binary identity verified; package-manager simulation upgrades Magnetofon with0removals.

Private build/probe receipts are retained by the operator; they are not embedded in the source repository. CI separately builds/tests Linux, exercises the bundled renderer and extracted AppImage, verifies Windows executable version and extracted startup, then publishes one tag-owned release with SHA256SUMS.

Physical speaker mapping/HDMI/Bluetooth/USB, perceived sound/video quality, physical GPU frame pacing and Windows speaker behaviour are not certified by virtual-device or source tests. Native visuals/metering remain Linux-only. No fixed performance or bit-perfect guarantee is made.
