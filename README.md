# SoftLoaf Trichrome

<p align="center">
  <img src="docs/logo/softloaf-macos-icon.png" width="128" alt="SoftLoaf Trichrome app icon">
</p>

<p align="center">
  A standalone desktop workspace for composing three-shot trichrome captures into finished RGB images.
</p>

<p align="center">
  <a href="https://github.com/Flemyng1999/SoftLoaf-Trichrome/releases">Downloads</a>
  ·
  <a href="docs/release.md">Release Notes</a>
</p>

SoftLoaf Trichrome is a Qt/QML and C++ application for photographers working
with sequential red, green, and blue captures. It imports triplets from RAW or
standard image files, previews the composed color frame, and exports finished
images for editing or archival workflows.

![SoftLoaf Trichrome workspace showing imported frames and a composite preview](docs/screenshots/workspace-preview.png)

This project is currently beta software. The trichrome workflow, export path,
and release packaging are ready for broader testing, while RAW camera
correctness remains guarded by explicit provenance boundaries.

## Downloads

Prebuilt beta packages are published on the
[GitHub Releases](https://github.com/Flemyng1999/SoftLoaf-Trichrome/releases)
page.

| Platform | Package | Status |
| --- | --- | --- |
| macOS 15.0 or newer | DMG | Signed and notarized release workflow |
| Windows 10/11 x64 | Installer and portable zip | Unsigned beta tester build |

Windows SmartScreen may warn because the beta Windows builds may not yet be
Authenticode signed. Verify downloads with the `.sha256` files attached to each
release.

## Features

- Import individual files or folders of trichrome source images.
- Group captures into R/G/B frames by filename order or selection order.
- Switch between Bayer and monochrome source assumptions.
- Decode common camera RAW formats through LibRaw.
- Decode TIFF, JPEG, and PNG inputs through OpenCV.
- Preview composed trichrome frames in the desktop app.
- Cache previews for faster repeat interactions.
- Export the current frame or all complete frames.
- Export TIFF output.
- Choose 8-bit or 16-bit output for handoff.
- Select export color spaces including sRGB, Display P3, Adobe RGB, Rec.2020,
  ProPhoto RGB Linear, ACES AP0 Linear, and ACEScg/AP1 Linear.

## First Run Tutorial

SoftLoaf Trichrome expects each finished color image to come from three source
captures: one red-filter exposure, one green-filter exposure, and one
blue-filter exposure.

### 1. Prepare a capture folder

Put the files for a roll, contact sheet, or test set in one folder. The easiest
starting point is filename order:

```text
DSCF0195.RAF  red exposure
DSCF0196.RAF  green exposure
DSCF0197.RAF  blue exposure
DSCF0198.RAF  red exposure
DSCF0199.RAF  green exposure
DSCF0200.RAF  blue exposure
```

If your camera or scanning workflow records the channels in another order, keep
the files together and use the `Order` control after import.

### 2. Import the files

Click `Import` and choose either individual source files or a folder. SoftLoaf
will scan the selection, split the sources into three-channel frames, and show
the result in the left sidebar.

Each complete frame should show one `R`, one `G`, and one `B` source. If a frame
looks incomplete or the channels are assigned incorrectly, check the grouping
controls before exporting.

### 3. Choose the source assumptions

Use the controls under `Import` to describe the source set:

- `Sensor`: choose `Bayer` for normal color-camera RAW files, or `Monochrome`
  for monochrome camera or already-separated sources.
- `Grouping`: choose how SoftLoaf should form R/G/B triplets. `Filename` is the
  best default when captures are named in shooting order.
- `Order`: choose the channel order in the source sequence, such as `RGB` or
  `BGR`.

Changing these controls reprocesses the imported files, so you can correct the
setup without importing again.

### 4. Review the composite preview

Select a frame in the sidebar and inspect the large `Composite preview`. The
preview is meant for checking registration, channel order, and obvious exposure
problems before committing time to export.

Use the frame list to move through a roll. The count at the top of the sidebar
shows how many complete frames SoftLoaf found.

### 5. Export finished images

Click `Export` in the upper-right corner to open the export window.

![SoftLoaf Trichrome export settings window](docs/screenshots/export-settings.png)

Choose:

- `Range`: export the `Current frame` or `All complete frames`.
- `Format`: export as `TIFF`.
- `Color Space`: choose the handoff color space for your next editing or
  archival step.
- `Bit Depth`: choose `16-bit` for editing headroom, or `8-bit` when you need
  smaller files.
- `Suffix`: customize the text appended to exported filenames.

Click `Choose...` to pick an export folder, then click `Start Export`.

### Supported Inputs

Supported RAW containers include common camera files such as `.3fr`, `.dng`,
`.arw`, `.cr2`, `.cr3`, `.nef`, `.raf`, `.rw2`, `.orf`, `.pef`, and `.srw`,
plus TIFF/JPEG/PNG. RAW output uses explicit class/provenance boundaries; file
open support is not a blanket color-correctness guarantee for every camera.

## Build From Source

### macOS

Install dependencies with Homebrew for local development:

```bash
brew install cmake ninja pkg-config qt opencv libraw
```

Configure, build, and test:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt);$(brew --prefix opencv)"
cmake --build build
ctest --test-dir build --output-on-failure
```

Run the app:

```bash
open build/softloaf_trichrome_app.app
```

### Windows

Use Qt for MSVC, OpenCV and LibRaw from vcpkg, plus CMake, Ninja, and MSVC. The
release packaging route is documented in [docs/release.md](docs/release.md).

## License

SoftLoaf Trichrome is licensed under the GNU General Public License v3.0. See
[LICENSE](LICENSE).

The GPLv3 permits commercial use and sale, but distributing modified versions
or binaries requires providing the corresponding source code under the same
license.

`SoftLoaf`, `SoftLoaf Trichrome`, official icons, signing identities, and store
listings are not licensed for use by forks. See
[TRADEMARKS.md](TRADEMARKS.md). Closed-source redistribution or official-brand
use requires a separate commercial agreement; see
[COMMERCIAL_LICENSE.md](COMMERCIAL_LICENSE.md).

Third-party dependency notes are in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
