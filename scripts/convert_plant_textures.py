from pathlib import Path
from PIL import Image
import sys


ROOT = Path(__file__).resolve().parents[1]


def save_ppm(source: Path, destination: Path, mode: str = "RGB"):
    if not source.exists():
        raise FileNotFoundError(f"Missing texture: {source}")

    image = Image.open(source)

    if mode == "RGB":
        image = image.convert("RGB")
    elif mode == "MASK":
        print(
            f"{source.name}: mode={image.mode}, "
            f"bands={image.getbands()}, "
            f"extrema={image.getextrema()}"
        )

        # Use a real alpha channel only when it contains varying values.
        if "A" in image.getbands():
            alpha = image.getchannel("A")
            if alpha.getextrema() != (255, 255):
                image = alpha
            else:
                image = image.convert("RGB").convert("L")
        else:
            image = image.convert("L")

        image = image.convert("RGB")
    elif mode == "GRAY":
        image = image.convert("L").convert("RGB")
    else:
        raise ValueError(f"Unsupported output mode: {mode}")

    destination.parent.mkdir(parents=True, exist_ok=True)

    with destination.open("wb") as output:
        output.write(f"P6\n{image.width} {image.height}\n255\n".encode("ascii"))
        output.write(image.tobytes())

    print(f"Converted: {source.name} -> {destination.name}")


def main():
    assets = ROOT / "assets"

    conversions = [
        (
            assets / "calathea_orbifolia_01_2k.blend/textures/"
            "calathea_orbifolia_01_diff_2k.jpg",
            assets / "calathea_orbifolia_01_2k.blend/textures/"
            "calathea_diffuse.ppm",
            "RGB",
        ),
        (
            assets / "calathea_orbifolia_01_2k.blend/textures/"
            "calathea_orbifolia_01_alpha_2k.png",
            assets / "calathea_orbifolia_01_2k.blend/textures/"
            "calathea_alpha.ppm",
            "MASK",
        ),
        (
            assets / "pachira_aquatica_01_2k.blend/textures/"
            "pachira_aquatica_01_bark_diff_2k.png",
            assets / "pachira_aquatica_01_2k.blend/textures/"
            "pachira_bark_diffuse.ppm",
            "RGB",
        ),
        (
            assets / "pachira_aquatica_01_2k.blend/textures/"
            "pachira_aquatica_01_bark_rough_2k.png",
            assets / "pachira_aquatica_01_2k.blend/textures/"
            "pachira_bark_roughness.ppm",
            "GRAY",
        ),
        (
            assets / "pachira_aquatica_01_2k.blend/textures/"
            "pachira_aquatica_01_leaves_diff_2k.png",
            assets / "pachira_aquatica_01_2k.blend/textures/"
            "pachira_leaves_diffuse.ppm",
            "RGB",
        ),
        (
            assets / "pachira_aquatica_01_2k.blend/textures/"
            "pachira_aquatica_01_leaves_alpha_2k.png",
            assets / "pachira_aquatica_01_2k.blend/textures/"
            "pachira_leaves_alpha.ppm",
            "MASK",
        ),
        (
            assets / "pachira_aquatica_01_2k.blend/textures/"
            "pachira_aquatica_01_leaves_rough_2k.png",
            assets / "pachira_aquatica_01_2k.blend/textures/"
            "pachira_leaves_roughness.ppm",
            "GRAY",
        ),
    ]

    for source, destination, mode in conversions:
        save_ppm(source, destination, mode)


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(f"Texture conversion failed: {error}", file=sys.stderr)
        raise