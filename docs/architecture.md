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
- Export is an explicit background task. Active-frame export and export-all
  both re-compose from the source files instead of saving the published preview.
- Open/clear/quit request worker interruption and drain known workers before
  replacing project state or leaving the app.
- `.sltrichrome` project files persist source paths, relative source paths,
  selection order, grouping mode, role order, sensor mode, and active frame.
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
- `project.open`
- `project.save`
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
- Project files preserve source paths but do not yet provide a missing-file
  recovery UI or recent-project list.
- Export has an async full-source task, but it still writes display-referred
  `QImage` output only. Format-level bit depth, ICC metadata, and color policy
  are not yet equivalent to SoftLoaf Negative export.
- Cache cleanup/LRU limits are not implemented.
