# Windows 11 303mini Test Environment

Use this note when testing SoftLoaf Trichrome on the Windows 11 machine
`303mini`. Do not rediscover the toolchain from scratch unless one of the paths
below fails.

## Host

- Windows SSH host: `cau-303mini`
- WSL SSH host: `cau-303mini-wsl`
- Default Windows SSH shell: `cmd.exe`
- Default Windows user directory: `C:\Users\flemyng.303mini`
- Preferred scratch checkout for Codex tests:
  `C:\Users\flemyng.303mini\Desktop\SoftLoaf-Trichrome-codex-test`
- Existing dependency/cache root:
  `C:\Users\flemyng.303mini\Desktop\softloaf-trichrome-check`

The WSL host is useful for basic Linux shell checks, but the Trichrome Windows
build should be run through the Windows host because the configured Qt kit is
MSVC.

## Installed Toolchain

- Visual Studio Build Tools 2022:
  `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools`
- MSVC developer environment:
  `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat`
- Visual Studio bundled CMake:
  `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`
- Visual Studio bundled Ninja:
  `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe`
- Qt:
  `C:\Qt\6.8.3\msvc2022_64`
- vcpkg toolchain:
  `C:\Users\flemyng.303mini\Desktop\softloaf-trichrome-check\.vcpkg\scripts\buildsystems\vcpkg.cmake`
- Reusable installed dependencies:
  `C:\Users\flemyng.303mini\Desktop\softloaf-trichrome-check\build-windows\vcpkg_installed`

## Recommended Workflow

Create or update a disposable checkout under the scratch path, apply the local
patch being tested, and configure against the existing dependency tree.

Example clone:

```bat
git clone https://github.com/Flemyng1999/SoftLoaf-Trichrome.git C:\Users\flemyng.303mini\Desktop\SoftLoaf-Trichrome-codex-test
```

Example local patch transfer from macOS:

```bash
git diff --output=/private/tmp/softloaf-trichrome-303mini.patch -- include/softloaf_trichrome/color_management.hpp src/trichrome_controller.cpp
scp /private/tmp/softloaf-trichrome-303mini.patch cau-303mini:C:/Users/flemyng.303mini/Desktop/softloaf-trichrome-303mini.patch
ssh cau-303mini "git -C C:\Users\flemyng.303mini\Desktop\SoftLoaf-Trichrome-codex-test apply C:\Users\flemyng.303mini\Desktop\softloaf-trichrome-303mini.patch"
```

Configure with manifest mode disabled. This is important: otherwise vcpkg sees
the repository manifest and may try to install or build a large dependency set,
including Qt.

```bat
set "SRC=C:\Users\flemyng.303mini\Desktop\SoftLoaf-Trichrome-codex-test"
set "BUILD=%SRC%\build-windows"
set "CACHE=C:\Users\flemyng.303mini\Desktop\softloaf-trichrome-check"
set "QT=C:\Qt\6.8.3\msvc2022_64"
set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set "CMAKE=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

call "%VCVARS%"
"%CMAKE%" -S "%SRC%" -B "%BUILD%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QT%;%CACHE%\build-windows\vcpkg_installed\x64-windows" -DCMAKE_TOOLCHAIN_FILE="%CACHE%\.vcpkg\scripts\buildsystems\vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows -DVCPKG_MANIFEST_MODE=OFF -DVCPKG_INSTALLED_DIR="%CACHE%\build-windows\vcpkg_installed"
```

Build the desktop core test:

```bat
set "BUILD=C:\Users\flemyng.303mini\Desktop\SoftLoaf-Trichrome-codex-test\build-windows"
set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set "CMAKE=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

call "%VCVARS%"
"%CMAKE%" --build "%BUILD%" --target trichrome_desktop_core_test --config Release
```

Build the full app when changes touch `src/trichrome_controller.cpp`, QML,
platform code, or Windows compile boundaries:

```bat
set "BUILD=C:\Users\flemyng.303mini\Desktop\SoftLoaf-Trichrome-codex-test\build-windows"
set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set "CMAKE=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

call "%VCVARS%"
"%CMAKE%" --build "%BUILD%" --target softloaf_trichrome_app --config Release
```

Build all CTest executables:

```bat
set "BUILD=C:\Users\flemyng.303mini\Desktop\SoftLoaf-Trichrome-codex-test\build-windows"
set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set "CMAKE=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

call "%VCVARS%"
"%CMAKE%" --build "%BUILD%" --target trichrome_smoke trichrome_desktop_core_test raw_fixture_regression_test --config Release
```

Run CTest:

```bat
set "BUILD=C:\Users\flemyng.303mini\Desktop\SoftLoaf-Trichrome-codex-test\build-windows"
set "QT=C:\Qt\6.8.3\msvc2022_64"
set "DEPS=C:\Users\flemyng.303mini\Desktop\softloaf-trichrome-check\build-windows\vcpkg_installed\x64-windows"
set "CTEST=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"

set "PATH=%QT%\bin;%DEPS%\bin;%PATH%"
cd /d "%BUILD%"
"%CTEST%" --output-on-failure -C Release
```

Expected successful CTest summary:

```text
100% tests passed, 0 tests failed out of 3
```

## Known Pitfalls

- The Windows SSH host defaults to `cmd.exe`; Unix commands like `pwd` and
  `uname` will fail there. Use PowerShell explicitly for inspection commands.
- PowerShell pipes may be interpreted by the remote `cmd.exe` if quoting is not
  tight. Prefer simple `powershell.exe -NoProfile -Command "..."` commands.
- Do not let CMake enter vcpkg manifest mode for quick validation. Use
  `-DVCPKG_MANIFEST_MODE=OFF` and point `-DVCPKG_INSTALLED_DIR` at the existing
  installed tree.
- If a cancelled vcpkg configure leaves background processes, check and stop
  them before retrying:

```bat
tasklist /FI "IMAGENAME eq vcpkg.exe"
tasklist /FI "IMAGENAME eq cmake.exe"
tasklist /FI "IMAGENAME eq cl.exe"
taskkill /F /IM vcpkg.exe /T
```

Only terminate processes that were started by the current test run.

## Current Export Encode Status

Windows export encode still falls back to CPU. The D3D12 backend file exists,
but `MakeD3D12ExportEncodeBackend()` currently returns `nullptr` in
`src/export_encode_backend_d3d12.cpp`. A full Windows GPU encode implementation
still needs an HLSL/D3D12 path equivalent to the Metal encode backend.
