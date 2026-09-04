[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [switch]$Installer
)

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$build = Join-Path $root 'build'
$stage = Join-Path $root 'dist\app'

cmake -S $root -B $build -A x64
cmake --build $build --config $Configuration --parallel
cmake --install $build --config $Configuration --prefix $stage

if ($Installer) {
    $iscc = Get-Command iscc.exe -ErrorAction SilentlyContinue
    if (-not $iscc) {
        $candidate = Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe'
        if (Test-Path -LiteralPath $candidate) {
            $iscc = Get-Item -LiteralPath $candidate
        }
    }
    if (-not $iscc) {
        $candidate = 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe'
        if (Test-Path -LiteralPath $candidate) {
            $iscc = Get-Item -LiteralPath $candidate
        }
    }
    if (-not $iscc) {
        throw 'Inno Setup 6 was not found. Install JRSoftware.InnoSetup or build without -Installer.'
    }
    & $iscc.FullName (Join-Path $root 'installer\PowerModeNative.iss')
}

Get-ChildItem -LiteralPath (Join-Path $root 'dist') -Recurse -File |
    Select-Object FullName, Length
