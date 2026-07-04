# RAW Correctness Report

Date: 2026-07-01

## 2026-07-04 Targeted RAW Compatibility Sync

Goal: sync the confirmed SoftLoaf-Negative product RAW decode/admission fixes
without importing the full Negative RAW sweep taxonomy or diagnostic-only compare
changes.

Product fixes synced:

- Leica SL2 DNG matrix handling now has a built-in dcraw matrix override for the
  Trichrome camera-to-XYZ metadata path. The matrix is implemented in
  `include/softloaf_trichrome/raw_camera_matrix.hpp` and applied by
  `src/desktop_decoder.cpp` before falling back to LibRaw `cam_xyz`.
- Sony packed full-color admission is covered for the LibRaw shape
  `filters=0 colors=3 color4_image=1`. Trichrome already classified this as
  `packed_four_color` fallback; `tests/trichrome_smoke.cpp` now protects that
  exact case and verifies that camera-native fallback remains allowed.
- RAW policy/provenance identity includes the Leica SL2 matrix change so preview
  cache and artifact identities do not silently reuse pre-override RAW metadata.

Not changed:

- No full RAW sweep was rerun or copied into this report.
- No EXCLUDE/PARK strategy was changed for Nikon HE/TicoRAW, X3F/Foveon,
  ARQ/Pixel Shift, CinemaDNG, float/processed DNG, Canon mRAW/sRAW, or
  Phase/Leaf IIQ.
- SoftLoaf-Negative `onda_rt_tap_compare --align-orientation` was not migrated:
  Trichrome does not have the same staged RT tap compare tool, and that change
  is diagnostic geometry alignment rather than a product RAW decode fix.

Targeted verification:

```bash
cmake --build build --target trichrome_smoke trichrome_desktop_core_test softloaf_raw_probe
ctest --test-dir build -R 'trichrome_smoke|trichrome_desktop_core' --output-on-failure
```

No Leica SL2 or Sony A1M2/FX2 packed sample was found at the documented local
raw.pixls.us paths during this sync, so no new real-sample probe result is
claimed here.

## 2026-07-01 RAW Boundary Update

Goal: default RAW export/probe output remains a guarded linear Rec.2020 path,
not a blanket claim that every LibRaw-openable RAW is correctly developed.

Current boundary:

- `include/softloaf_trichrome/raw_classification.hpp` classifies LibRaw metadata
  into Bayer/other CFA, X-Trans, packed RGB, packed four-color, monochrome,
  Foveon, and unknown `filters == 0`.
- `DecodeRawToLinearRec2020()` only proceeds for Bayer/other CFA. X-Trans and
  packed full-color classes are marked fallback-only, so they no longer get
  silently tagged as correct `rec_2020_linear` output.
- `DecodeLinear()` camera-native fallback accepts Bayer/other CFA, X-Trans,
  packed RGB, and packed four-color; it rejects Foveon, monochrome, and unknown
  `filters == 0`.
- Foveon, monochrome, and unknown `filters == 0` are protected boundaries, not
  supported linear Rec.2020 paths.
- Preview cache identity includes `raw_decode_policy`; artifact signatures also
  include the current RAW policy via `input_preprocess_sig`, and compose algo
  version is bumped to 7.

Status buckets:

| class | status | note |
| --- | --- | --- |
| Bayer/other CFA | supported for guarded default Rec.2020 path | Still not an RT parity claim without sample comparison. |
| X-Trans | fallback-only | Do not rely on LibRaw default demosaic as RT parity. |
| packed RGB | fallback-only | LibRaw full-color fallback only. |
| packed four-color | fallback-only | LibRaw full-color fallback only. |
| monochrome | unsupported for default RAW path | Must not be labeled correct Rec.2020 by opening successfully. |
| Foveon | unsupported for default RAW path | Not a Bayer/X-Trans problem. |
| unknown `filters == 0` | unsupported for default RAW path | Requires explicit classification before enabling. |

Minimal tests:

```bash
cmake --build build --target trichrome_smoke trichrome_desktop_core_test
ctest --test-dir build -R 'trichrome_smoke|trichrome_desktop_core' --output-on-failure
```

No new RAW sample matrix was added in this update; the classification boundary
is protected, but no additional camera family coverage is claimed.

## 2026-07-01 RAW Provenance Update

Goal: make RAW decode results carry structured provenance so downstream checks do
not have to infer RAW behavior from `color_space` strings alone.

Current status:

- `RawDecodeProvenance` records raw class, Rec.2020 policy, decode mode,
  fallback status, target color space, and the active RAW policy key.
- `ImageBuf` now carries this provenance for RAW decodes. Camera-native fallback
  output records the classified RAW class and fallback status; guarded Rec.2020
  output is still only produced for Bayer/other CFA.
