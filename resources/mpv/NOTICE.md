# Native mpv runtime

Magnetofon v2.2 uses the libmpv C API in-process, not the historical external portable mpv executable.

- Linux DEB links the distribution libmpv/FFmpeg libraries; the package manager provides them. The AppImage deploys the build environment libraries alongside the application. The release build environment is Ubuntu24.04; distribution source packages identify corresponding distro revisions.
- Windows x64 bundles `libmpv-2.dll` from shinchiro/mpv-winbuild-cmake release20261111: `mpv-dev-x86_64-20261111-git-bcde6b93cc.7z`.
- Windows source build/archive information: <https://github.com/shinchiro/mpv-winbuild-cmake/releases/tag/20261111>.
- Pinned Windows archive SHA256: `273c079b11ea5575822f972756f4c9a71149a5c10130353abeb8586738938452`.
- mpv source: <https://github.com/mpv-player/mpv>; build scripts: <https://github.com/shinchiro/mpv-winbuild-cmake>.

mpv and its codec dependencies retain their respective upstream licenses (the GPL build license is included in resources/licenses). No portable updater/download hook is run. The old resources/mpv/linux-x64 preparation script is legacy and is not the current C++ playback or packaging path.
