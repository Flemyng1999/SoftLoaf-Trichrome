# RAW Correctness Report

Date: 2026-06-29

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