- `softloaf_raw_probe` prints structured RAW provenance columns, including a
  stable provenance signature.
- `softloaf_raw_probe --provenance-only` can inspect classification and policy
  without writing a TIFF. Normal probe failures also print provenance CSV when
  LibRaw metadata can be opened/unpacked, so fallback-only and unsupported
  boundaries are visible instead of collapsing to an opaque decode failure.
- Preview cache identity now includes `raw_provenance_identity` in addition to
  the RAW policy field. Artifact `input_preprocess_sig` uses the shared RAW
  provenance pipeline identity, so pixel-affecting RAW policy/provenance changes
  dirty cached artifacts.
- Full artifact rebuilds also append the actual per-source RAW provenance
  signatures returned by decode, so artifact identity records source-level RAW
  class/policy/fallback behavior when it is available.
- Full artifact rebuilds write readable per-source RAW provenance fields onto
  each `ProjectTrichromeSource` (`raw_class`, `raw_policy`, decode mode,
  fallback status, target color space, and signature), so the behavior is
  inspectable and not only present as a hash input.
- `softloaf_raw_provenance_matrix` emits a CSV matrix for one or more RAW files
  across guarded Rec.2020 and camera-native fallback targets.

Boundary:

- supported: Bayer/other CFA for guarded linear Rec.2020.
- fallback-only: X-Trans, packed RGB, packed four-color for camera-native
  fallback only.
- unsupported: monochrome, Foveon, and unknown `filters == 0`.

Minimal tests:

```bash
cmake --build build --target trichrome_smoke trichrome_desktop_core_test softloaf_raw_probe
ctest --test-dir build -R 'trichrome_smoke|trichrome_desktop_core' --output-on-failure
```

Optional local sample classification matrix:

```bash
cmake --build build --target softloaf_raw_provenance_matrix
build/softloaf_raw_provenance_matrix /path/to/sample.CR3 /path/to/sample.RAF \
  > /tmp/softloaf_raw_provenance_matrix.csv
```

No new real RAW samples were added for this provenance update. The
classification/provenance boundary is protected, but no new camera coverage is
claimed.

Local provenance matrix run:

- Output: `/tmp/softloaf_raw_provenance_matrix.csv`
- Samples found locally: CR2, RAF, DNG, 3FR, RW2.
- Result: all five available samples classified as `bayer_or_other_cfa`; both
  guarded `rec_2020_linear` and `camera_native_linear` targets were policy-ok
  for these files.
- This is a local classification/provenance smoke only. It does not cover
  X-Trans, packed full-color, monochrome, Foveon, or unknown `filters == 0`, and
  it is not a new camera-family correctness claim.

## Current Project Path

- RAW read entry: `src/desktop_decoder.cpp`, `DecodeLinear()` dispatches RAW-like extensions to `DecodeRawImage()`.
- Decoder: LibRaw `open_file()` -> `unpack()` -> `dcraw_process()` -> `dcraw_make_mem_image()`.
- Current RAW output: 16-bit LibRaw processed bitmap converted to `CV_32F` by dividing by 65535, tagged as `ColorState::kCameraLinear` and `camera_native_linear_preview/export`.
- Preview mode: LibRaw half-size and no interpolation.
- Export mode: full-resolution LibRaw interpolation.
- Trichrome compose path: `ComposeGroupSequential()` extracts role channels for Bayer, then `ComposeMonoRgb()`.
- Color/export path: `ComposeGroupToEncodedImage()` normalizes per-channel white and calls `LinearRgbToQImage()`.
- Rec.2020 export path: Qt `QColorSpace::Primaries::Bt2020` with linear transfer, but the upstream composed buffer is `linear_srgb_trichrome`, not an independently verified RT scene-linear Rec.2020 RAW development.
- Test/tool entry points: CTest targets in `CMakeLists.txt`, existing `softloaf_trichrome_check`, new `softloaf_raw_probe`, and Python tools under `tools/`.

## RT32 Oracle Toolchain

RawTherapee 32-bit float oracle generation is now scripted by:

```bash
python3 tools/make_rt32_oracle.py /path/to/raws --out-dir /tmp/softloaf_rt32
```

For paths containing spaces, prefer a newline-delimited input list:

```bash
python3 tools/make_rt32_oracle.py \
  --input-list /tmp/softloaf_raw_samples.txt \
  --rawtherapee-cli /Volumes/rs_bucket/Data.flemyng/film/rawtherapee_market_core_2026-06-29/rawtherapee-src/build-rt/rtgui/rawtherapee-cli \
  --pp3 /Volumes/rs_bucket/Data.flemyng/film/rawtherapee_market_core_2026-06-29/tools/rawtherapee_linear_rec2020.pp3 \
  --out-dir /tmp/softloaf_rt32
```

The generated command is:

