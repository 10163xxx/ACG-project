"""Convert the newly imported Poly Haven greenhouse assets to the
PPM texture pipeline used by the ACG Path Tracer.

Source textures are the 2k downloads placed under
assets/<asset>_2k.blend/textures/. Diffuse maps convert to RGB P6
PPM, opacity masks to grayscale-encoded RGB P6 PPM and roughness
maps to grayscale-encoded RGB P6 PPM.
"""

from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"


def save_ppm(source: Path, destination: Path, mode: str) -> None:
    if not source.exists():
        raise FileNotFoundError(f"Missing texture: {source}")

    image = Image.open(source)

    if mode == "RGB":
        image = image.convert("RGB")
    elif mode == "MASK":
        if "A" in image.getbands():
            alpha = image.getchannel("A")
            if alpha.getextrema() != (255, 255):
                image = alpha
            else:
                image = image.convert("RGB").convert("L")
        else:
            image = image.convert("L")
    elif mode == "GRAY":
        image = image.convert("L")
    else:
        raise ValueError(f"Unsupported output mode: {mode}")

    image = image.convert("RGB")

    destination.parent.mkdir(parents=True, exist_ok=True)

    with destination.open("wb") as output:
        output.write(f"P6\n{image.width} {image.height}\n255\n".encode("ascii"))
        output.write(image.tobytes())

    print(f"Converted: {source.name} -> {destination.name}")


def main() -> None:
    conversions = [
        (
            ASSETS / "moss_01_2k.blend/textures/moss_01_diff_2k.jpg",
            ASSETS / "moss_01_2k.blend/textures/moss_01_diffuse.ppm",
            "RGB",
        ),
        (
            ASSETS / "moss_01_2k.blend/textures/moss_01_alpha_2k.png",
            ASSETS / "moss_01_2k.blend/textures/moss_01_alpha.ppm",
            "MASK",
        ),
        (
            ASSETS / "rock_moss_set_01_2k.blend/textures/"
            "rock_moss_set_01_diff_2k.jpg",
            ASSETS / "rock_moss_set_01_2k.blend/textures/"
            "rock_moss_diffuse.ppm",
            "RGB",
        ),
        (
            ASSETS / "rock_moss_set_01_2k.blend/textures/"
            "rock_moss_set_01_rough_2k.jpg",
            ASSETS / "rock_moss_set_01_2k.blend/textures/"
            "rock_moss_roughness.ppm",
            "GRAY",
        ),
        (
            ASSETS / "shrub_01_2k.blend/textures/shrub_01_diff_2k.jpg",
            ASSETS / "shrub_01_2k.blend/textures/shrub_01_diffuse.ppm",
            "RGB",
        ),
        (
            ASSETS / "shrub_01_2k.blend/textures/shrub_01_alpha_2k.png",
            ASSETS / "shrub_01_2k.blend/textures/shrub_01_alpha.ppm",
            "MASK",
        ),
        (
            ASSETS / "periwinkle_plant_2k.blend/textures/"
            "periwinkle_plant_diff_2k.jpg",
            ASSETS / "periwinkle_plant_2k.blend/textures/"
            "periwinkle_diffuse.ppm",
            "RGB",
        ),
        (
            ASSETS / "periwinkle_plant_2k.blend/textures/"
            "periwinkle_plant_opacity_2k.png",
            ASSETS / "periwinkle_plant_2k.blend/textures/"
            "periwinkle_opacity.ppm",
            "MASK",
        ),
    ]

    for source, destination, mode in conversions:
        save_ppm(source, destination, mode)


if __name__ == "__main__":
    main()
