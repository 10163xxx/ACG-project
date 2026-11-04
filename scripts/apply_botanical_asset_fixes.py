from pathlib import Path

ROOT = Path.cwd()
ASSETS = ROOT / "assets"
SCRIPTS = ROOT / "scripts"

required = [
    "anthurium_botany_01_2k.blend/anthurium_botany_01_2k.obj",
    "anthurium_botany_01_2k.blend/anthurium_botany_01_2k.mtl",
    "anthurium_botany_01_2k.blend/textures/anthurium_botany_01_diff_2k.ppm",
    "anthurium_botany_01_2k.blend/textures/anthurium_botany_01_alpha_2k.ppm",
    "fern_02_2k.blend/fern_02_2k.obj",
    "fern_02_2k.blend/fern_02_2k.mtl",
    "fern_02_2k.blend/textures/fern_02_diff_2k.ppm",
    "fern_02_2k.blend/textures/fern_02_alpha_2k.ppm",
    "shrub_03_2k.blend/shrub_03_2k.obj",
    "shrub_03_2k.blend/shrub_03_2k.mtl",
    "shrub_03_2k.blend/textures/shrub_03_diff_2k.jpg",
    "shrub_03_2k.blend/textures/shrub_03_alpha_2k.png",
    "shrub_04_2k.blend/shrub_04_2k.obj",
    "shrub_04_2k.blend/shrub_04_2k.mtl",
    "shrub_04_2k.blend/textures/shrub_04_diff_2k.jpg",
    "shrub_04_2k.blend/textures/shrub_04_alpha_2k.png",
    "calathea_orbifolia_01_2k.blend/calathea_orbifolia_01_2k.obj",
    "calathea_orbifolia_01_2k.blend/calathea_orbifolia_01_2k.mtl",
    "calathea_orbifolia_01_2k.blend/textures/calathea_orbifolia_01_diff_2k.jpg",
    "calathea_orbifolia_01_2k.blend/textures/calathea_orbifolia_01_alpha_2k.png",
    "pachira_aquatica_01_2k.blend/pachira_aquatica_01_2k.obj",
    "pachira_aquatica_01_2k.blend/pachira_aquatica_01_2k.mtl",
    "pachira_aquatica_01_2k.blend/textures/pachira_aquatica_01_bark_diff_2k.png",
    "pachira_aquatica_01_2k.blend/textures/pachira_aquatica_01_bark_rough_2k.png",
    "pachira_aquatica_01_2k.blend/textures/pachira_aquatica_01_bark_nor_gl_2k.png",
    "pachira_aquatica_01_2k.blend/textures/pachira_aquatica_01_leaves_diff_2k.png",
    "pachira_aquatica_01_2k.blend/textures/pachira_aquatica_01_leaves_alpha_2k.png",
    "pachira_aquatica_01_2k.blend/textures/pachira_aquatica_01_leaves_rough_2k.png",
    "pachira_aquatica_01_2k.blend/textures/pachira_aquatica_01_leaves_nor_gl_2k.png",
    "potted-split-leaf-monstera.obj",
]
missing = [p for p in required if not (ASSETS / p).is_file()]
if missing:
    raise SystemExit("Missing expected assets:\n  - " + "\n  - ".join(missing))

SCRIPTS.mkdir(parents=True, exist_ok=True)

