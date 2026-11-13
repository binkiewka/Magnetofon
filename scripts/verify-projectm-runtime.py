#!/usr/bin/env python3
"""Bind the shipped helper to the exact capture sources and pinned vendor patches."""
import argparse
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parent.parent
MANIFEST = ROOT / "resources/projectm/linux-x64/magnetofon-runtime.json"
HELPER = ROOT / "resources/projectm/linux-x64/bin/projectMSDL"
SOURCES = ["src-cpp/audio/PulseAudioAnalyzer.cpp", "src-cpp/audio/PulseAudioAnalyzer.hpp"] + [
    "resources/projectm/patches/" + name
    for name in ["frontend-sdl-cpp.patch", "magnetofon-audio.patch", "projectm-core.patch", "window-drag.patch"]
]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write", action="store_true", help="Record a newly rebuilt runtime (build script only)")
    args = parser.parse_args()
    build_script = (ROOT / "scripts/build-projectm-linux.sh").read_text()
    pins = {}
    for key, variable in [("frontendCommit", "FRONTEND_COMMIT"), ("coreCommit", "CORE_COMMIT"), ("pocoCommit", "POCO_COMMIT")]:
        match = re.search(r"^" + variable + r"=([a-f0-9]{40})$", build_script, re.MULTILINE)
        if not match:
            raise SystemExit("Missing exact source revision: " + variable)
        pins[key] = match.group(1)
    expected = {"format": 1, **pins,
                "features": ["process-isolated-pcm", "audio-readiness-heartbeat", "F-F11-Escape-fullscreen"],
                "sourceSha256": {source: digest(ROOT / source) for source in SOURCES},
                "helperSha256": digest(HELPER)}
    if args.write:
        MANIFEST.write_text(json.dumps(expected, indent=2) + "\n")
        print("Recorded rebuilt projectM runtime: " + expected["helperSha256"])
    elif json.loads(MANIFEST.read_text()) != expected:
        raise SystemExit("Bundled projectM does not match its pinned source/patch receipt. Rebuild before shipping.")
    else:
        print("Verified bundled projectM binary, capture sources, patches and vendor pins")


if __name__ == "__main__":
    main()
