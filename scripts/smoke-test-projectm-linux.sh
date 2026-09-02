#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RUNTIME="$ROOT/resources/projectm/linux-x64"
BINARY="$RUNTIME/bin/projectMSDL"
HELP_LOG="$(mktemp)"
RUNTIME_LOG="$(mktemp)"

cleanup() {
  rm -f "$HELP_LOG" "$RUNTIME_LOG"
}
trap cleanup EXIT

if [[ ! -x "$BINARY" ]]; then
  echo "Bundled projectMSDL runtime is missing: $BINARY" >&2
  exit 1
fi

export LD_LIBRARY_PATH="$RUNTIME/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

"$BINARY" --help > "$HELP_LOG" 2>&1
for option in borderless presetDuration transitionDuration beatSensitivity hardCutsEnabled; do
  if ! grep -q -- "--$option" "$HELP_LOG"; then
    echo "Bundled projectMSDL does not support --$option" >&2
    cat "$HELP_LOG"
    exit 1
  fi
done

if ! command -v xvfb-run >/dev/null 2>&1; then
  echo "projectMSDL CLI smoke passed; xvfb-run unavailable, skipping live rotation smoke."
  exit 0
fi

set +e
timeout 14s xvfb-run --auto-servernum env SDL_AUDIODRIVER=dummy \
  "$BINARY" \
  --presetPath="$ROOT/visuals/curated/presets" \
  --texturePath="$RUNTIME/textures" \
  --enableSplash=0 \
  --borderless=1 \
  --shuffleEnabled=0 \
  --fps=30 \
  --presetDuration=3 \
  --transitionDuration=0.5 \
  --beatSensitivity=1.0 \
  --hardCutsEnabled=0 \
  > "$RUNTIME_LOG" 2>&1
runtime_status=$?
set -e

if [[ "$runtime_status" -ne 124 ]]; then
  echo "Bundled projectMSDL exited unexpectedly (status $runtime_status)" >&2
  cat "$RUNTIME_LOG"
  exit 1
fi

preset_count="$(grep -c 'Displaying preset:' "$RUNTIME_LOG" || true)"
if (( preset_count < 3 )); then
  echo "Timed rotation did not display enough presets (saw $preset_count)" >&2
  cat "$RUNTIME_LOG"
  exit 1
fi

echo "projectMSDL live rotation smoke passed with $preset_count displayed presets."