files = {
    "shrub_03_2k.blend/shrub_03_2k.mtl": """# Converted for ACG Path Tracer.
# Diffuse and alpha maps are converted by scripts/prepare_botanical_assets.py.

newmtl shrub_03
Ns 80.000000
Ka 1.000000 1.000000 1.000000
Kd 1.000000 1.000000 1.000000
Ks 0.04 0.04 0.04
Ni 1.000000
d 1.000000
illum 2
map_Kd textures/shrub_03_diff_2k.ppm
map_d textures/shrub_03_alpha_2k.ppm
""",
    "shrub_04_2k.blend/shrub_04_2k.mtl": """# Converted for ACG Path Tracer.
# Diffuse and alpha maps are converted by scripts/prepare_botanical_assets.py.

newmtl shrub_04
Ns 80.000000
Ka 1.000000 1.000000 1.000000
Kd 1.000000 1.000000 1.000000
Ks 0.04 0.04 0.04
Ni 1.000000
d 1.000000
illum 2
map_Kd textures/shrub_04_diff_2k.ppm
map_d textures/shrub_04_alpha_2k.ppm
""",
    "pachira_aquatica_01_2k.blend/pachira_aquatica_01_2k.mtl": """# Converted for ACG Path Tracer.
# Original asset: Poly Haven - Pachira Aquatica 01
# License: CC0 1.0
# Source maps are converted by scripts/prepare_botanical_assets.py.

newmtl pachira_aquatica_01_bark
Ns 45.000000
Ka 1.000000 1.000000 1.000000
Kd 1.000000 1.000000 1.000000
Ks 0.04 0.04 0.04
Ni 1.450000
d 1.000000
illum 2
map_Kd textures/pachira_bark_diffuse.ppm
map_Pr textures/pachira_bark_roughness.ppm
map_Kn textures/pachira_bark_normal.ppm

newmtl pachira_aquatica_01_leaves
Ns 80.000000
Ka 1.000000 1.000000 1.000000
Kd 1.000000 1.000000 1.000000
Ks 0.04 0.04 0.04
Ni 1.000000
d 1.000000
illum 2
map_Kd textures/pachira_leaves_diffuse.ppm
map_d textures/pachira_leaves_alpha.ppm
map_Pr textures/pachira_leaves_roughness.ppm
map_Kn textures/pachira_leaves_normal.ppm
""",
    "monstera.mtl": """# Material definitions for potted-split-leaf-monstera.obj.
# Original model: Innerscene Corp, dedicated under CC0 1.0 Universal.
# The source OBJ has no accompanying MTL or texture package.

newmtl Matte_ivory_plant_pot
Kd 0.72 0.68 0.58
Ks 0.04 0.04 0.04
Pr 0.78
illum 2

newmtl Potting_soil
Kd 0.095 0.060 0.030
Ks 0.02 0.02 0.02
Pr 0.96
illum 2

newmtl Green_stems
Kd 0.12 0.28 0.075
Ks 0.04 0.04 0.04
Pr 0.58
illum 2

newmtl Living_foliage
Kd 0.075 0.24 0.045
Ks 0.04 0.04 0.04
Pr 0.50
illum 2
""",
}

for relative_path, content in files.items():
    target = ASSETS / relative_path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(content, encoding="utf-8")

