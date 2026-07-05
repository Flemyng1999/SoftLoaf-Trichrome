# Trichrome RAW Compatibility Status, 2026-07-05

Scope: synchronize the proven SoftLoaf-Negative RAW import/decode/diagnostic
boundary into SoftLoaf-Trichrome without changing trichrome recipe or workflow
semantics.

Negative references read for this pass:

- `/Users/flemyng/GitHub/SoftLoaf-Negative/engine/io/raw_decode.cpp`
- `/Users/flemyng/GitHub/SoftLoaf-Negative/tests/raw_decode_test.cpp`
- `/Users/flemyng/GitHub/SoftLoaf-Negative/tools/onda_rt_tap_compare.cpp`
- `/Users/flemyng/GitHub/SoftLoaf-Negative/tools/onda_c1_oracle_compare.cpp`
- `/Users/flemyng/GitHub/SoftLoaf-Negative/docs/reports/raw_rt_camera_sweep_2026-07-03/raw_compatibility_closeout_20260705.md`
- `/Users/flemyng/GitHub/SoftLoaf-Negative/docs/reports/raw_rt_camera_sweep_2026-07-03/current_abnormal_buckets_20260704.md`
- `/Users/flemyng/GitHub/SoftLoaf-Negative/docs/status/raw_decode_rt_living_status.md`

## Trichrome preflight

Product RAW entry points:

- `src/desktop_decoder.cpp`: RAW extension dispatch, LibRaw open/unpack,
  processed bitmap decode, guarded Rec.2020 probe path, provenance probe.
- `src/desktop_decoder.hpp`: desktop decode/provenance API.
- `include/softloaf_trichrome/raw_classification.hpp`: RAW class, policy, and
  cache/provenance identity.
- `include/softloaf_trichrome/raw_levels.hpp`: trusted black/white scale policy.
- `include/softloaf_trichrome/raw_camera_matrix.hpp`: built-in Leica SL2
  dcraw-style matrix override.
- `include/softloaf_trichrome/raw_decode_hints.hpp`: narrow camera/buffer-shape
  hints synced from the Negative RAW closeout.

Tests and diagnostics:

- `tests/trichrome_smoke.cpp`: classification, matrix, hint, white-level, cache
  identity, import/provenance smoke.
- `tests/trichrome_desktop_core_test.cpp`: desktop cache/core smoke.
- `tests/raw_fixture_regression_test.cpp`: fixture-gated real RAW regression
  over `tests/fixtures/raw_compat_manifest.tsv`; skips only when the configured
  fixture root is absent.
- `tools/softloaf_raw_probe.cpp`: one-file RAW decode/provenance CSV.
- `tools/softloaf_raw_provenance_matrix.cpp`: policy matrix for one or more
  RAW files.
- `tools/make_rt32_oracle.py`, `tools/softloaf_rt32_compare.cpp`,
  `tools/compare_rt32.py`: RT32 oracle generation/compare.

CMake targets:

- `trichrome_smoke`
- `trichrome_desktop_core_test`
- `raw_fixture_regression_test`
- `softloaf_raw_probe`
- `softloaf_raw_provenance_matrix`
- `softloaf_rt32_compare`

## Negative-to-Trichrome comparison

Directly synced:

- Sony packed full-color `filters=0 colors=3 color4_image=1 raw_image=0`
  admission remains fallback-only for camera-native output, with a narrow
  `sony_packed_color4_black_v1` black hint applied through LibRaw
  `user_black=512` when the exact Negative shape is present.
- Canon EOS 70D reduced full-color RAW gets `canon_70d_sraw_white_v1`, setting
  the white/saturation hint to `13480` only for
  `filters=0 colors=3 raw_image=0 color4_image=1`.
- Leica SL2 uses the built-in dcraw matrix override already present in
  `raw_camera_matrix.hpp`, and the RAW policy key now reflects this synced
  policy together with the new white/hint behavior.
