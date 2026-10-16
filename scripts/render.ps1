param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug"
)

$ErrorActionPreference = "Stop"

$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Renderer = Join-Path $ProjectRoot "build\$Config\path_tracer.exe"
$Converter = Join-Path $PSScriptRoot "convert_ppm_to_jpg.py"
$Ppm = Join-Path $ProjectRoot "results\renders\material_test.ppm"
$Jpg = Join-Path $ProjectRoot "results\renders\material_test.jpg"

if (-not (Test-Path $Renderer)) {
    throw "Renderer not found: $Renderer. Build the project first."
}

# Step 1: Render the image.
& $Renderer

if ($LASTEXITCODE -ne 0) {
    throw "Rendering failed with exit code $LASTEXITCODE"
}

# Step 2: Convert PPM to JPG.
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

Write-Host "Render and JPG conversion completed."
