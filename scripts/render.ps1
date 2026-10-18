param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",

    [ValidateSet("Showcase", "UVTest")]
    [string]$Scene = "Showcase"
)

$ErrorActionPreference = "Stop"

$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Renderer = Join-Path $ProjectRoot "build\$Config\path_tracer.exe"
$Converter = Join-Path $PSScriptRoot "convert_ppm_to_jpg.py"

if (-not (Test-Path $Renderer)) {
    throw "Renderer not found: $Renderer. Build the project first."
}

if ($Scene -eq "UVTest") {
    $Ppm = Join-Path $ProjectRoot "results\renders\uv_test.ppm"
    $Jpg = Join-Path $ProjectRoot "results\renders\uv_test.jpg"

    & $Renderer --uv-test
}
else {
    $Ppm = Join-Path $ProjectRoot "results\renders\showcase_scene.ppm"
    $Jpg = Join-Path $ProjectRoot "results\renders\showcase_scene.jpg"

    & $Renderer
}

if ($LASTEXITCODE -ne 0) {
    throw "Rendering failed with exit code $LASTEXITCODE"
}

if (Get-Command py -ErrorAction SilentlyContinue) {
    & py -3 $Converter $Ppm $Jpg
}
elseif (Get-Command python -ErrorAction SilentlyContinue) {
    & python $Converter $Ppm $Jpg
}
else {
    throw "Python was not found. Install Python and Pillow first."
}

if ($LASTEXITCODE -ne 0) {
    throw "PPM-to-JPG conversion failed."
}

Write-Host "Render and JPG conversion completed: $Jpg"
