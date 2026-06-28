# Contributing

Thanks for your interest in SoftLoaf Trichrome.

This project is GPLv3 licensed. By submitting a contribution, you agree that
your contribution may be distributed under GPLv3.

At this early stage, please open an issue before sending large pull requests,
new dependencies, licensing changes, packaging changes, or changes that affect
RAW/color-management policy. The project may require a separate contributor
agreement before accepting substantial contributions so future commercial
dual-licensing remains possible.

## Development

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt);$(brew --prefix opencv)"
cmake --build build
ctest --test-dir build --output-on-failure
```

Keep changes focused. Include tests or verification notes when changing decode,
compose, cache, export, or color-management behavior.

Do not commit local RAW fixture downloads, generated exports, app bundles, DMGs,
MSIX packages, or build directories.
