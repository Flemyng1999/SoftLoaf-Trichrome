# Windows Path and Export Audit

Date: 2026-06-29

## Root Causes Found

- Path conversion relied on Qt/filesystem convenience conversion instead of an explicit Windows UTF-16 boundary.
- Some OpenCV tools passed `std::filesystem::path::string()` directly to `imread()`/`imwrite()`, which is unsafe for non-ASCII Windows paths.
- Project metadata/import reports used `path.string()` for persisted paths, which is not guaranteed to be UTF-8 on Windows.
- Filename sorting used `filename().string()`, which can misorder or fail for non-ASCII Windows paths.

## Fixes

- Centralized path conversion in `src/qt_path_utils.hpp`:
  - Windows: `QString` <-> `std::filesystem::path` via UTF-16.
  - Unix/macOS: UTF-8.
  - Local `QUrl` paths are normalized through `QDir::cleanPath()`.
- Added Qt byte read/write helpers for path-safe file I/O.
- Kept app export on `QImageWriter(QString)`, avoiding lossy narrow strings.
- Replaced core persisted path strings with UTF-8 helpers.
- Replaced UI filename sorting with `QString` comparison.
- Changed OpenCV tools to use `imdecode()`/`imencode()` with filesystem/QFile-backed bytes.
- Added a desktop core test covering paths with spaces and Chinese characters.

## Verification

```bash
cmake --build build --target trichrome_desktop_core_test trichrome_smoke softloaf_trichrome_check softloaf_raw_probe softloaf_rt32_compare softloaf_image_stats
ctest --test-dir build --output-on-failure
```

Both tests pass locally. Remaining `.string()` usages are limited to extension checks, test-only ASCII keys, or human-readable tool output.