```bash
rawtherapee-cli -q -Y -o <out> -p <linear_rec2020_rt32.pp3> -t -b32 -c <raw>
```

The default pp3 pins camera input, Rec.2020 working profile, no TRC, RTv4 Rec2020 output, and disables the creative/lookup/baseline paths identified in the RAW/RT anchor notes. Do not replace this with 16-bit TIFF when judging scene-linear correctness.

## Project Decode Dump

Build the project, then dump the current LibRaw export-mode output. The default
probe output is now scene-linear Rec.2020:

```bash
cmake --build build --target softloaf_raw_probe
build/softloaf_raw_probe --input /path/to/sample.CR3 --output /tmp/softloaf_project/sample.project.tiff
```

To reproduce the older camera-native LibRaw processed output:

```bash
build/softloaf_raw_probe --input /path/to/sample.CR3 --output /tmp/softloaf_project/sample.project.tiff --space camera-native
```

Batch example:

```bash
mkdir -p /tmp/softloaf_project
find /path/to/raws -type f \( -iname '*.cr2' -o -iname '*.cr3' -o -iname '*.nef' -o -iname '*.arw' -o -iname '*.raf' -o -iname '*.orf' -o -iname '*.dng' -o -iname '*.pef' -o -iname '*.3fr' -o -iname '*.x3f' \) \
  -exec sh -c 'build/softloaf_raw_probe --input "$1" --output "/tmp/softloaf_project/$(basename "${1%.*}").project.tiff"' sh {} \;
```

## Difference CSV

```bash
cmake --build build --target softloaf_rt32_compare
build/softloaf_rt32_compare \
  --oracle-dir /tmp/softloaf_rt32 \
  --project-dir /tmp/softloaf_project \
  --out-csv /tmp/softloaf_raw_diff.csv
```

CSV fields include:

```text
sample, oracle, project, status, oracle_width, oracle_height, project_width, project_height, crop_offset, low_mean_abs, gain_mean_abs, channel_ratios, failure_reason
```

## White-Level Fix

Added `SelectTrustedRawWhiteLevel()` in `include/softloaf_trichrome/raw_levels.hpp`.

Policy:

- Use `linear_max` only when it is greater than black plus a safety margin.
- Reject pathological values such as Canon CR3 CRAW `linear_max=153` with black around 511.
- Fall back to LibRaw `maximum`, then `data_maximum`, only when those are also above black plus margin.
- Apply the trusted value through LibRaw `params.user_sat`.

Unit coverage:

```bash
cmake --build build --target trichrome_smoke
ctest --test-dir build -R trichrome_smoke --output-on-failure
```

## Sample Matrix

Run directory:

```text
/tmp/softloaf_raw_rt32_20260629_200733
```

RT oracle path:

```text
/Volumes/rs_bucket/Data.flemyng/film/rawtherapee_market_core_2026-06-29/rawtherapee-src/build-rt/rtgui/rawtherapee-cli
```

PP3:

```text
/Volumes/rs_bucket/Data.flemyng/film/rawtherapee_market_core_2026-06-29/tools/rawtherapee_linear_rec2020.pp3
```

ExifTool and `tiffinfo` confirmed the sampled RT oracles are 32-bit IEEE floating-point RGB TIFFs. The project probe also wrote 32-bit IEEE floating-point RGB TIFFs.

This is a 10-file smoke matrix, one sample per available local family. It is not a market-coverage claim.

Root causes found so far:

- Fixed: project output used LibRaw's active area, which was 8 px wider and 8 px taller than RT output for all 10 samples. The Rec.2020 probe now crops 4 px from each side to match RT active-area dimensions for this matrix.
- Fixed for probe: project comparison output was camera-native/LibRaw processed RGB while the oracle is scene-linear Rec.2020. The probe now has a Rec.2020 path using LibRaw linear XYZ plus an XYZ-to-Rec.2020 matrix.
- Not fixed: the remaining difference is not a simple WB or channel gain issue. `--wb none` improved Fuji slightly but worsened Canon/Nikon. Pixel stats show RT32 preserves float scene-linear range beyond 0..1 and negative values, while LibRaw `dcraw_make_mem_image()` is still a 16-bit processed intermediate. Matching RT32 more closely requires a raw-domain float pipeline or a deeper RT-equivalent development path, not just changing the output profile.

Before these fixes, the camera-native comparison averaged `low_mean_abs=0.2436191394` and `gain_mean_abs=0.1182381782`. After active-area crop plus Rec.2020 probe output, the comparison averages `low_mean_abs=0.2097294919` and `gain_mean_abs=0.1211764855`; dimensions now match exactly for all 10 samples.

