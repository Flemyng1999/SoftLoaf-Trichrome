# SoftLoaf Trichrome

Standalone Qt/QML + C++ desktop app for composing trichrome photo batches into
finished RGB frames.

The core workflow is:

- choose files or a folder;
- group photos three at a time as R/G/B captures;
- choose monochrome or Bayer source mode;
- preview the composed frame;
- export the active frame or all complete frames as PNG/TIFF/JPEG.

The C++ core also remains usable as a small library and CLI test target:

- trichrome file grouping and stable JSON import reports;
- project/input model structs needed by trichrome artifacts;
- artifact identity and stale/missing/dirty guards;
- structure correlation diagnostics;
- mono/Bayer role composition into RGB;
- atomic RGB16 NPY artifact writing.
- structured diagnostic logs;
- latest-wins preview compose tasks;
- half-size RAW reads plus 4096 px long-edge composed preview cache, with
  full-resolution RAW reads reserved for export;
- async full-resolution export tasks;
- explicit TIFF/PNG 8-bit or 16-bit export, with 16-bit as the professional
  default;
- content-addressed preview cache;
- QML display handoff through an image provider;
- fixed sRGB display/preview, with larger linear working/export spaces such as
  ACES AP0, ACEScg, ProPhoto RGB, and Rec.2020 reserved for export.

The desktop decoder supports the same import extension family as SoftLoaf
Negative: common RAW formats (`.3fr`, `.dng`, `.arw`, `.cr2`, `.cr3`, `.nef`,
`.raf`, `.rw2`, `.orf`, `.pef`, `.srw`, etc.) via LibRaw, plus TIFF/JPEG/PNG via
OpenCV.

## Build

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt);$(brew --prefix opencv)"
cmake --build build
ctest --test-dir build --output-on-failure
```

## Run The Desktop App

```bash
open build/softloaf_trichrome_app.app
```

See [docs/architecture.md](docs/architecture.md) for the current log, cache,
display, and task lifecycle boundaries. The working backlog lives in
[docs/todo.md](docs/todo.md).

Release and packaging notes live in [docs/release.md](docs/release.md).

## CLI

```bash
./build/softloaf_trichrome_check --synthetic-mono-oracle
./build/softloaf_trichrome_check --folder /path/to/rgb-roll
./build/softloaf_trichrome_check --file R.raf --file G.raf --file B.raf
```

## License

SoftLoaf Trichrome is licensed under the GNU General Public License v3.0. See
[LICENSE](LICENSE).

The GPLv3 permits commercial use and sale, but distributing modified versions
or binaries requires providing the corresponding source code under the same
license.

`SoftLoaf`, `SoftLoaf Trichrome`, official icons, signing identities, and store
listings are not licensed for use by forks. See [TRADEMARKS.md](TRADEMARKS.md).
Closed-source redistribution or official-brand use requires a separate
commercial agreement; see [COMMERCIAL_LICENSE.md](COMMERCIAL_LICENSE.md).

Third-party dependency notes are in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## RAW Test Data

Public RAW fixtures should not be committed to this repository. Keep fixture
manifests and checksums in git, and cache downloaded raw.pixls.us files locally
under:

```text
/Users/flemyng/Desktop/Film/raw.pixls.us/
```
