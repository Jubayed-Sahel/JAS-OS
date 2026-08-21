# Build JAS OS x86 ISO using the bundled i686-elf toolchain.
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root
python "$Root\scripts\build.py"
$iso = Join-Path $Root "build\jas-os.iso"
if (Test-Path $iso) {
    Write-Host ""
    Write-Host "ISO ready: $iso"
    Write-Host "VirtualBox: Other/Unknown (32-bit), 64+ MB RAM, BIOS (not EFI), PS/2 mouse."
} else {
    throw "ISO was not created."
}
