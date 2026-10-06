param([string]$BuildDir, [string]$DistDir)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$setup = Get-ChildItem $DistDir -Filter '*-Setup.exe' | Select-Object -First 1
if (-not $setup) { throw 'Missing setup executable.' }
$installDir = Join-Path $env:ProgramFiles 'Aura'
$vstDir = Join-Path $env:CommonProgramFiles 'VST3\Aura.vst3'
if ((Test-Path $installDir) -or (Test-Path $vstDir)) { throw 'Installer smoke test requires a clean runner.' }
$p = Start-Process $setup.FullName -ArgumentList '/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART' -Wait -PassThru
if ($p.ExitCode -ne 0) { throw "Installer failed: $($p.ExitCode)" }
$installedExe = Join-Path $installDir 'Aura.exe'
$installedVst = Join-Path $vstDir 'Contents\x86_64-win\Aura.vst3'
foreach ($pair in @(
    @($installedExe, "$BuildDir\Aura_artefacts\Release\Standalone\Aura.exe"),
    @($installedVst, "$BuildDir\Aura_artefacts\Release\VST3\Aura.vst3\Contents\x86_64-win\Aura.vst3")
)) {
    if (-not (Test-Path $pair[0])) { throw "Missing installed payload: $($pair[0])" }
    if ((Get-FileHash $pair[0]).Hash -ne (Get-FileHash $pair[1]).Hash) { throw 'Installed payload hash mismatch.' }
}
if (-not (Test-Path "$installDir\LICENSE")) { throw 'License was not installed.' }
Write-Host 'PASS installer payloads and license match the built release'
$p = Start-Process "$installDir\unins000.exe" -ArgumentList '/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART' -Wait -PassThru
if ($p.ExitCode -ne 0) { throw 'Uninstaller failed.' }
if ((Test-Path $installedExe) -or (Test-Path $installedVst)) { throw 'Uninstaller left an installed executable.' }
Write-Host 'PASS uninstaller removed its payloads'
