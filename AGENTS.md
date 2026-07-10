# Repository Notes

## Windows 11 303mini Test Environment

When testing Windows builds on `303mini`, use the fixed environment notes in
`docs/windows11_303mini_environment.md`. Prefer the documented paths for
Visual Studio Build Tools, Qt, CMake/Ninja, vcpkg, the reusable installed
dependency tree, and the scratch checkout. Do not rediscover the Windows
toolchain from scratch unless one of those paths fails.

## macOS Notarization Secret Pointer

The local Apple app-specific password for notarization is stored outside this
repository at:

```text
/Users/flemyng/.config/softloaf-trichrome/notary.env
```

Do not copy this secret into tracked files. To sync it into GitHub Actions:

```bash
sed -n 's/^APPLE_APP_SPECIFIC_PASSWORD=//p' \
  /Users/flemyng/.config/softloaf-trichrome/notary.env \
  | gh secret set APPLE_APP_SPECIFIC_PASSWORD --app actions
```

## Color Management Reference Memory

For color-management work, treat
[colour-science/colour](https://github.com/colour-science/colour) as the
scientific reference for RGB colourspace constants, ACES AP0 / ACEScg AP1
definitions, whitepoints, normalised primary matrices, chromatic adaptation, and
transfer functions.

Keep project color constants centralized in
`include/softloaf_trichrome/color_management.hpp`. Do not add one-off
primaries, RGB-to-XYZ matrices, ACES matrices, or transfer-curve snippets inside
UI/controller code. If a color transform changes, add or update tests against
colour-science reference values.