prepare_script = r"""from pathlib import Path
import sys

try:
    from PIL import Image
except ImportError as exc:
    raise SystemExit("Install Pillow with: py -m pip install Pillow") from exc

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
MARKER = "# ACG_TRANSFORM: Z-up to Y-up"

# ImageTexture supports PPM P3/P6. EXR maps are intentionally not referenced.
CONVERSIONS = [
    ("shrub_03_2k.blend/textures/shrub_03_diff_2k.jpg", "shrub_03_2k.blend/textures/shrub_03_diff_2k.ppm", "rgb"),
    ("shrub_03_2k.blend/textures/shrub_03_alpha_2k.png", "shrub_03_2k.blend/textures/shrub_03_alpha_2k.ppm", "gray"),
    ("shrub_04_2k.blend/textures/shrub_04_diff_2k.jpg", "shrub_04_2k.blend/textures/shrub_04_diff_2k.ppm", "rgb"),
    ("shrub_04_2k.blend/textures/shrub_04_alpha_2k.png", "shrub_04_2k.blend/textures/shrub_04_alpha_2k.ppm", "gray"),
    ("calathea_orbifolia_01_2k.blend/textures/calathea_orbifolia_01_diff_2k.jpg", "calathea_orbifolia_01_2k.blend/textures/calathea_diffuse.ppm", "rgb"),
    ("calathea_orbifolia_01_2k.blend/textures/calathea_orbifolia_01_alpha_2k.png", "calathea_orbifolia_01_2k.blend/textures/calathea_alpha.ppm", "gray"),
    ("pachira_aquatica_01_2k.blend/textures/pachira_aquatica_01_bark_diff_2k.png", "pachira_aquatica_01_2k.blend/textures/pachira_bark_diffuse.ppm", "rgb"),
    ("pachira_aquatica_01_2k.blend/textures/pachira_aquatica_01_bark_rough_2k.png", "pachira_aquatica_01_2k.blend/textures/pachira_bark_roughness.ppm", "gray"),
    ("pachira_aquatica_01_2k.blend/textures/pachira_aquatica_01_bark_nor_gl_2k.png", "pachira_aquatica_01_2k.blend/textures/pachira_bark_normal.ppm", "rgb"),
    ("pachira_aquatica_01_2k.blend/textures/pachira_aquatica_01_leaves_diff_2k.png", "pachira_aquatica_01_2k.blend/textures/pachira_leaves_diffuse.ppm", "rgb"),
    ("pachira_aquatica_01_2k.blend/textures/pachira_aquatica_01_leaves_alpha_2k.png", "pachira_aquatica_01_2k.blend/textures/pachira_leaves_alpha.ppm", "gray"),
    ("pachira_aquatica_01_2k.blend/textures/pachira_aquatica_01_leaves_rough_2k.png", "pachira_aquatica_01_2k.blend/textures/pachira_leaves_roughness.ppm", "gray"),
    ("pachira_aquatica_01_2k.blend/textures/pachira_aquatica_01_leaves_nor_gl_2k.png", "pachira_aquatica_01_2k.blend/textures/pachira_leaves_normal.ppm", "rgb"),
]

MODELS = [
    "anthurium_botany_01_2k.blend/anthurium_botany_01_2k.obj",
    "fern_02_2k.blend/fern_02_2k.obj",
    "shrub_03_2k.blend/shrub_03_2k.obj",
    "shrub_03_2k.blend/shrub_03_2k.mtl",
    "shrub_04_2k.blend/shrub_04_2k.obj",
    "shrub_04_2k.blend/shrub_04_2k.mtl",
    "calathea_orbifolia_01_2k.blend/calathea_orbifolia_01_2k.obj",
    "calathea_orbifolia_01_2k.blend/calathea_orbifolia_01_2k.mtl",
    "pachira_aquatica_01_2k.blend/pachira_aquatica_01_2k.obj",
    "pachira_aquatica_01_2k.blend/pachira_aquatica_01_2k.mtl",
    "potted-split-leaf-monstera.obj",
]


def convert_textures():
    for source_name, target_name, mode in CONVERSIONS:
        source = ASSETS / source_name
        target = ASSETS / target_name
        if not source.is_file():
            raise FileNotFoundError(f"Missing source texture: {source}")
        with Image.open(source) as image:
            converted = image.convert("L").convert("RGB") if mode == "gray" else image.convert("RGB")
            converted.save(target, format="PPM")
        print(f"Converted: {source.relative_to(ROOT)}")


Z_UP_MODELS = [
    "anthurium_botany_01_2k.blend/anthurium_botany_01_2k.obj",
    "fern_02_2k.blend/fern_02_2k.obj",
    "potted-split-leaf-monstera.obj",
]


def rotate_z_up_models():
    # These source assets are Z-up; the path tracer scene is Y-up.
    # Proper rotation around X: (x, y, z) -> (x, z, -y).
    for relative_path in Z_UP_MODELS:
        path = ASSETS / relative_path
        source = path.read_text(encoding="utf-8")
        if MARKER in source:
            print(f"Already Y-up: {relative_path}")
            continue

        output = [MARKER]
        for line in source.splitlines():
            fields = line.strip().split()
            if fields and fields[0] in {"v", "vn"} and len(fields) >= 4:
                x, y, z = map(float, fields[1:4])
                line = " ".join([
                    fields[0], f"{x:.9g}", f"{z:.9g}", f"{-y:.9g}", *fields[4:]
                ])
            output.append(line)
        path.write_text("\n".join(output) + "\n", encoding="utf-8")
        print(f"Rotated to Y-up: {relative_path}")


def validate_assets():
    errors = []
    for model_name in MODELS:
        obj_path = ASSETS / model_name
        if not obj_path.is_file() or obj_path.stat().st_size == 0:
            errors.append(f"Missing or empty OBJ: {model_name}")
            continue

        libraries, used_materials, face_count = [], set(), 0
        with obj_path.open("r", encoding="utf-8", errors="replace") as stream:
            for line in stream:
                fields = line.strip().split()
                if not fields:
                    continue
                if fields[0] == "mtllib":
                    libraries.extend(fields[1:])
                elif fields[0] == "usemtl" and len(fields) > 1:
                    used_materials.add(fields[1])
                elif fields[0] == "f":
                    face_count += 1

        if not face_count:
            errors.append(f"OBJ has no faces: {model_name}")
            continue
        if not libraries:
            errors.append(f"OBJ has no mtllib declaration: {model_name}")

        for library in libraries:
            mtl_path = obj_path.parent / library
            if not mtl_path.is_file():
                errors.append(f"Missing MTL: {mtl_path.relative_to(ROOT)}")
                continue

            declared = set()
            with mtl_path.open("r", encoding="utf-8", errors="replace") as stream:
                for line in stream:
                    fields = line.strip().split()
                    if not fields:
                        continue
                    if fields[0] == "newmtl" and len(fields) > 1:
                        declared.add(fields[1])
                    elif fields[0].startswith("map_") or fields[0] in {"bump", "map_Bump", "map_bump"}:
                        texture = mtl_path.parent / fields[-1]
                        if not texture.is_file():
                            errors.append(f"Missing texture: {texture.relative_to(ROOT)}")

            absent = used_materials - declared
            if absent:
                errors.append(f"{model_name} has undefined materials: {', '.join(sorted(absent))}")

        print(f"Validated: {model_name} ({face_count} face records)")

    if errors:
        print("\nAsset validation failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        raise SystemExit(1)
    print("\nAll botanical OBJ/MTL/texture references are present.")


if __name__ == "__main__":
    convert_textures()
    rotate_z_up_models()
    validate_assets()
"""

(SCRIPTS / "prepare_botanical_assets.py").write_text(
    prepare_script, encoding="utf-8"
)

print("Applied botanical MTL fixes and added scripts/prepare_botanical_assets.py")
print("Run: py -m pip install Pillow")
print("Then: py scripts/prepare_botanical_assets.py")
