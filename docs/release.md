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

Supported release platforms:

- macOS 15.0 or newer
- Windows 10 and Windows 11

## macOS

Recommended distribution:

- GitHub release asset: notarized `.dmg` or `.zip`
- Signing: Apple Developer ID Application certificate
- Notarization: Apple notary service
- Deployment helper: `macdeployqt`
- Build deployment target: `CMAKE_OSX_DEPLOYMENT_TARGET=15.0`
- CI runner for release builds: `macos-15`

Compatibility requirement: every Mach-O file inside the final app bundle must
have `LC_BUILD_VERSION minos <= 15.0`. The package script runs
`tools/check_macos_compatibility.sh` after deployment and signing.

Do not use dependency binaries built with a newer minimum OS for release
artifacts. Homebrew packages installed on a newer macOS can be built with a
newer `minos` than this app supports; those builds are acceptable for local
development only, not for the public macOS 15+ release. Use a dependency build
route that produces Qt/OpenCV/LibRaw binaries targeting macOS 15.0.

The formal macOS release line should build on a macOS 15 environment. GitHub
Actions workflows must pin `runs-on: macos-15`; do not use `macos-latest` for
release builds.

Recommended local release-shaped build with vcpkg:

```bash
git pull
rm -rf build-release
git clone https://github.com/microsoft/vcpkg.git .vcpkg
git -C .vcpkg checkout a0400024711b283056538ac19ced80b91a83c24c
.vcpkg/bootstrap-vcpkg.sh -disableMetrics

cmake -S . -B build-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/.vcpkg/scripts/buildsystems/vcpkg.cmake"

cmake --build build-release
ctest --test-dir build-release --output-on-failure
```

Local unsigned smoke package, for development only:

```bash
SOFTLOAF_CODESIGN_IDENTITY="-" \
SOFTLOAF_TRICHROME_VERSION="0.1.0-alpha.1" \
  tools/package_macos_dmg.sh build-release
```

Signed package:

```bash
SOFTLOAF_CODESIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)" \
SOFTLOAF_TRICHROME_VERSION="0.1.0-alpha.1" \
  tools/package_macos_dmg.sh build-release
```

The package script automatically runs:

```bash
tools/check_macos_compatibility.sh build-release/softloaf_trichrome_app.app
```

If any Qt/OpenCV/LibRaw binary reports `minos > 15.0`, packaging fails. That
means the dependency set is not suitable for the public macOS 15+ release.

Notarization:

```bash
xcrun notarytool store-credentials softloaf-notary \
  --apple-id "you@example.com" \
  --team-id "TEAMID" \
  --password "app-specific-password"

SOFTLOAF_NOTARY_KEYCHAIN_PROFILE=softloaf-notary \
  tools/notarize_macos_dmg.sh build-release/SoftLoaf-Trichrome-0.1.0-alpha.1-macOS.dmg
```

Apple's notarization guide:
<https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution>

Suggested GitHub Actions secrets for a future packaging workflow:

- `APPLE_DEVELOPER_ID_APPLICATION`
- `APPLE_ID`
- `APPLE_TEAM_ID`
- `APPLE_APP_SPECIFIC_PASSWORD`
- `MACOS_CERTIFICATE_P12`
- `MACOS_CERTIFICATE_PASSWORD`

The first automated macOS release workflow should:

1. install or restore Qt, OpenCV, LibRaw, and Ninja builds targeting macOS 15.0;
2. configure and build Release with `CMAKE_OSX_DEPLOYMENT_TARGET=15.0`;
3. run CTest;
4. run `macdeployqt`;
5. run `tools/check_macos_compatibility.sh`;
6. sign nested frameworks and the app bundle;
7. create a DMG;
8. notarize and staple;
9. upload the artifact to a GitHub release.

The manual release workflow is `.github/workflows/release-macos.yml`. It runs
on `macos-15`, bootstraps vcpkg at the baseline recorded in `vcpkg.json`,
restores a GitHub Actions vcpkg binary cache, requires signing/notarization
secrets, uploads the notarized DMG as a workflow artifact, and can optionally
create a draft GitHub Release for an existing tag.

`MACOS_CERTIFICATE_P12` must contain the base64-encoded Developer ID
Application `.p12` export. `APPLE_DEVELOPER_ID_APPLICATION` should be the exact
codesigning identity, for example
`Developer ID Application: Your Name (TEAMID)`.

