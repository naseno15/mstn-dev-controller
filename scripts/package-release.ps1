# Build static Release and zip portable folder for GitHub Releases.
param(
  [string]$Version = "1.0.0",
  [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root "build"
$stage = Join-Path $root "out\mstnDevController-$Version-win-x64"
$zip = Join-Path $root "out\mstnDevController-$Version-win-x64.zip"

$cmake = @(
  "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
  "C:\Program Files\CMake\bin\cmake.exe",
  "cmake"
) | Where-Object { $_ -eq "cmake" -or (Test-Path $_) } | Select-Object -First 1

Write-Host "== configure (static CRT + WebView2LoaderStatic) =="
& $cmake -S $root -B $build -G "Visual Studio 17 2022" -A x64 -DMSTN_STATIC=ON
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "== build $Config =="
& $cmake --build $build --config $Config --target mstnDevController
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$exeDir = Join-Path $build $Config
$exe = Join-Path $exeDir "mstnDevController.exe"
if (-not (Test-Path $exe)) { throw "Missing $exe" }

# Drop leftover dynamic loader from older builds
$legacyDll = Join-Path $exeDir "WebView2Loader.dll"
if (Test-Path $legacyDll) { Remove-Item $legacyDll -Force }

Write-Host "== stage $stage =="
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Copy-Item $exe $stage
Copy-Item (Join-Path $exeDir "ui") (Join-Path $stage "ui") -Recurse -Force
Copy-Item (Join-Path $exeDir "profiles") (Join-Path $stage "profiles") -Recurse -Force

$readme = @"
mstnDevController $Version (win-x64, static)
===========================================
- Static MSVC CRT (/MT) — no vcruntime140.dll needed
- WebView2Loader linked statically — no WebView2Loader.dll beside exe
- Still needs WebView2 Evergreen Runtime (Edge / WebView2 Runtime)

Run: mstnDevController.exe
Type2: Zadig WinUSB, close denconv before Start USB
"@
Set-Content -Path (Join-Path $stage "README.txt") -Value $readme -Encoding UTF8

if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip -Force

# Dependency sanity: should not need VCRUNTIME / WebView2Loader
$dumpbin = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" `
  -latest -find "**\dumpbin.exe" 2>$null | Select-Object -First 1
if ($dumpbin) {
  Write-Host "== dumpbin /dependents =="
  & $dumpbin /dependents $exe | Select-String -Pattern "\.dll" | ForEach-Object { $_.Line.Trim() }
}

Write-Host "OK: $zip"
Get-Item $zip | Format-List FullName, Length, LastWriteTime
