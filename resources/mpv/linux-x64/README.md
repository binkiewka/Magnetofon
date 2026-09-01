# Bundled mpv runtime

Linux packages include a pinned, extracted anylinux mpv runtime here at build time.

Run `npm run prepare:linux-audio` to download it, verify its SHA-256 checksum, and stage only the runtime files needed by Magnetofon. Generated runtime files are intentionally ignored by Git; CI repeats the same pinned preparation before packaging.

See [`../NOTICE.md`](../NOTICE.md) for source and licensing details.
