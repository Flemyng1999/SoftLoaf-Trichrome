# Release And Packaging

SoftLoaf Trichrome is intended to ship as signed desktop binaries, with source
available under GPLv3.

Initial public version:

```text
v0.1.0-alpha.1
```

The CMake project version remains `0.1.0`; the `alpha.1` suffix belongs to the
Git tag, GitHub Release title, DMG filename, and release notes.

The app icon currently reuses the SoftLoaf identity assets from SoftLoaf
Negative:

- `resources/AppIcon.icns`
- `resources/AppIcon.ico`
- `docs/logo/logo.svg`

## macOS

Recommended distribution:

- GitHub release asset: notarized `.dmg` or `.zip`
- Signing: Apple Developer ID Application certificate
- Notarization: Apple notary service
- Deployment helper: `macdeployqt`

Local unsigned smoke package:

```bash
SOFTLOAF_CODESIGN_IDENTITY="-" \
SOFTLOAF_TRICHROME_VERSION="0.1.0-alpha.1" \
  tools/package_macos_dmg.sh build
```

Signed package:

```bash
SOFTLOAF_CODESIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)" \
SOFTLOAF_TRICHROME_VERSION="0.1.0-alpha.1" \
  tools/package_macos_dmg.sh build
```

Notarization:

```bash
xcrun notarytool store-credentials softloaf-notary \
  --apple-id "you@example.com" \
  --team-id "TEAMID" \
  --password "app-specific-password"

SOFTLOAF_NOTARY_KEYCHAIN_PROFILE=softloaf-notary \
  tools/notarize_macos_dmg.sh build/SoftLoaf-Trichrome-0.1.0-alpha.1-macOS.dmg
```

Suggested GitHub Actions secrets for a future packaging workflow:

- `APPLE_DEVELOPER_ID_APPLICATION`
- `APPLE_ID`
- `APPLE_TEAM_ID`
- `APPLE_APP_SPECIFIC_PASSWORD`
- `MACOS_CERTIFICATE_P12`
- `MACOS_CERTIFICATE_PASSWORD`

The first automated macOS release workflow should:

1. install Qt, OpenCV, LibRaw, and Ninja;
2. configure and build Release;
3. run CTest;
4. run `macdeployqt`;
5. sign nested frameworks and the app bundle;
6. create a DMG;
7. notarize and staple;
8. upload the artifact to a GitHub release.

## Windows EXE

The most convenient user-facing Windows package is a zip or installer containing
the `.exe` plus Qt/OpenCV/LibRaw runtime files. That is simpler than MSIX for
early testers, but it has a signing tradeoff.

- Unsigned `.exe`: easiest, but Windows SmartScreen will warn heavily.
- OV Authenticode certificate: common independent-developer route; cheaper than
  EV, but reputation builds slowly per certificate and publisher.
- EV certificate / hardware token: stronger initial SmartScreen reputation, but
  expensive and operationally annoying for a small personal project.
- Cloud signing, such as Azure Trusted Signing: good long-term CI shape if the
  account and identity requirements are practical for you.
- Microsoft Store: useful for Store distribution, but it does not solve signing
  for a standalone `.exe` downloaded from GitHub.

Recommended path:

1. For `v0.1.0-alpha.1`, publish macOS only, or publish Windows as an explicit
   unsigned tester zip.
2. Before wider Windows release, buy an OV code-signing certificate or set up a
   cloud signing provider.
3. Sign the app `.exe`, DLLs you ship if appropriate, and the installer/zip
   wrapper if using one. Always timestamp signatures.

Future Windows package script should:

1. configure and build Release with Visual Studio;
2. run CTest;
3. run `windeployqt`;
4. copy OpenCV/LibRaw runtime DLLs;
5. sign binaries with `signtool` or a cloud signer;
6. create a zip or NSIS/Inno Setup installer;
7. sign the installer;
8. emit SHA256 checksums.

## Windows Store / MSIX

Recommended Store package:

- Microsoft Store / Partner Center upload
- Package format: MSIX
- Deployment helper: `windeployqt`

This repository does not yet contain a working Windows dependency bootstrap.
Before enabling Windows CI, add a stable dependency route such as vcpkg or a
documented Qt/OpenCV/LibRaw binary setup, then verify that RAW decoding and Qt
deployment work on a clean Windows runner.

## Test Data

Do not publish raw.pixls.us fixture files in the repository or release assets
unless their terms explicitly allow redistribution. Keep fixture manifests and
checksums in the repository, and cache downloaded RAW files locally under:

```text
/Users/flemyng/Desktop/Film/raw.pixls.us/
```
