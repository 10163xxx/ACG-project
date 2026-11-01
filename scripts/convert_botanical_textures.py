from pathlib import Path
from PIL import Image


ASSETS = [
    (
        Path("assets/anthurium_botany_01_2k.blend/textures"),
        "anthurium_botany_01_diff_2k.jpg",
        "anthurium_botany_01_alpha_2k.png",
    ),
    (
        Path("assets/fern_02_2k.blend/textures"),
        "fern_02_diff_2k.jpg",
        "fern_02_alpha_2k.png",
    ),
]


def convert_rgb(source, destination):
    with Image.open(source) as image:
        image.convert("RGB").save(destination, format="PPM")


def convert_alpha(source, destination):
    with Image.open(source) as image:
        image.convert("L").convert("RGB").save(
            destination,
            format="PPM"
        )


for directory, diffuse_name, alpha_name in ASSETS:
    diffuse_source = directory / diffuse_name
    alpha_source = directory / alpha_name

    diffuse_output = directory / (
        Path(diffuse_name).stem + ".ppm"
    )

    alpha_output = directory / (
        Path(alpha_name).stem + ".ppm"
    )

    convert_rgb(diffuse_source, diffuse_output)
    convert_alpha(alpha_source, alpha_output)

    print("Converted:", diffuse_output)
    print("Converted:", alpha_output)

print("Botanical texture conversion completed.")
