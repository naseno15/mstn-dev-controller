# Downloads Microsoft.Web.WebView2 NuGet into third_party/ (dynamic loader DLL, not static).
$ErrorActionPreference = "Stop"
$ver = if ($args.Count -gt 0) { $args[0] } else { "1.0.2903.40" }
$proj = Split-Path $PSScriptRoot -Parent
$out = Join-Path $proj "third_party\webview2"
$extract = Join-Path $out "pkg"
$nupkg = Join-Path $env:TEMP "Microsoft.Web.WebView2.$ver.nupkg"
$zip = Join-Path $env:TEMP "Microsoft.Web.WebView2.$ver.zip"
$url = "https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/$ver/microsoft.web.webview2.$ver.nupkg"

New-Item -ItemType Directory -Force -Path $out | Out-Null
Write-Host "Downloading $url"
Invoke-WebRequest -Uri $url -OutFile $nupkg
Copy-Item $nupkg $zip -Force
if (Test-Path $extract) { Remove-Item -Recurse -Force $extract }
Expand-Archive -LiteralPath $zip -DestinationPath $extract -Force

$native = Join-Path $extract "build\native"
$header = Join-Path $native "include\WebView2.h"
if (-not (Test-Path $header)) { throw "WebView2.h missing after extract at $header" }
Write-Host "OK: $native"
Write-Host "cmake -S . -B build -DWEBVIEW2_ROOT=`"$native`""
