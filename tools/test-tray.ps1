[CmdletBinding()]
param([Parameter(Mandatory)][string]$Toolchain, [switch]$Integration)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$compiler=Join-Path $Toolchain 'bin/clang++.exe'
$output=Join-Path $projectRoot 'build/test-tray-registration.exe'
New-Item -ItemType Directory -Path (Split-Path -Parent $output) -Force | Out-Null
$common=@('--target=x86_64-w64-mingw32','-std=c++20','-O2','-UNDEBUG','-Wall','-Wextra',
    '-DUNICODE','-D_UNICODE','-DNOMINMAX','-DWIN32_LEAN_AND_MEAN','-D_WIN32_WINNT=0x0A00',
    '-I',(Join-Path $projectRoot 'src'))
& $compiler @common (Join-Path $PSScriptRoot 'test-tray-registration.cpp') -luser32 -lgdi32 -static -o $output
if($LASTEXITCODE -ne 0){throw 'Tray regression test compilation failed.'}
& $output
if($LASTEXITCODE -ne 0){throw 'Tray regression tests failed.'}
if($Integration) {
    $resources=Join-Path $projectRoot 'build/portable/resources.o'
    if(-not (Test-Path -LiteralPath $resources)){throw 'Build the portable app first to generate resource data.'}
    $integrationOutput=Join-Path $projectRoot 'build/test-tray-integration.exe'
    & $compiler @common (Join-Path $PSScriptRoot 'test-tray-integration.cpp') `
        (Join-Path $projectRoot 'src/power_controller.cpp') (Join-Path $projectRoot 'src/renderer.cpp') $resources `
        -ladvapi32 -ld2d1 -ld3d11 -ldxgi -ldcomp -ldwrite -ldwmapi -lgdi32 -lole32 -lshell32 -luser32 `
        -lwindowscodecs -static -o $integrationOutput
    if($LASTEXITCODE -ne 0){throw 'Tray integration test compilation failed.'}
    & $integrationOutput
    if($LASTEXITCODE -ne 0){throw 'Tray integration tests failed.'}
}