- General RAW white selection now follows the current Negative/RT-like stance:
  prefer plausible LibRaw `maximum` over `linear_max`, then fall back to
  plausible `linear_max`/`data_maximum`.
- Sony ILCE-1M2 `5632x4096` processed output has the narrow
  `sony_ilce_1m2_lossless_m_raw_crop_v1` crop hint available at the processed
  bitmap boundary.

Rewritten for Trichrome architecture:

- Negative's sensor-native full-color fallback reads `color3_image` /
  `color4_image` directly. Trichrome currently uses LibRaw
  `dcraw_process()` / `dcraw_make_mem_image()`, so the synced behavior is
  expressed through LibRaw parameters, classification, processed-output crop,
  and probe-visible hint labels rather than a copied raw-buffer pipeline.
- Negative's RT tap comparison is diagnostic geometry tooling. Trichrome does
  not have the same staged tap pipeline, so the useful part synced here is
  orientation/crop status documentation and probe observability, not the tool.

Not synced:

- Negative's sensor-native Bayer/X-Trans demosaic, camconst JSON ingestion,
  masked-area black, aperture-scaled white, C1 affine/oracle compare, and
  broader RAW sweep tooling are not part of Trichrome's current input boundary.
- Phase One/Leaf native IIQ C1 parity is not implemented. The product stance is
  `BLOCKED_BY_VENDOR_SDK_PROFILE_CORRECTION_ORACLE`; use Capture One-exported
  16-bit ACEScg linear TIFF/DNG as the documented workaround/oracle.

## Product boundary

Supported normal gate:

- Ordinary still-camera Bayer/other CFA RAWs that LibRaw can open/unpack.
- Guarded linear Rec.2020 output remains enabled only for
  `bayer_or_other_cfa`.

Fallback-only:

- X-Trans, packed RGB, and packed four-color are camera-native fallback paths,
  not Rec.2020 correctness claims.
- Canon mRAW/sRAW representatives are treated as special full-color fallback
  cases; Canon 70D reduced RAW has a narrow white hint.

Parked or excluded from ordinary RAW bugs:

- Phase One/Leaf native IIQ: parked/blocked by vendor SDK or C1
  profile-correction oracle; C1 ACEScg TIFF/DNG is the workaround.
- Nikon HE/TicoRAW and Sony A7M5 `Compression=Next`:
  `NEEDS_RT_OR_LIBRAW_UPSTREAM`.
- Foveon/X3F: non-Bayer sensor boundary, excluded from the ordinary gate.
- Processed/float/computational DNG, ARQ/pixel-shift, Panasonic high-resolution
  RW2, monochrome RAW, and cinema/special RAW: parked feature boundaries unless
  a scoped product intake defines provenance, range, profile, and oracle policy.

## Verification

Build and unit tests:

```bash
cmake --build build --target trichrome_smoke trichrome_desktop_core_test softloaf_raw_probe softloaf_raw_provenance_matrix
ctest --test-dir build -R 'trichrome_smoke|trichrome_desktop_core' --output-on-failure
```

Result: both CTest targets passed locally. Linker emitted existing Homebrew
dependency deployment-target warnings for OpenCV/LibRaw/Little CMS.

Fixture-gated regression:

```bash
cmake --build build --target raw_fixture_regression_test
build/raw_fixture_regression_test
```

Result with `/Volumes/T7 Touch/Film/RAW/raw_pixls_us_camera_only_rsync`
mounted:

```text
PASS raw_fixture_regression rows=7 root="/Volumes/T7 Touch/Film/RAW/raw_pixls_us_camera_only_rsync"
```

Confirmed fixture facts:

