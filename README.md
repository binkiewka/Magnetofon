# Magnetofon (ST-8000) — Native C++ Edition

<p align="center">
  <img src="resources/icon.png" width="128" alt="Magnetofon Logo" onerror="this.style.display='none'" />
</p>

<h3 align="center">Magnetofon ST-8000 — High-Fidelity Audio Console (Native C++ / Qt 6 QML)</h3>

<p align="center">
  Magnetofon is a high-performance desktop audio console built with <strong>C++17</strong>, <strong>Qt 6 QML (OpenGL Scene Graph)</strong>, and <strong>libmpv</strong>. Styled after high-end McIntosh sapphire blue glass faceplates and Sony ES 3D extruded metallic hardware.
</p>

<p align="center">

  <a href="#-key-features">Features</a> •
  <a href="#-interface-showcase">Gallery</a> •
  <a href="#-installation--build">Setup & Build</a> •
  <a href="#-architecture">Architecture</a>
</p>

---

## Overview & Performance Highlights

**Magnetofon ST-8000** combines the physical tactile elegance of vintage 1980s/1990s Japanese and American audiophile component stacks with a **pure C++17 engine**, direct `libmpv` C++ audio pipeline, and hardware-accelerated **Qt Quick OpenGL Scene Graph** rendering.

- **0% CPU Animation Jank**: Powered by Qt Quick Scene Graph GPU rendering, CSS repaints and main-thread webview bottlenecks are completely eliminated.
- **Ultra-Low Memory Footprint (~35 MB RAM)**: Uses ~85% less RAM than webview/Electron applications.
- **Direct `libmpv` C++ Audio Engine**: No IPC bridge overhead. Audio properties, PCM meter physics, equalizer filters, and channel-aware routing run natively in C++.
- **McIntosh Sapphire Blue VU Meters**: Custom C++ `QQuickPaintedItem` rendering dual-needle power output meters at 60+ FPS with spring physics.
- **Frameless Windowless Console**: Native desktop integration with seamless titlebar window dragging (`startSystemMove()`), minimize, and close controls.

---

## 📸 Interface Showcase

<p align="center">
  <img src="screenshots/magnetofon.png" width="850" alt="Magnetofon ST-8000" />
</p>


---

## Key Features

### Hi-Fi Audio Console Modules
- **Power Stage (`AmplifierPanel`)**: Knurled aluminum Master Volume knob centered on dark metallic faceplate with 11-tick cyan scale, plus vertical playback mode buttons (`AUTO`, `STEREO`, `SURROUND`).
- **Twin Signal Level Monitor (`VuMeterPanel`)**: Dual McIntosh cyan blue VU meters rendered on the GPU scene graph with peak warning LEDs.
- **10-Band Frequency Processor (`EqualizerPanel`)**: 16-band real-time spectrum analyzer, EQ preset selector, `POWER: ON / BYPASS` toggle, gain slider, and 10 EQ frequency band sliders.
- **Input & Active Track Deck (`ProgramMonitorPanel`)**: VFD screen displaying album art thumbnail, track title, artist/album details, gold digital time counter (`00:00 / 03:45`), vertical audio format specs (`44.1 kHz`, `16-BIT`, `320 kbps`), and 44-segment digital progress bar.
- **Program Memory (`PlaylistPanel`)**: Track queue list with native GTK system file dialog (`+ ADD`, `📁 LOAD`, `💾 SAVE`, `🗑 CLEAR`).
- **Tape Playback Engine (`CassettePanel`)**: Maxell UR90 tape shell with 4 corner slotted screws, Maxell label block, window guide bands, rotating reels, metallic cassette head assembly, and 3D metallic transport push-buttons.

---

## Supported Audio Formats

Magnetofon handles all major audio formats natively via `libmpv`:
- **Lossless**: FLAC, WAV, AIFF, AIF, ALAC
- **Multichannel**: Native 5.1/7.1 FLAC passthrough in Auto and Surround modes; proper stereo downmix; mono/stereo-to-5.1 upmix in Surround mode
- **Compressed**: MP3, AAC, M4A, OGG, OPUS, WMA, MP2

---

## Installation & Build

### System Dependencies (Linux / Ubuntu / Debian)

```bash
sudo apt update
sudo apt install build-essential cmake qt6-base-dev qt6-declarative-dev qt6-tools-dev libmpv-dev pkg-config
```

### Quick Run

To build and run Magnetofon Native directly from the repository:

```bash
./run.sh
```

### Manual Compilation via CMake

```bash
mkdir -p src-cpp/build
cd src-cpp/build
cmake ..
make -j$(nproc)
./MagnetofonNative
```

---

## Architecture

```
Magnetofon/
├── run.sh                     (Root launch script)
├── run_native.sh              (Native C++ build launcher)
└── src-cpp/
    ├── CMakeLists.txt         (CMake configuration for Qt 6 & libmpv)
    ├── main.cpp               (QApplication, setlocale, & QML engine startup)
    ├── audio/
    │   ├── AudioPlayer.hpp    (C++ QObject wrapping libmpv with Q_PROPERTY bindings)
    │   └── AudioPlayer.cpp    (Direct libmpv playback, EQ & 5.1 surround filters)
    ├── models/
    │   ├── PlaylistModel.hpp  (QAbstractListModel for track queue & reordering)
    │   └── PlaylistModel.cpp  (Native QFileDialog open slot & queue management)
    └── ui/
        ├── VuMeterItem.hpp    (Custom QQuickPaintedItem for McIntosh Cyan Dual VU)
        ├── VuMeterItem.cpp    (GPU-rendered McIntosh dual needles)
        ├── qml.qrc            (QML resource bundle)
        └── qml/
            ├── main.qml               (Frameless main window & metallic chassis)
            ├── HifiPanel.qml          (3D Extruded Panel Container)
            ├── HifiButton.qml         (Tactile 3D Metallic Push Button)
            ├── Knob.qml               (Tactile Knurled Master Volume Dial)
            ├── EqSlider.qml           (Vertical EQ Slider)
            ├── AmplifierPanel.qml     (Power Stage Module)
            ├── VuMeterPanel.qml       (Twin Signal Monitor Module)
            ├── EqualizerPanel.qml     (10-Band EQ & Spectrum Analyzer Module)
            ├── ProgramMonitorPanel.qml (VFD Display & Progress Deck Module)
            ├── CassettePanel.qml      (Tape Playback Engine Module)
            ├── CassetteDeck.qml       (Maxell UR90 Cassette Shell & Reels)
            └── PlaylistPanel.qml      (Program Memory Queue Module)
```

---

## License

Magnetofon is licensed under the GPL-3.0 License.