| sample | format | RT32 status | project status | oracle size | project size | low_mean_abs | gain_mean_abs | channel ratios | crop offset |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Nikon_Z_fc_14bit_uncompressed | NEF | ok | ok | 5592x3720 | 5592x3720 | 0.176058441 | 0.0899629893 | 0.38405171;0.414397111;0.433567769 | oracle_x=0;oracle_y=0;project_x=0;project_y=0 |
| OM_Systems_OM1_RAW_Image_20mpx_P1010025 | ORF | ok | ok | 5212x3904 | 5212x3904 | 0.238224821 | 0.179291034 | 0.450143833;0.414426222;0.386143698 | oracle_x=0;oracle_y=0;project_x=0;project_y=0 |
| DSCF5075 | RAF | ok | ok | 11654x8738 | 11654x8738 | 0.103431686 | 0.0927983922 | 0.183566432;0.498834726;1.28624865 | oracle_x=0;oracle_y=0;project_x=0;project_y=0 |
| IMGP8550 | PEF | ok | ok | 7368x4924 | 7368x4924 | 0.245526957 | 0.131015906 | 0.299177395;0.362545531;0.503047096 | oracle_x=0;oracle_y=0;project_x=0;project_y=0 |
| _DSC1066 | ARW | ok | ok | 6016x4016 | 6016x4016 | 0.190247202 | 0.138967397 | 0.393972143;0.362713879;0.342901679 | oracle_x=0;oracle_y=0;project_x=0;project_y=0 |
| 5G4A9394 | CR2 | ok | ok | 5788x3862 | 5788x3862 | 0.221241709 | 0.0953891561 | 0.458461017;0.265906539;0.136675721 | oracle_x=0;oracle_y=0;project_x=0;project_y=0 |
| M10R0324 | DNG | ok | ok | 7864x5200 | 7864x5200 | 0.228607423 | 0.0779283638 | 0.378194002;0.337395704;0.228450693 | oracle_x=0;oracle_y=0;project_x=0;project_y=0 |
| 20260109_0001 | RW2 | ok | ok | 5776x4336 | 5776x4336 | 0.253232293 | 0.123490857 | 1.09309741;0.380216856;0.124385648 | oracle_x=0;oracle_y=0;project_x=0;project_y=0 |
| B0007463 | 3FR | ok | ok | 8270x6200 | 8270x6200 | 0.223002558 | 0.173941544 | 0.990438593;0.342372236;0.143589151 | oracle_x=0;oracle_y=0;project_x=0;project_y=0 |
| Canon_EOS_R_RAW_ISO_100 | CR3 | ok | ok | 6734x4490 | 6734x4490 | 0.217721829 | 0.108979216 | 0.443801318;0.424602434;0.406660418 | oracle_x=0;oracle_y=0;project_x=0;project_y=0 |

## Format Support Status

| family | current status | notes |
| --- | --- | --- |
| Canon CR2 | Smoke decoded and compared | RT32/project both output; real color/path difference remains. |
| Canon CR3/CRAW | CR3 smoke decoded and compared; white-level guard added | EOS R CR3 output differs from RT32. Known R50 V CRAW `linear_max <= black` issue is unit-covered but still needs that exact file in the matrix. |
| Nikon NEF, including HE/HE* | 14-bit uncompressed NEF smoke decoded and compared | HE/HE* compression variants still need explicit samples. |
| Sony ARW compressed | ARW smoke decoded and compared | Compression mode should be recorded per sample before broader claims. |
| Fuji RAF/X-Trans | RAF/X-Trans smoke decoded and compared | Keep separate from Bayer conclusions. |
| DNG | Native Leica DNG smoke decoded and compared | Converted DNG still needs separate coverage. |
| Pentax PEF | Smoke decoded and compared | Real difference remains. |
| Hasselblad 3FR/FFF | 3FR smoke decoded and compared | FFF still needs sample validation. |
| Olympus ORF | Smoke decoded and compared | Real difference remains. |
| Panasonic RW2 | Smoke decoded and compared | Real difference remains. |
| Sigma/Foveon X3F | Not in import extension list; no X3F sample in this smoke | Do not merge with Bayer/X-Trans conclusions. If LibRaw processed RGB succeeds, treat as fallback smoke, not full correctness. |

## Final Buckets

- Fixed: RT32 tooling, project RAW dump tooling, diff CSV tooling, path-safe input lists, and white-level guard for bad `linear_max <= black`.
- Oracle error/false positive: 16-bit RT TIFF clipping is explicitly excluded; no 16-bit oracle results used.
- Current format not supported: X3F is not currently importable by extension and needs a separate Foveon path.
- Still real differences: all 10 smoke samples show material differences after low-pass comparison, even after simple per-channel gain correction. The dominant known remaining cause is LibRaw's 16-bit processed intermediate and non-RT development pipeline, not the already-fixed active-area or output-space mismatch.
