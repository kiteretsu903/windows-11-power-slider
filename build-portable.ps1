[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$Toolchain
)

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$bin = Join-Path $Toolchain 'bin'
$build = Join-Path $root 'build\portable'
$dist = Join-Path $root 'dist\app'

if (-not (Test-Path -LiteralPath (Join-Path $bin 'clang++.exe'))) {
    throw "LLVM-MinGW was not found at '$Toolchain'."
}

New-Item -ItemType Directory -Path $build -Force | Out-Null
New-Item -ItemType Directory -Path $dist -Force | Out-Null

& (Join-Path $root 'tools\prepare-assets.ps1')

& (Join-Path $bin 'x86_64-w64-mingw32-windres.exe') `
    (Join-Path $root 'src\resources.rc') `
    -I (Join-Path $root 'src') -O coff -o (Join-Path $build 'resources.o')
if ($LASTEXITCODE -ne 0) { throw "windres failed with exit code $LASTEXITCODE" }

$arguments = @(
    '--target=x86_64-w64-mingw32', '-std=c++20', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-municode',
    '-DUNICODE', '-D_UNICODE', '-DWIN32_LEAN_AND_MEAN', '-DNOMINMAX', '-D_WIN32_WINNT=0x0A00',
    '-I', (Join-Path $root 'src'),
    (Join-Path $root 'src\main.cpp'),
    (Join-Path $root 'src\power_controller.cpp'),
    (Join-Path $root 'src\renderer.cpp'),
    (Join-Path $build 'resources.o'),
    '-ladvapi32', '-ld2d1', '-ld3d11', '-ldxgi', '-ldcomp', '-ldwrite', '-ldwmapi', '-lgdi32', '-lole32', '-lshell32', '-luser32',
    '-lwindowscodecs', '-static', '-Wl,--subsystem,windows',
    '-o', (Join-Path $dist 'PowerModeNative.exe')
)

& (Join-Path $bin 'clang++.exe') @arguments
if ($LASTEXITCODE -ne 0) { throw "clang++ failed with exit code $LASTEXITCODE" }

$artifact = Join-Path $dist 'PowerModeNative.exe'
Copy-Item -LiteralPath (Join-Path $root 'assets\fluent\LICENSE.txt') -Destination (Join-Path $dist 'Fluent-Icons-LICENSE.txt')
$hash = Get-FileHash -Algorithm SHA256 -LiteralPath $artifact
[pscustomobject]@{
    File = $artifact
    Bytes = (Get-Item -LiteralPath $artifact).Length
    SHA256 = $hash.Hash
}
