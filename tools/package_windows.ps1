param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir,

    [Parameter(Mandatory = $true)]
    [string]$InstallRoot,

    [string]$Version = "0.1.0-alpha.1",
    [string]$OutDir = "",
    [string]$Configuration = "Release",
    [string]$AppName = "SoftLoaf Trichrome",
    [string]$ExeName = "softloaf_trichrome_app.exe",
    [string]$SignCertPath = "",
    [string]$SignCertPassword = "",
    [string]$TimestampUrl = "http://timestamp.digicert.com"
)

$ErrorActionPreference = "Stop"

function Resolve-PathStrict([string]$PathValue) {
    return (Resolve-Path -LiteralPath $PathValue).Path
}

function Copy-IfExists([string]$Source, [string]$Destination) {
    if (Test-Path -LiteralPath $Source) {
        Copy-Item -LiteralPath $Source -Destination $Destination -Force
    }
}

function Invoke-CodeSign([string]$PathValue) {
    if ([string]::IsNullOrWhiteSpace($SignCertPath)) {
        return
    }

    $signtool = Get-Command signtool.exe -ErrorAction SilentlyContinue
    if (-not $signtool) {
        $kits = @(
            "${env:ProgramFiles(x86)}\Windows Kits\10\bin",
            "${env:ProgramFiles}\Windows Kits\10\bin"
        )
        foreach ($kit in $kits) {
            if (-not (Test-Path -LiteralPath $kit)) {
                continue
            }
            $candidate = Get-ChildItem -LiteralPath $kit -Recurse -Filter signtool.exe -ErrorAction SilentlyContinue |
                Where-Object { $_.FullName -match "\\x64\\signtool\.exe$" } |
                Sort-Object FullName -Descending |
                Select-Object -First 1
            if ($candidate) {
                $signtool = $candidate
                break
            }
        }
    }
    if (-not $signtool) {
        throw "signtool.exe not found"
    }

    $args = @(
        "sign",
        "/fd", "SHA256",
        "/tr", $TimestampUrl,
        "/td", "SHA256",
        "/f", $SignCertPath
    )
    if (-not [string]::IsNullOrWhiteSpace($SignCertPassword)) {
        $args += @("/p", $SignCertPassword)
    }
    $args += $PathValue

    & $signtool @args
    if ($LASTEXITCODE -ne 0) {
        throw "signtool failed for $PathValue"
    }
}

$RepoRoot = Resolve-PathStrict (Join-Path $PSScriptRoot "..")
$BuildDir = Resolve-PathStrict $BuildDir
$InstallRoot = Resolve-PathStrict $InstallRoot
if ([string]::IsNullOrWhiteSpace($OutDir)) {
    $OutDir = Join-Path $BuildDir "windows_dist"
}
$OutDir = [System.IO.Path]::GetFullPath($OutDir)
$StageDir = Join-Path $OutDir "stage"
$PortableDir = Join-Path $StageDir $AppName
$InstallerScript = Join-Path $RepoRoot "tools\windows_installer.nsi"
$QmlDir = Join-Path $RepoRoot "qml"
$TripletBin = Join-Path $InstallRoot "bin"
$QtToolsDir = Join-Path $InstallRoot "tools\Qt6\bin"
$WindeployqtCandidates = @()
if (-not [string]::IsNullOrWhiteSpace($env:QT_ROOT_DIR)) {
    $WindeployqtCandidates += Join-Path $env:QT_ROOT_DIR "bin\windeployqt.exe"
}
$WindeployqtCandidates += Join-Path $QtToolsDir "windeployqt.exe"
$WindeployqtCommand = Get-Command windeployqt.exe -ErrorAction SilentlyContinue
if ($WindeployqtCommand) {
    $WindeployqtCandidates += $WindeployqtCommand.Source
}
$Windeployqt = $WindeployqtCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
$Makensis = (Get-Command makensis.exe -ErrorAction SilentlyContinue)

if (-not $Windeployqt) {
    throw "windeployqt.exe not found. Checked: $($WindeployqtCandidates -join ', ')"
}
if (-not $Makensis) {
    throw "makensis.exe not found. Install NSIS first."
}

$ExeCandidates = @(
    Join-Path $BuildDir "$Configuration\$ExeName",
    Join-Path $BuildDir $ExeName
)
$BuiltExe = $ExeCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $BuiltExe) {
    throw "Built executable not found. Checked: $($ExeCandidates -join ', ')"
}

Remove-Item -LiteralPath $OutDir -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $PortableDir | Out-Null

Copy-Item -LiteralPath $BuiltExe -Destination (Join-Path $PortableDir "SoftLoaf Trichrome.exe") -Force
Copy-IfExists (Join-Path $RepoRoot "LICENSE") $PortableDir
Copy-IfExists (Join-Path $RepoRoot "COMMERCIAL_LICENSE.md") $PortableDir
Copy-IfExists (Join-Path $RepoRoot "README.md") $PortableDir

if (Test-Path -LiteralPath $TripletBin) {
    Copy-Item -Path (Join-Path $TripletBin "*.dll") -Destination $PortableDir -Force -ErrorAction SilentlyContinue
}

& $Windeployqt (Join-Path $PortableDir "SoftLoaf Trichrome.exe") `
    --release `
    --qmldir $QmlDir `
    --compiler-runtime
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed"
}

Get-ChildItem -LiteralPath $PortableDir -Recurse -Include *.exe,*.dll |
    ForEach-Object { Invoke-CodeSign $_.FullName }

$PortableZip = Join-Path $OutDir "SoftLoaf-Trichrome-$Version-Windows-x64-portable.zip"
$InstallerExe = Join-Path $OutDir "SoftLoaf-Trichrome-$Version-Windows-x64-Setup.exe"
$VersionNumbers = @([regex]::Matches($Version, "\d+") | ForEach-Object { $_.Value })
while ($VersionNumbers.Count -lt 4) {
    $VersionNumbers += "0"
}
$VersionQuad = ($VersionNumbers | Select-Object -First 4) -join "."

Compress-Archive -LiteralPath $PortableDir -DestinationPath $PortableZip -Force

& $Makensis.Source `
    "/DAPP_VERSION=$Version" `
    "/DAPP_VERSION_QUAD=$VersionQuad" `
    "/DAPP_STAGE=$PortableDir" `
    "/DAPP_OUT=$InstallerExe" `
    "/DAPP_ICON=$(Join-Path $RepoRoot "resources\AppIcon.ico")" `
    $InstallerScript
if ($LASTEXITCODE -ne 0) {
    throw "makensis failed"
}

Invoke-CodeSign $InstallerExe

Push-Location $OutDir
try {
    $HashInputs = @((Split-Path -Leaf $InstallerExe), (Split-Path -Leaf $PortableZip))
    Get-FileHash -Path $HashInputs -Algorithm SHA256 |
        ForEach-Object { "$($_.Hash.ToLowerInvariant())  $($_.Path | Split-Path -Leaf)" } |
        Set-Content -Encoding ascii "SoftLoaf-Trichrome-$Version-Windows-x64.sha256"
}
finally {
    Pop-Location
}

Write-Host "package-windows: $InstallerExe"
Write-Host "package-windows: $PortableZip"
Write-Host "package-windows: $(Join-Path $OutDir "SoftLoaf-Trichrome-$Version-Windows-x64.sha256")"
