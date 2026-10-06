param(
    [string]$BuildDir = "$PSScriptRoot\build-windows",
    [string]$DistDir = "$PSScriptRoot\dist",
    [switch]$PackageOnly
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = $PSScriptRoot
$BuildDir = [IO.Path]::GetFullPath($BuildDir)
$DistDir = [IO.Path]::GetFullPath($DistDir)
$version = [regex]::Match((Get-Content "$root\CMakeLists.txt" -Raw), 'project\(Aura VERSION ([0-9.]+)').Groups[1].Value
if (-not $version) { throw 'Cannot read the Aura version.' }
if (-not $PackageOnly) {
    & cmake -S $root -B $BuildDir -G 'Visual Studio 17 2022' -A x64 -DAURA_BUILD_TESTS=ON
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
    & cmake --build $BuildDir --config Release --parallel 4 --target Aura_VST3 Aura_Standalone AuraTests
    if ($LASTEXITCODE -ne 0) { throw 'Release build failed.' }
    & ctest --test-dir $BuildDir -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Regression tests failed.' }
}
$iscc = Get-Command ISCC.exe -ErrorAction SilentlyContinue
$compiler = if ($iscc) { $iscc.Source } else { "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe" }
if (-not (Test-Path $compiler)) { $compiler = "$env:ProgramFiles\Inno Setup 7\ISCC.exe" }
if (-not (Test-Path $compiler)) { throw 'Install Inno Setup 6 or 7 from https://jrsoftware.org/isinfo.php.' }
New-Item -ItemType Directory -Force -Path $DistDir | Out-Null
& $compiler "/DAuraVersion=$version" "/DBuildDir=$BuildDir" "/O$DistDir" "$root\Packaging\windows\Aura.iss"
if ($LASTEXITCODE -ne 0) { throw 'Setup compilation failed.' }
$stage = Join-Path $BuildDir 'portable-release'
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Copy-Item "$BuildDir\Aura_artefacts\Release\VST3\Aura.vst3" $stage -Recurse -Force
Copy-Item "$BuildDir\Aura_artefacts\Release\Standalone\Aura.exe" $stage -Force
Copy-Item "$root\LICENSE", "$root\THIRD_PARTY_NOTICES.md", "$root\Packaging\windows\SOURCE.txt" $stage -Force
Copy-Item "$root\ThirdParty" $stage -Recurse -Force
Compress-Archive -Path "$stage\*" -DestinationPath "$DistDir\Aura-$version-Windows-x64.zip" -Force
Copy-Item "$BuildDir\Aura_artefacts\Release\Standalone\Aura.exe" "$DistDir\Aura-$version-Windows-x64.exe" -Force
Get-ChildItem $DistDir -File | Where-Object { $_.Extension -in '.exe', '.zip' } | ForEach-Object {
    $hash = (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $($_.Name)" | Set-Content -Path "$($_.FullName).sha256" -Encoding ascii
}
Write-Host "Packaged Aura $version Windows x64 in $DistDir"
