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
- Display handoff uses `TrichromeImageProvider` (`image://trichrome/...`) so QML
  does not infer state from temporary files.
- Preview cache identity is:

```text
source file identity + sensor mode + role order + cache schema
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

The format is the same family as SoftLoaf Negative:

```text
category=<name> key=value key=value
```

## Known Gaps

- Quit/close does not yet drain active workers gracefully.
- Export is still synchronous from the already-published preview image.
- Cache cleanup/LRU limits are not implemented.
- Full-resolution export and preview may diverge for future downscaled preview
  work until an explicit export task is added.
