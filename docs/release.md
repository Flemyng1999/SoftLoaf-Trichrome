# Release And Packaging

SoftLoaf Trichrome is intended to ship as signed desktop binaries, with source
available under GPLv3.

## macOS

Recommended distribution:

- GitHub release asset: notarized `.dmg` or `.zip`
- Signing: Apple Developer ID Application certificate
- Notarization: Apple notary service
- Deployment helper: `macdeployqt`

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

## Windows

Recommended first distribution:

- Microsoft Store / Partner Center package
- Package format: MSIX
- Deployment helper: `windeployqt`

This repository does not yet contain a working Windows dependency bootstrap.
Before enabling Windows CI, add a stable dependency route such as vcpkg or a
documented Qt/OpenCV/LibRaw binary setup, then verify that RAW decoding and Qt
deployment work on a clean Windows runner.

If distributing outside the Microsoft Store, obtain a normal code-signing
certificate and sign the installer or MSIX package independently.

## Test Data

Do not publish raw.pixls.us fixture files in the repository or release assets
unless their terms explicitly allow redistribution. Keep fixture manifests and
checksums in the repository, and cache downloaded RAW files locally under:

```text
/Users/flemyng/Desktop/Film/raw.pixls.us/
```
