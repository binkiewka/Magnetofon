# Third-party components

Magnetofon is GPLv3; see LICENSE. Components retain their own notices/licenses.

| Component | Source and build information | License text |
|---|---|---|
| Qt6 | https://www.qt.io/licensing/ ; Linux distro packages, Windows6.7.3 | resources/licenses/Qt-LGPL-3.txt |
| libmpv / FFmpeg | resources/mpv/NOTICE.md ; Linux distro libraries / pinned Windows archive | resources/licenses/mpv-GPL-2.txt ; corresponding upstream component licenses |
| projectMSDL | https://github.com/projectM-visualizer/frontend-sdl-cpp ; pinned commit and patches in resources/projectm/README.md | resources/licenses/projectMSDL-GPL-3.md |
| projectM core | https://github.com/projectM-visualizer/projectm ; pinned runtime manifest | resources/licenses/projectM-LGPL-2.1.txt |
| Poco | https://github.com/pocoproject/poco ; pinned runtime manifest | resources/licenses/Poco-Boost.txt |

The modified projectMSDL frontend also compiles Magnetofon's PulseAudioAnalyzer source. Exact frontend/core/Poco commits, patches and source-build commands are documented in resources/projectm/README.md and scripts/build-projectm-linux.sh; the shipped helper/source hash receipt is resources/projectm/linux-x64/magnetofon-runtime.json.

Linux AppImage shared dependencies are deployed from Ubuntu24.04 distribution packages; their corresponding source packages and upstream license notices remain applicable. Curated visual presets retain their authors' identities in their source files.
