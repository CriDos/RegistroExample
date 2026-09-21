param(
    [string]$Version = "",
    [string]$QtPrefix = "",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$repo = Split-Path $PSScriptRoot -Parent
$runVs = Join-Path $PSScriptRoot "run-vs.cmd"

if (-not $Version) {
    $cmakeSrc = Get-Content (Join-Path $repo "CMakeLists.txt") -Raw
    if ($cmakeSrc -notmatch 'VERSION\s+(\d+\.\d+\.\d+)') {
        throw "Cannot parse project version from CMakeLists.txt - pass -Version explicitly"
    }
    $Version = $Matches[1]
}

if (-not $QtPrefix) { $QtPrefix = $env:QT_ROOT_DIR }
if (-not $QtPrefix) { $QtPrefix = $env:CMAKE_PREFIX_PATH }
if (-not $QtPrefix) { throw "Qt kit dir required: pass -QtPrefix, set QT_ROOT_DIR or CMAKE_PREFIX_PATH" }
$windeployqt = Join-Path $QtPrefix "bin\windeployqt.exe"
if (-not (Test-Path $windeployqt)) { throw "windeployqt not found: $windeployqt" }
$env:PATH = "$QtPrefix\bin;$env:PATH"

if (-not $SkipBuild) {
    if (-not (Test-Path (Join-Path $repo "build-release\build.ninja"))) {
        Write-Host "== configure (release preset) =="
        & $runVs cmake --preset release
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    Write-Host "== build (release preset) =="
    & $runVs cmake --build --preset release
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$serverExe = Join-Path $repo "build-release\RegistroServer.exe"
$clientExe = Join-Path $repo "build-release\RegistroClient.exe"
foreach ($e in @($serverExe, $clientExe)) {
    if (-not (Test-Path $e)) { throw "Missing build artifact: $e" }
}

# The project builds MSVC x64 only, on all supported Windows versions.
$distName = "RegistroExample-$Version-win64"
$stagingRoot = Join-Path $repo "dist\staging"
$staging = Join-Path $stagingRoot $distName
$serverDir = Join-Path $staging "server"
$clientDir = Join-Path $staging "client"
Remove-Item -Recurse -Force $stagingRoot -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $serverDir, $clientDir -Force | Out-Null

Write-Host "== deploy server =="
Copy-Item $serverExe $serverDir
Push-Location $serverDir
try {
    & $windeployqt --release --no-translations --compiler-runtime RegistroServer.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} finally {
    Pop-Location
}

Write-Host "== deploy client (QML) =="
Copy-Item $clientExe $clientDir
Push-Location $clientDir
try {
    & $windeployqt --release --no-translations --compiler-runtime `
        --qmldir (Join-Path $repo "src\client\qml") RegistroClient.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} finally {
    Pop-Location
}

Write-Host "== launchers + readme =="
Set-Content (Join-Path $staging "start-server.cmd") @"
@echo off
cd /d %~dp0server
RegistroServer.exe --port 9080
pause
"@ -Encoding Ascii
Set-Content (Join-Path $staging "start-client.cmd") @"
@echo off
cd /d %~dp0client
RegistroClient.exe http://127.0.0.1:9080
pause
"@ -Encoding Ascii
Set-Content (Join-Path $staging "README.txt") @"
RegistroExample $Version - client-server client catalog.

Run:
1. start-server.cmd - server on http://127.0.0.1:9080.
   Demo mode (default): separate database registro-demo.db with the
   demo/demo account (admin) and 10,000 seeded clients - the client login
   form is prefilled automatically.
   Plain server: RegistroServer.exe --no-demo --admin-user admin
   --admin-password <min 8 chars> (after that the API requires auth).
2. start-client.cmd - QML client (connects to 127.0.0.1:9080 by default;
   pass another address as an argument).

Config: on first start both apps create their default config files
(registro-server.ini next to the server, config.ini in the user's
AppConfigLocation). Missing files are recreated with defaults; existing
files are read as-is (unreadable or garbled content keeps built-in defaults).

Build: release, MSVC 2022, Qt $QtPrefix
"@ -Encoding Ascii

Write-Host "== archive =="
$zip = Join-Path $repo "dist\$distName.zip"
Remove-Item -Force $zip -ErrorAction SilentlyContinue
Compress-Archive -Path $staging -DestinationPath $zip
Remove-Item -Recurse -Force $stagingRoot
$size = [math]::Round((Get-Item $zip).Length / 1MB, 1)
Write-Host "OK: $zip ($size MB)"