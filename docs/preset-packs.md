# Preset pack storage and recovery

The native player keeps loose legacy/custom presets in its existing application-data
`visuals/` directory. New managed downloads are kept in the **sibling**
`visuals-managed-packs/` directory. This separation is intentional: projectM's own
recursive scanner includes hidden directories, so hiding staging inside `visuals/`
would still expose partially extracted/inactive packs to MY PRESETS.

- `visuals-managed-packs/.pack-install-XXXXXX/`: one operation's temporary archive
  and extracted content; never returned to the visualizer.
- `visuals-managed-packs/pack-<uuid>/`: a completely extracted version. Its
  `completed.json` records format version, archive SHA-256/byte count, and preset
  count after a successful extractor exit (unzip also checks entry CRCs).
- `visuals-managed-packs/active-pack.json`: atomically replaced with `QSaveFile`
  only after extraction, minimum-count verification, completion-manifest commit,
  and the same-filesystem rename succeed.

`PresetPackDownloader::installedPackDirectory(visualsRoot)` accepts the **legacy
visuals root**, resolves the sibling managed store, and returns only the activated
version whose completion metadata and current preset count agree. No file-count
heuristic certifies a legacy install as complete. Legacy/custom files remain usable
through the visualizer's separate source selection; the downloader labels them
unverified, not deleted or corrupted. Completion metadata establishes installation
integrity, not upstream authenticity or an independently pinned release checksum.

The previous active version and loose files are never overwritten by download or
extraction. Failed or cancelled operations remove only their own staging directory.
If the process is interrupted, an orphan staging or inactive version may remain in
the managed sibling, but will not become active or appear in legacy scans. Prior
managed versions are deliberately retained; no automatic destructive pruning runs.

Downloads have a 30-second inactivity timeout and a 15-minute total operation
deadline; extraction has a 120-second deadline. Cancel works while downloading or
extracting. Every download write/flush is checked before extraction. Retry is
available after failure/cancellation. The download limit is 512 MiB.

## Regression coverage

`tests/PresetPackDownloaderTest.cpp` uses an in-process loopback HTTP server,
synthetic stored ZIPs, temporary storage, and an injected storage-failure boundary.
It covers complete activation/reopen, cancellation/stall/retry, negative and short
writes plus flush failure, incomplete/corrupt replacements preserving the old pack,
interrupted staging and uncertified legacy counts, extraction timeout/cancel,
and invalidated completion metadata retaining custom files. No real preset download
or user data is needed. Linux test execution requires `unzip`; Windows runtime and
PowerShell extraction still require platform qualification.
