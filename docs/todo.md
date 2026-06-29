# SoftLoaf Trichrome Todo

Last updated: 2026-06-28

## Anchor

- Repository goal: a standalone Qt/QML + C++ desktop app that composes
  batched trichrome captures into finished RGB frames.
- Current MVP: file/folder import, three-image grouping, mono/Bayer mode,
  role-order selection, background preview compose, preview cache,
  async active/all export, and worker drain.
- Architecture reference: SoftLoaf Negative's project lifecycle, task policy,
  cache/logging style, display handoff, and export rigor.

## Assumptions

- The app stays independent from SoftLoaf Negative, but can reuse its proven
  patterns and later provide an adapter back into the parent app.
- The first complete product target is macOS desktop; Windows packaging can
  follow after the core workflow stabilizes.
- The core output should be photographically useful, not just a preview demo.

## Blockers

- No real DCP/profile/color-management pipeline yet.
- No metadata probe for camera/exposure consistency checks.
- Export is async, source-based, and supports large linear output spaces, but
  camera-profile-driven RAW colorimetry is not complete.
- Cache has no cleanup, quota, or inspection surface.

## Boundary

- Do not expand into SoftLoaf Negative film-negative features.
- Do not add project-file persistence, recent projects, or migration UI; this is
  a session-only batch tool.
- Do not add license gates before the core workflow is stable.
- Keep the trichrome core usable without Qt where practical.
- Prefer one finished vertical slice per change: code, focused test, docs.

## Task List

### P0 - Make It A Durable Working App

- [x] Extract standalone trichrome core and CLI smoke target.
- [x] Add Qt/QML desktop shell for import, grouping, preview, and export.
- [x] Add structured logs, preview cache, QML image-provider display handoff,
  and latest-wins preview workers.
- [x] Add async active-frame and export-all tasks from source files.
- [x] Add worker tracking and drain on clear and quit.
- [ ] Add explicit task progress and cancellation.
  - Exit criteria: preview/export jobs report current frame, total frames, and
    user cancellation; cancellation prevents stale UI publication.
- [ ] Add full-resolution export policy.
  - Exit criteria: export settings are explicit for format, bit depth, naming,
    overwrite behavior, and output folder; active/all export share the same
    snapshot model.

### P1 - Make The Photo Workflow Comfortable

- [ ] Add real metadata probing.
  - Exit criteria: RAW/EXIF probe reports camera model, capture time, ISO,
    shutter, aperture when available, and records probe failures.
- [ ] Add import validation UI.
  - Exit criteria: groups with inconsistent metadata or missing roles show a
    clear warning and suggested repair action.
- [ ] Add manual grouping editor.
  - Exit criteria: user can reorder files, swap R/G/B roles, skip a bad frame,
    and rebuild groups without re-importing.
- [ ] Add viewer controls.
  - Exit criteria: fit, 100%, zoom, pan, and reset work on large previews.
- [ ] Add image inspection overlays.
  - Exit criteria: channel solo, before/after, clipping warning, and histogram
    can be toggled without changing source/session data.
- [ ] Add alignment diagnostics to the UI.
  - Exit criteria: structure correlation results are visible per group and
    failures explain likely motion/misorder problems.
- [ ] Add simple registration.
  - Exit criteria: identity/manual offset/auto alignment options are stored in
    session data and reflected in preview/export.

### P1 - Color And Output Quality

- [x] Add large-space export targets and high-bit-depth selection.
  - Exit criteria: TIFF/DNG-family outputs expose 8-bit and 16-bit choices; export defaults to
    16-bit and offers large linear spaces rather than display-only targets.
- [ ] Define the color pipeline.
  - Exit criteria: document source linear state, working space, display view,
    and export encoding; preview/display remains fixed sRGB, while export uses
    named target transforms.
- [ ] Add DCP/profile lookup.
  - Exit criteria: supported cameras can resolve profiles; unknown cameras
    fall back predictably with a visible warning.
- [ ] Add RawTherapee-informed RAW compatibility matrix.
  - Exit criteria: import/decode/export decisions are checked against
    RawTherapee-style RAW handling for common film-copy cameras using
    public raw.pixls.us fixtures, excluding phones and uncommon cameras from
    the first matrix.
- [ ] Add ICC-aware SDR export.
  - Exit criteria: TIFF/DNG-family outputs embed the correct ICC and tests verify
    metadata for each supported output space.
- [ ] Verify high-bit-depth export metadata.
  - Exit criteria: 16-bit TIFF/DNG-family output does not round-trip
    through 8-bit `QImage`, and automated tests inspect the written files.
- [ ] Add regression samples.
  - Exit criteria: a small fixture set covers mono, Bayer, missing frame,
    swapped role, and export metadata smoke cases.

### P1 - Cache, Logs, And Diagnostics

- [ ] Add cache quota and cleanup.
  - Exit criteria: cache supports size limit, LRU cleanup, corrupt-file
    eviction, and a test for false-hit prevention.
- [ ] Add cache inspection UI.
  - Exit criteria: user can see cache size, clear cache, and view last cache
    result for the active frame.
- [ ] Add user-facing error panel.
  - Exit criteria: failed import/preview/export events produce readable
    messages with details available for diagnosis.
- [ ] Add diagnostic bundle export.
  - Exit criteria: bundle includes app version/build id, recent logs, task
    failures, and relevant cache keys without copying source photos unless
    explicitly requested.

### P2 - Packaging And Delivery

- [ ] Add app identity/version metadata.
  - Exit criteria: app bundle carries stable bundle id, version, build id, and
    about-window fields.
- [ ] Add macOS dependency packaging.
  - Exit criteria: packaged app runs on a clean machine with bundled Qt,
    LibRaw, OpenCV, and required plugins.
- [ ] Add signed DMG workflow.
  - Exit criteria: Developer ID signed, hardened runtime enabled, notarized,
    stapled, and smoke-tested from mounted DMG.
- [ ] Add CI.
  - Exit criteria: GitHub Actions or equivalent runs configure, build, CTest,
    and at least one packaging smoke check.
- [ ] Add formatting/lint policy.
  - Exit criteria: developer command checks C++ formatting and QML formatting
    consistently.
- [ ] Add crash/log collection policy.
  - Exit criteria: local crash/log locations are documented and visible in the
    app without network upload.

### P2 - Product Boundary

- [ ] Define license/entitlement boundary.
  - Exit criteria: commands can be gated outside render/color kernels and tests
    prove locked features fail closed.
- [ ] Decide parent-app integration strategy.
  - Exit criteria: adapter direction is documented with source-of-truth model,
    ownership boundaries, and parity tests.
- [ ] Add user manual.
  - Exit criteria: documents import, grouping, export, troubleshooting, and
    known limitations with screenshots.

## Next Suggested Order

1. Explicit progress/cancel for export-all.
2. Full-resolution export settings and high-bit-depth path.
3. Metadata probe plus import validation UI.
4. Color pipeline design and DCP/profile lookup.
5. Cache quota and user-facing diagnostics.
