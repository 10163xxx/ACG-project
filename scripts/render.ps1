param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Release",

    [ValidateSet("Showcase", "Greenhouse", "Test")]
    [string]$Scene = "Showcase",

    [ValidateSet("UV", "Volume", "InhomogeneousVolume", "Mipmap", "MipmapNoFilter", "OBJ", "Materials")]
    [string]$Test = "UV",

    [string]$ObjPath = "assets\tests\models\test_model.obj",

    [switch]$Build
)

$ErrorActionPreference = "Stop"
$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Renderer = Join-Path $ProjectRoot "build\$Config\path_tracer.exe"
$Converter = Join-Path $PSScriptRoot "convert_ppm_to_jpg.py"

if ($Build) {
    cmake --build (Join-Path $ProjectRoot "build") --config $Config
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE"
    }
}
if (-not (Test-Path $Renderer)) {
    throw "Renderer not found: $Renderer. Build the project first."
}

switch ($Scene) {
    "Showcase" {
        $PpmName = "showcase_scene"
        $RenderArgs = @("--scene", "showcase")
    }
    "Greenhouse" {
        $PpmName = "greenhouse_scene"
        $RenderArgs = @("--scene", "greenhouse")
    }
    "Test" {
        switch ($Test) {
            "UV" { $TestName = "uv"; $PpmName = "uv_test" }
            "Volume" { $TestName = "volume"; $PpmName = "volume_test" }
            "InhomogeneousVolume" {
                $TestName = "inhomogeneous-volume"
                $PpmName = "inhomogeneous_volume_test"
            }
            "Mipmap" { $TestName = "mipmap"; $PpmName = "mipmap_test" }
            "MipmapNoFilter" {
                $TestName = "mipmap-no-filter"
                $PpmName = "mipmap_test_lod0"
            }
            "Materials" { $TestName = "materials"; $PpmName = "material_test" }
            "OBJ" {
                $TestName = "obj"
                $PpmName = "obj_scene"
                $ObjFullPath = Join-Path $ProjectRoot $ObjPath
                if (-not (Test-Path $ObjFullPath -PathType Leaf)) {
                    throw "OBJ file not found: $ObjFullPath"
                }
            }
        }

        $RenderArgs = @("--scene", "test", "--test", $TestName)
        if ($Test -eq "OBJ") {
            $RenderArgs += @("--obj", $ObjFullPath)
        }
    }
}

$Ppm = Join-Path $ProjectRoot "results\renders\$PpmName.ppm"
$Jpg = Join-Path $ProjectRoot "results\renders\$PpmName.jpg"

Write-Host "Configuration: $Config"
Write-Host "Scene: $Scene"
if ($Scene -eq "Test") { Write-Host "Test: $Test" }

Push-Location $ProjectRoot
try {
    & $Renderer @RenderArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Rendering failed with exit code $LASTEXITCODE"
    }

    if (Get-Command py -ErrorAction SilentlyContinue) {
        & py -3 $Converter $Ppm $Jpg
    } elseif (Get-Command python -ErrorAction SilentlyContinue) {
        & python $Converter $Ppm $Jpg
    } else {
        throw "Python was not found. Install Python and Pillow first."
    }
    if ($LASTEXITCODE -ne 0) {
        throw "PPM-to-JPG conversion failed."
    }
} finally {
    Pop-Location
}

Write-Host "Render completed: $Jpg"