For notarization, configure either Apple ID credentials:

- `APPLE_ID`
- `APPLE_TEAM_ID`
- `APPLE_APP_SPECIFIC_PASSWORD`

or App Store Connect API key credentials:

- `APPLE_API_KEY_ID`
- `APPLE_API_KEY`
- `APPLE_API_ISSUER_ID` for Team API Keys only

## GitHub Release

Release notes live under `docs/release_notes/`.

Use Clash Verge Rev v2.5.1 as the public release page benchmark:
<https://github.com/clash-verge-rev/clash-verge-rev/releases/tag/v2.5.1>

The benchmark shape is:

- concise version heading and changelog;
- explicit platform download section in the release body;
- architecture/runtime variants named in the asset filename;
- detached signature files when signing is configured;
- a machine-readable update manifest such as `latest.json` when auto-update is
  implemented;
- all assets uploaded to a draft before publishing.

SoftLoaf does not need to mirror every Clash Verge platform immediately. For
alpha releases, match the structure while only publishing supported artifacts.
The required public asset naming pattern is:

```text
SoftLoaf.Trichrome_<version>_x64.dmg
SoftLoaf.Trichrome_<version>_x64-setup.exe
SoftLoaf.Trichrome_<version>_x64-portable.zip
SoftLoaf.Trichrome_<version>_x64.sha256
```

When Authenticode signing is configured, add detached signature assets next to
Windows downloads:

```text
SoftLoaf.Trichrome_<version>_x64-setup.exe.sig
SoftLoaf.Trichrome_<version>_x64-portable.zip.sig
```

When Sparkle, WinSparkle, or another updater exists, add:

```text
latest.json
```

The current workflow names are still accepted for `v0.1.0-alpha.1`, but new
release automation should migrate to the dotted product prefix and underscore
version separator above so the asset list reads like the benchmark release.

Every release note should follow this order:

1. short alpha/stable warning;
2. `Downloads` grouped by platform and architecture;
3. `Changes`;
4. `Known Limitations`;
5. `Verification` with SHA256 and signing state;
6. `Packaging Notes` only when the release has unusual constraints.

```bash
git add docs/release_notes/v0.1.0-alpha.1.md
git commit -m "Add v0.1.0-alpha.1 release notes"
git tag -a v0.1.0-alpha.1 -m "SoftLoaf Trichrome v0.1.0-alpha.1"
git push origin main
git push origin v0.1.0-alpha.1
```

Create the release as a draft first:

```bash
gh release create v0.1.0-alpha.1 \
  build-release/SoftLoaf-Trichrome-0.1.0-alpha.1-macOS.dmg \
  build-release/SoftLoaf-Trichrome-0.1.0-alpha.1-macOS.dmg.sha256 \
  --title "SoftLoaf Trichrome v0.1.0-alpha.1" \
  --notes-file docs/release_notes/v0.1.0-alpha.1.md \
  --draft
```

Inspect the draft in GitHub before publishing it.

Release publication checklist:

1. create or verify the annotated tag, for example `v0.1.0-alpha.1`;
2. create the GitHub Release as `--draft --prerelease`;
3. run the macOS release workflow with draft creation enabled, or upload to the
   existing draft;
4. run the Windows release workflow with `upload_to_release=true`;
5. update the draft notes from `docs/release_notes/<tag>.md` after all assets
   are uploaded;
6. verify the asset list, checksums, draft flag, and prerelease flag with:

```bash
gh release view v0.1.0-alpha.1 \
  --json tagName,name,url,isDraft,isPrerelease,assets
```

Do not publish the GitHub Release until both platform workflows have completed
successfully and the release notes describe the exact platform support and
signing state.

## Windows EXE

The most convenient user-facing Windows package is a zip or installer containing
the `.exe` plus Qt/OpenCV/LibRaw runtime files. That is simpler than MSIX for
early testers, but it has a signing tradeoff.

Supported Windows versions are Windows 10 and Windows 11. CMake defines
`WINVER`, `_WIN32_WINNT`, and `NTDDI_VERSION` for the Windows 10 API family.

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

1. For `v0.1.0-alpha.1`, publish Windows only as explicit unsigned tester
   artifacts.
