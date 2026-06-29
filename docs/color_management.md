# Color Management

SoftLoaf Trichrome uses
[colour-science/colour](https://github.com/colour-science/colour) as the
reference model for RGB colourspace definitions: primaries, whitepoints,
normalised primary matrices, Bradford chromatic adaptation, and transfer
functions.

## Current Export Pipeline

The composed trichrome buffer is treated as linear sRGB D65. Export converts that
state into the requested RGB target with:

1. RGB primary matrix construction from xy primaries and whitepoint.
2. Bradford chromatic adaptation when the target whitepoint differs from D65.
3. Target transfer encoding: linear, sRGB, or explicit gamma.
4. Little CMS generated ICC profile attachment before TIFF-family writing.

Supported profile-backed targets are:

- sRGB
- Display P3
- P3-D65 Gamma 2.6
- Rec.709 Gamma 2.4
- Adobe RGB
- Rec.2020 Linear
- ProPhoto RGB Linear
- ACES AP0 Linear
- ACEScg AP1 Linear

ACES AP0 / ACEScg are no longer special-cased as profile-less exports. Their
pixel transforms still use explicit matrices, but those matrices now come from
the same central colourspace definitions as the rest of the export path, and
their TIFF-family outputs carry generated ICC profiles.

## Validation

`trichrome_smoke` checks selected matrices against values generated from
`colour.matrix_RGB_to_RGB(..., chromatic_adaptation_transform="Bradford")` in
colour-science 0.4.7. `trichrome_desktop_core` checks that every export target
can generate an ICC profile, that Qt accepts those profiles, and that a 16-bit
ACEScg TIFF round-trips with an embedded profile.

## Remaining Boundary

This page covers export color encoding and metadata. It does not claim the RAW
development side is fully calibrated: DCP/profile lookup, camera-specific
matrices, white-balance policy, and RawTherapee-informed fixture coverage remain
separate RAW correctness work.
