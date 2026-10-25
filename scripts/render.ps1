param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Release",

    [ValidateSet("Showcase", "UVTest", "VolumeTest", "MipmapTest", "MipmapTestNoFilter", "OBJ")]
    [string]$Scene = "Showcase",

    [string]$ObjPath = "assets\models\test_model.obj",

    [switch]$Build
)

$ErrorActionPreference = "Stop"

$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Renderer = Join-Path $ProjectRoot "build\$Config\path_tracer.exe"
$Converter = Join-Path $PSScriptRoot "convert_ppm_to_jpg.py"

if ($Build) {
    Write-Host "Building configuration: $Config"

    cmake --build (Join-Path $ProjectRoot "build") --config $Config

    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE"
    }
}

if (-not (Test-Path $Renderer)) {
    throw "Renderer not found: $Renderer. Build the project first."
}

switch ($Scene) {
    "MipmapTestNoFilter" {
        $Ppm = Join-Path $ProjectRoot "results\renders\mipmap_test_lod0.ppm"
        $Jpg = Join-Path $ProjectRoot "results\renders\mipmap_test_lod0.jpg"
        $RenderArgs = @("--mipmap-test-lod0")
    }
    
    "MipmapTest" {
        $Ppm = Join-Path $ProjectRoot "results\renders\mipmap_test.ppm"
        $Jpg = Join-Path $ProjectRoot "results\renders\mipmap_test.jpg"
        $RenderArgs = @("--mipmap-test")
    }

    "UVTest" {
        $Ppm = Join-Path $ProjectRoot "results\renders\uv_test.ppm"
        $Jpg = Join-Path $ProjectRoot "results\renders\uv_test.jpg"
        $RenderArgs = @("--uv-test")
    }

    "VolumeTest" {
        $Ppm = Join-Path $ProjectRoot "results\renders\volume_test.ppm"
        $Jpg = Join-Path $ProjectRoot "results\renders\volume_test.jpg"
        $RenderArgs = @("--volume-test")
    }

    "OBJ" {
        $ObjFullPath = Join-Path $ProjectRoot $ObjPath

        if (-not (Test-Path $ObjFullPath -PathType Leaf)) {
            throw "OBJ file not found: $ObjFullPath"
        }

        $Ppm = Join-Path $ProjectRoot "results\renders\obj_scene.ppm"
        $Jpg = Join-Path $ProjectRoot "results\renders\obj_scene.jpg"
        $RenderArgs = @("--obj", $ObjFullPath)
    }

    default {
        $Ppm = Join-Path $ProjectRoot "results\renders\showcase_scene.ppm"
        $Jpg = Join-Path $ProjectRoot "results\renders\showcase_scene.jpg"
        $RenderArgs = @()
    }
}

Write-Host "Configuration: $Config"
Write-Host "Scene: $Scene"

if ($Scene -eq "OBJ") {
    Write-Host "OBJ file: $ObjFullPath"
}

Write-Host "Rendering..."
Push-Location $ProjectRoot

try {
    & $Renderer @RenderArgs

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
}
finally {
    Pop-Location
}

Write-Host "Render and JPG conversion completed: $Jpg"