2. Before wider Windows release, buy an OV code-signing certificate or set up a
   cloud signing provider.
3. Sign the app `.exe`, DLLs you ship if appropriate, and the installer/zip
   wrapper if using one. Always timestamp signatures.

Windows packaging is handled by `.github/workflows/release-windows.yml` and
`tools/package_windows.ps1`. The workflow:

1. runs on `windows-2022`;
2. installs pinned prebuilt Qt with `jurplel/install-qt-action`;
3. installs OpenCV/LibRaw with the Windows-only vcpkg manifest at
   `.github/vcpkg/windows/vcpkg.json`;
4. configures and builds Release with Ninja and MSVC;
5. runs CTest;
6. runs `windeployqt`;
7. copies OpenCV/LibRaw runtime DLLs;
8. optionally signs binaries with `signtool` when `WINDOWS_CODESIGN_PFX` and
   `WINDOWS_CODESIGN_PASSWORD` are configured;
9. creates a portable zip and NSIS installer;
10. optionally signs the installer;
11. emits SHA256 checksums;
12. optionally uploads the assets to the existing GitHub release draft.

Run Windows packaging from GitHub Actions, not a local developer shell:

```bash
gh workflow run "Release Windows" \
  -f version=0.1.0-alpha.1 \
  -f upload_to_release=true
```

Expected Windows release assets:

```text
SoftLoaf-Trichrome-<version>-Windows-x64-Setup.exe
SoftLoaf-Trichrome-<version>-Windows-x64-portable.zip
SoftLoaf-Trichrome-<version>-Windows-x64.sha256
```

After the run completes, verify both the workflow and the release draft:

```bash
gh run list --workflow "Release Windows" --limit 5
gh release view v0.1.0-alpha.1 --json isDraft,isPrerelease,assets
```

For `v0.1.0-alpha.1`, the successful Windows run was `28321855649`. The
important fixed decisions from that run are:

- use prebuilt Qt from `jurplel/install-qt-action`; do not build Qt through
  vcpkg for release packaging;
- use `.github/vcpkg/windows/vcpkg.json` for release dependencies; do not use
  the repository root `vcpkg.json` for the Windows release workflow;
- keep OpenCV default features disabled in the Windows release manifest;
- keep CMake in `VCPKG_MANIFEST_MODE=OFF` after the dependency install so it
  consumes the already-installed vcpkg tree and the prebuilt Qt prefix;
- set `VCPKG_ROOT` back to `SOFTLOAF_VCPKG_ROOT` after `ilammy/msvc-dev-cmd`;
  that action can expose Visual Studio's bundled vcpkg path;
- keep `windeployqt` discovery flexible: first `QT_ROOT_DIR`, then vcpkg
  `tools\Qt6\bin`, then `PATH`;
- write PowerShell `Join-Path` calls in scripts with explicit `-Path` and
  `-ChildPath` arguments when building arrays of paths;
- make Windows-specific C++ code compile on MSVC; POSIX-only headers such as
  `dlfcn.h` must be behind platform guards.

Known pitfalls from the first Windows packaging pass:

- `macos-latest` and local Homebrew builds are not a release environment for
  macOS 15+ artifacts.
- `macos-15` and `windows-2022` are intentionally pinned; do not replace them
  with `*-latest` in release workflows.
- vcpkg automatically enters manifest mode when it sees the root `vcpkg.json`;
  passing individual packages in that mode fails. Use a dedicated manifest or
  `--classic` deliberately.
- OpenCV default features pull in large modules such as DNN, GAPI, highgui, and
  platform UI/video backends. The app only requires `core`, `imgproc`, and
  `imgcodecs`; the release manifest must stay trimmed.
- GitHub `actions/cache` saves at job success. If a job fails after vcpkg
  installation, the cache may not be saved. A later successful run is the
  baseline cache warmer.
- An unsigned Windows alpha installer is expected to trigger SmartScreen.
  Do not describe it as a trusted general-public installer until Authenticode
  signing is configured.

Microsoft SignTool requires explicit digest parameters on modern signing
commands:

```powershell
signtool sign /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 /a path\to\SoftLoaf-Trichrome.exe
signtool verify /pa /v path\to\SoftLoaf-Trichrome.exe
```

SignTool reference:
<https://learn.microsoft.com/en-us/windows/win32/seccrypto/signtool>

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
