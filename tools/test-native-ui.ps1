[CmdletBinding()]
param([Parameter(Mandatory)][string]$Toolchain)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$testOutput = Join-Path $projectRoot 'build\test-native-ui.exe'
New-Item -ItemType Directory -Path (Split-Path -Parent $testOutput) -Force | Out-Null
& (Join-Path $Toolchain 'bin\clang++.exe') --target=x86_64-w64-mingw32 `
    -std=c++20 -O2 -Wall -Wextra -UNDEBUG -DUNICODE -D_UNICODE -DNOMINMAX -D_WIN32_WINNT=0x0A00 `
    -I (Join-Path $projectRoot 'src') (Join-Path $PSScriptRoot 'test-native-ui.cpp') `
    -lole32 -ladvapi32 -lgdi32 -luser32 -static -o $testOutput
if ($LASTEXITCODE -ne 0) { throw 'Native UI test compilation failed.' }
& $testOutput
if ($LASTEXITCODE -ne 0) { throw 'Native UI tests failed.' }
