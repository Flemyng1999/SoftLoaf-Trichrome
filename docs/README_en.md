<p align="center">
  <img src="logo/softloaf-macos-icon.png" width="128" alt="SoftLoaf Trichrome app icon">
</p>

<h1 align="center">SoftLoaf Trichrome</h1>

<p align="center">
  <strong>Three exposures, one color image.</strong>
</p>

<p align="center">
  <a href="../README.md">简体中文</a> ·
  <a href="./README_en.md">English</a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/macOS-15%2B-222222" alt="macOS 15+">
  <img src="https://img.shields.io/badge/Windows-10%20%7C%2011-0078d4" alt="Windows 10/11">
  <img src="https://img.shields.io/badge/TIFF-8--bit%20%7C%2016--bit-4f6f52" alt="TIFF 8-bit or 16-bit">
  <img src="https://img.shields.io/badge/license-GPLv3-bd0000" alt="GPLv3 license">
</p>

SoftLoaf Trichrome combines three color-separation photographs into one color
image. Import photographs made through red, green, and blue light sources or
filters, check the channel order and composite, then export a TIFF.

![SoftLoaf Trichrome workspace](screenshots/workspace-preview.png)

## Preparing Files

Keep the three exposures for each image together and in capture order. For
example:

```text
DSCF0195.RAF  red light/filter exposure
DSCF0196.RAF  green light/filter exposure
DSCF0197.RAF  blue light/filter exposure
DSCF0198.RAF  red light/filter exposure
DSCF0199.RAF  green light/filter exposure
DSCF0200.RAF  blue light/filter exposure
```

You can import individual files or a whole folder. By default, files are sorted
by filename and grouped into sets of three. Change `Grouping` if you need to
preserve the order in which the files were selected.

If the captures are not in RGB order, choose RBG, GRB, GBR, BRG, or BGR under
`Order`. These settings can be changed after import.

Use `Sensor` to describe the source files. Choose `Bayer` for RAW files from a
regular color camera, or `Monochrome` for a monochrome camera or
already-separated grayscale channels.

## Preview And Export

The Frames list shows each group assembled by the app. Select a frame to check
the composite, channel order, exposure, and obvious registration problems.

Previewing is not required for export. The app reads and composites the source
files during export, so you do not need to open every frame first.

Click `Export` in the upper-right corner.

![SoftLoaf Trichrome export window](screenshots/export-settings.png)

- `Current frame`: export the active frame.
- `Selected frames`: export complete frames selected in the sidebar. Cmd/Ctrl+A
  selects all frames.
- `All complete frames`: export every complete frame.

TIFF output is available in 8-bit or 16-bit, with common RGB, ProPhoto RGB, and
ACES color spaces. Use 16-bit when the files will be edited further.

## File Formats

The app supports common camera RAW files, TIFF, JPEG, and PNG. Being able to
read a RAW format does not mean that its camera has been validated for color
accuracy. See the [RAW correctness report](raw_correctness_report.md) for the
current record.

## License

The source code is licensed under the
[GNU General Public License v3.0](../LICENSE). Distributing modified versions or
binaries requires providing the corresponding source code under GPLv3.

The `SoftLoaf` and `SoftLoaf Trichrome` names, official icons, signing
identities, and store listings are not included in the GPLv3 license. See
[TRADEMARKS.md](../TRADEMARKS.md) and
[COMMERCIAL_LICENSE.md](../COMMERCIAL_LICENSE.md). Third-party dependency notices
are in [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md).
