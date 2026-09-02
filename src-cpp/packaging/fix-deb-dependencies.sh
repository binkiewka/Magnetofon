#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 || ! -f "$1" ]]; then
    echo "Usage: $0 <package.deb>" >&2
    exit 64
fi

deb_file="$(realpath "$1")"
work_dir="$(mktemp -d)"
trap 'rm -rf -- "$work_dir"' EXIT

dpkg-deb --raw-extract "$deb_file" "$work_dir"
control_file="$work_dir/DEBIAN/control"

if grep -Eq 'libavcodec-extra60 \([^)]*\) \| libavcodec60 \([^)]*\)' "$control_file"; then
    exit 0
fi

if ! grep -Eq 'libavcodec60 \([^)]*\)' "$control_file"; then
    echo "Expected libavcodec60 dependency was not found in $deb_file" >&2
    exit 1
fi

# Ubuntu's libavcodec-extra60 provides libavcodec60 but conflicts with the
# standard package. Some graphical installers ignore that versioned provider
# and remove Mint's codec meta-package. Prefer the installed extra variant.
sed -E -i \
    's/libavcodec60 \(([^)]*)\)/libavcodec-extra60 (\1) | libavcodec60 (\1)/' \
    "$control_file"

if ! grep -Eq 'libavcodec-extra60 \([^)]*\) \| libavcodec60 \([^)]*\)' "$control_file"; then
    echo "Failed to add the alternative libavcodec dependency" >&2
    exit 1
fi

rebuilt_file="${deb_file}.rebuilt"
dpkg-deb --build --root-owner-group "$work_dir" "$rebuilt_file" >/dev/null
mv "$rebuilt_file" "$deb_file"
