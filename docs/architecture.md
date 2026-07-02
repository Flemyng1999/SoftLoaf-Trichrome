# SoftLoaf Trichrome Architecture

This app follows a smaller version of the SoftLoaf Negative lifecycle model.

## Current Boundaries

- QML owns presentation only: controls bind to explicit `TrichromeController`
  properties and invoke named commands.
- `TrichromeController` is the QML adapter and task coordinator for now. It
  snapshots the active trichrome group before starting work.
- Decode/compose work runs on a background `QThread` with an 8 MiB stack because
  RAW decode can reach LibRaw.
- Preview compose is `latest_wins`: newer requests supersede older ones, and
  stale worker results are dropped by generation guard before UI mutation.
- RAW preview/development reads use LibRaw half-size processing. The preview
  cache stores the composed trichrome result, resampled so its long edge is at
  most 2048 px. A cache hit publishes that image without re-reading RAW files.
  Full demosaicked RAW resolution is reserved for explicit export tasks.
- Export is an explicit background task. Active-frame export and export-all
  both re-compose from the source files instead of saving the published preview.
- Clear/quit request worker interruption and drain known workers before
  replacing session state or leaving the app.
- The desktop app is intentionally session-only: users import a batch, preview,
  export, then discard the session. There is no project-file persistence or
  recent-project UI.
- Display handoff uses `TrichromeImageProvider` (`image://trichrome/...`) so QML
  does not infer state from temporary files.
- Display/preview is always encoded as sRGB. This is a project principle:
  display is the stable judging view, while larger working/export spaces are
  reserved for export.
- Decode and compose keep linear data explicit. RAW input is camera-native
  linear, regular image input is converted from encoded sRGB to linear sRGB,
  and the trichrome composite enters the named working state
  `linear_srgb_trichrome` before display or export encoding.
- Export encodes from that linear composite into named targets. The professional
  defaults are TIFF 16-bit and large linear spaces: ACES AP0, ACEScg,
  ProPhoto RGB, or Rec.2020. Legacy/display targets such as sRGB, Display P3,
  Rec.709, and Adobe RGB remain available for handoff needs.
- Export color constants are centralized in `color_management.hpp` and follow
  the `colour-science/colour` model: xy primaries, whitepoints, normalised
  primary matrices, Bradford chromatic adaptation, and target transfer curves.
  TIFF-family export attaches Little CMS generated ICC profiles for all named
  targets, including ACES AP0 and ACEScg/AP1.
- Preview cache identity is:

```text
source file identity + sensor mode + role order + RAW decode tier + max edge + cache schema
```

File identity includes path, size, mtime, and partial content hash. Cache writes
use temp file plus rename. A corrupt cache is a miss.

## Log Categories

- `app.startup`
- `command.received`
- `command.accepted`
- `task.started`
- `task.finished`
- `task.stale_drop`
- `cache.lookup`
- `cache.write`
- `trichrome.compose`
- `display.preview`
- `worker.started`
- `worker.finished`
- `worker.drain`

The format is the same family as SoftLoaf Negative:

```text
category=<name> key=value key=value
```

## Known Gaps

- Worker drain is cooperative. RAW decode and compose do not yet check
  interruption inside every long-running step, so shutdown can wait for the
  current decode to return.
- Export has an async full-source task, explicit target color-space encoding,
  TIFF 8-bit or 16-bit selection, and generated ICC profiles. The
  upstream camera-matrix / calibrated working-space policy is not yet equivalent
  to SoftLoaf Negative export.
- RAW-to-working-space compatibility needs a RawTherapee-informed test plan:
  camera profile lookup, white balance policy, highlight/headroom handling,
  demosaic choices, and raw.pixls.us coverage for common DSLR/mirrorless
  cameras used for film-copy work. Phone and uncommon-camera fixtures should be
  excluded from the first compatibility matrix.
- Cache cleanup/LRU limits are not implemented.
