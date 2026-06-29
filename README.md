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

This project is currently alpha software. The image pipeline, RAW compatibility,
and release packaging are actively being hardened.

## Downloads

Prebuilt alpha packages are published on the
[GitHub Releases](https://github.com/Flemyng1999/SoftLoaf-Trichrome/releases)
page.

| Platform | Package | Status |
| --- | --- | --- |
| macOS 15.0 or newer | DMG | Signed and notarized release workflow |
| Windows 10/11 x64 | Installer and portable zip | Unsigned alpha tester build |

Windows SmartScreen may warn because the alpha Windows builds are not yet
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
- Export TIFF or DNG-family output.
- Choose 8-bit or 16-bit output for handoff.
- Select export color spaces including sRGB, Display P3, Adobe RGB, Rec.2020,
  ProPhoto RGB Linear, ACES AP0 Linear, and ACEScg/AP1 Linear.

## Workflow

1. Click `Import` and choose three-shot source files or a folder.
2. Choose the sensor mode: `Bayer` or `Monochrome`.
3. Choose grouping mode and channel order, such as `RGB` or `BGR`.
4. Select a complete frame from the sidebar.
5. Preview the composed result.
6. Export a single frame or all complete frames.

Supported RAW formats include common camera files such as `.3fr`, `.dng`,
`.arw`, `.cr2`, `.cr3`, `.nef`, `.raf`, `.rw2`, `.orf`, `.pef`, and `.srw`,
plus TIFF/JPEG/PNG.

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
