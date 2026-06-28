# SoftLoaf Trichrome

Standalone Qt/QML + C++ desktop app for composing trichrome photo batches into
finished RGB frames.

The core workflow is:

- choose files or a folder;
- group photos three at a time as R/G/B captures;
- choose monochrome or Bayer source mode;
- preview the composed frame;
- export PNG/TIFF/JPEG.

The C++ core also remains usable as a small library and CLI test target:

- trichrome file grouping and stable JSON import reports;
- project/input model structs needed by trichrome artifacts;
- artifact identity and stale/missing/dirty guards;
- structure correlation diagnostics;
- mono/Bayer role composition into RGB;
- atomic RGB16 NPY artifact writing.
- structured diagnostic logs;
- latest-wins preview compose tasks;
- content-addressed preview cache;
- QML display handoff through an image provider.

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
display, and task lifecycle boundaries.

## CLI

```bash
./build/softloaf_trichrome_check --synthetic-mono-oracle
./build/softloaf_trichrome_check --folder /path/to/rgb-roll
./build/softloaf_trichrome_check --file R.raf --file G.raf --file B.raf
```
