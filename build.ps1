# Build (and optionally run) on Windows.  .\build.ps1 [-Run]
param([switch]$Run)
$root = $PSScriptRoot
$gcc = "$root\..\toolchain\w64devkit\bin"
if (Test-Path $gcc) { $env:PATH = "$gcc;$env:PATH" }
New-Item -ItemType Directory -Force "$root\build" | Out-Null
Push-Location $root
gcc -std=gnu99 -O2 -Wall -Wextra -Ithird_party\raylib\include -o build\jamsandbox.exe src\main.c -Lthird_party\raylib\lib -lraylib -lopengl32 -lgdi32 -lwinmm
if ($LASTEXITCODE -eq 0 -and $Run) { .\build\jamsandbox.exe }
Pop-Location
