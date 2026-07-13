# Export tests on external volumes and NAS

`export_integration_test` always exercises the local system volume. Two optional
targets run the same writer against real removable and network filesystems:

- `SOFTLOAF_EXPORT_EXTERNAL_TARGET`: a mounted external SSD/HDD root.
- `SOFTLOAF_EXPORT_NAS_TARGET`: a mounted SMB/NAS share or Windows UNC root.

The test creates only one isolated directory named
`SoftLoaf导出测试-<random UUID>` below each configured root. It exercises Chinese
and mixed-script paths, deep paths, overwrite/truncation, eight concurrent
exports, a multi-megabyte 16-bit file, immediate read-after-write, rename,
delete, and recursive cleanup. Never point either variable at a directory where
the test account must not create and delete files.

## macOS

Mount both volumes in Finder first, then run:

```bash
SOFTLOAF_EXPORT_EXTERNAL_TARGET="/Volumes/外接测试盘" \
SOFTLOAF_EXPORT_NAS_TARGET="/Volumes/摄影资料 NAS" \
ctest --test-dir build -R '^export_integration$' --output-on-failure
```

## Windows PowerShell

Map/authenticate the share first. A UNC path is preferred because it tests the
same wide-character path boundary used by the application:

```powershell
$env:SOFTLOAF_EXPORT_EXTERNAL_TARGET = 'E:\外接测试盘'
$env:SOFTLOAF_EXPORT_NAS_TARGET = '\\nas-host\摄影资料'
$env:PATH = "C:\Qt\6.8.3\msvc2022_64\bin;$env:PATH"
ctest --test-dir build-windows -R '^export_integration$' --output-on-failure
```

Ordinary CI has no physical external disk or authenticated NAS, so those two
cases report `SKIP` there. CI still runs the complete local-volume suite on both
macOS and Windows. Disconnect-during-write and disk-full tests require a
controlled disposable share/volume and are intentionally not destructive CI
tests.
