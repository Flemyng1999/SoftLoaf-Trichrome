# SoftLoaf Trichrome

Standalone C++20 extraction of SoftLoaf Negative's trichrome import core.

This repository intentionally starts as a small, Qt-free library:

- trichrome file grouping and stable JSON import reports;
- project/input model structs needed by trichrome artifacts;
- artifact identity and stale/missing/dirty guards;
- structure correlation diagnostics;
- mono/Bayer role composition into RGB;
- atomic RGB16 NPY artifact writing;
- a tiny CLI smoke checker and CTest target.

The desktop app integration, licensing gates, QML, ProjectController worker
coordination, RAW decode backend, and DCP profile lookup remain in
SoftLoaf-Negative for now. Those should be wired back through this library in a
separate integration stage.

## Build

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_PREFIX_PATH="$(brew --prefix opencv)"
cmake --build build
ctest --test-dir build --output-on-failure
```

## CLI

```bash
./build/softloaf_trichrome_check --synthetic-mono-oracle
./build/softloaf_trichrome_check --folder /path/to/rgb-roll
./build/softloaf_trichrome_check --file R.raf --file G.raf --file B.raf
```