| Fixture | Trichrome result | Evidence |
| --- | --- | --- |
| `SONY 2/ILCE-1M2/A1_07628_lossless_compressed_medium_full_frame.ARW` | `packed_four_color`, `fallback_only`; Rec.2020 blocked, camera-native decode ok | `raw_sensor_hints=sony_packed_color4_black_v1;sony_ilce_1m2_lossless_m_raw_crop_v1`; camera-native output `5628x3756`. |
| `Canon/EOS 70D/mRAW_CANON_EOS70D_01.CR2` | `packed_four_color`, `fallback_only`; camera-native decode ok | `raw_sensor_hints=canon_70d_sraw_white_v1`; output `4104x2736`. |
| `Canon/EOS 70D/sRAW_CANON_EOS70D_01.CR2` | `packed_four_color`, `fallback_only`; camera-native decode ok | `raw_sensor_hints=canon_70d_sraw_white_v1`; output `2736x1824`. |
| `Canon/EOS 5D Mark IV/B13A0732.CR2` | reduced Canon control remains fallback-only without 70D hint | `raw_sensor_hints=none`; output `3360x2240`. |
| `LEICA CAMERA AG 2/LEICA SL2/L1020010.DNG` | `bayer_or_other_cfa`, Rec.2020 supported | `raw_matrix_source=builtin_leica_sl2_dcraw_matrix_v1`; Rec.2020 output `8384x5605`. |
| `Nikon/Z 8/Nikon_Z8_raw_high_efficiency_hight.NEF` | upstream decoder boundary | `raw_unpack_failed`; make/model visible as `Nikon/Z 8`; no provenance claim. |
| `Sigma/DP1 Merrill/SDIM0555.X3F` | excluded special RAW boundary | `raw_open_failed`; no provenance claim; not treated as ordinary RAW support. |

Probe command used for the same fixture surface:

```bash
build/softloaf_raw_provenance_matrix \
  '/Volumes/T7 Touch/Film/RAW/raw_pixls_us_camera_only_rsync/SONY 2/ILCE-1M2/A1_07628_lossless_compressed_medium_full_frame.ARW' \
  '/Volumes/T7 Touch/Film/RAW/raw_pixls_us_camera_only_rsync/Canon/EOS 70D/mRAW_CANON_EOS70D_01.CR2' \
  '/Volumes/T7 Touch/Film/RAW/raw_pixls_us_camera_only_rsync/Canon/EOS 5D Mark IV/B13A0732.CR2' \
  '/Volumes/T7 Touch/Film/RAW/raw_pixls_us_camera_only_rsync/LEICA CAMERA AG 2/LEICA SL2/L1020010.DNG' \
  '/Volumes/T7 Touch/Film/RAW/raw_pixls_us_camera_only_rsync/Nikon/Z 8/Nikon_Z8_raw_high_efficiency_hight.NEF' \
  '/Volumes/T7 Touch/Film/RAW/raw_pixls_us_camera_only_rsync/Sigma/DP1 Merrill/SDIM0555.X3F'
```

Inference:

- These Trichrome fixture tests prove observable admission, fallback/blocking,
  hint selection, matrix-source selection, and product-path decode dimensions.
- They do not prove RawTherapee pixel parity for Trichrome's LibRaw processed
  bitmap path. Negative remains the RT parity reference for the deeper
  sensor-native path.

## Residual risks

- The Sony packed black/crop behavior is implemented at Trichrome's current
  LibRaw processed-output boundary, not Negative's direct raw-buffer boundary.
  A future sensor-native Trichrome decoder would need a deeper port.
- Processed/float DNG and Sigma sd Quattro linear DNG identification is still
  metadata-dependent; do not treat every LibRaw-openable DNG as a correctness
  claim without provenance review.
- Phase One native IIQ, Nikon HE/TicoRAW, Sony A7M5 Next compression, Foveon,
  pixel-shift/high-resolution RAW, mono, and cinema RAW remain explicitly
  outside the ordinary RAW gate.

Next safe step: if product scope needs stronger parity, add RT/C1 oracle output
for the manifest rows and compare pixels before changing more product decode
behavior.
