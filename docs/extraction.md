# Extraction Notes

This repository started as a standalone extraction of the trichrome module from
SoftLoaf-Negative and now includes a focused Qt/QML desktop app around that
core.

## Included

- `InputRecipe` / `ProjectMeta` subset needed by trichrome import artifacts.
- Trichrome import grouping and stable JSON reports.
- Metadata-policy hooks with a stub default probe.
- Artifact identity, source probes, preview/full tier signatures, and pipeline
  readiness guards.
- Structure correlation diagnostics.
- Mono and Bayer role composition through caller-supplied decoders.
- Atomic RGB16 NPY artifact writing.
- Qt/QML desktop shell for import, grouping, preview, and export.
- LibRaw/OpenCV desktop decoder for the same import extension family as the
  parent app.
- Smoke CLI and CTest target.

## Not Included Yet

- Parent app `ProjectController` integration.
- Licensing gates.
- DCP/profile lookup.
- SoftLoaf project JSON persistence.
- Full historical test suite from the parent repository.

## Re-Integration Plan

1. Keep this repository building independently.
2. Add adapter functions in SoftLoaf-Negative that translate the app's richer
   project model to this library's narrow trichrome model.
3. Replace direct includes in the app with this library once the adapter tests
   prove parity.
4. Move RAW/DCP decode behind explicit decoder callbacks instead of letting the
   trichrome core own those dependencies.
