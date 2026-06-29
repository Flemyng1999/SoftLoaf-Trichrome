# Repository Notes

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
