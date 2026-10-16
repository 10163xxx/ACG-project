import argparse
from pathlib import Path
from PIL import Image


def convert_ppm_to_jpg(input_path: Path, output_path: Path) -> None:
    if not input_path.is_file():
        raise FileNotFoundError(f"PPM image not found: {input_path}")

    output_path.parent.mkdir(parents=True, exist_ok=True)

    with Image.open(input_path) as image:
        image.convert("RGB").save(
            output_path,
            format="JPEG",
            quality=95,
            optimize=True,
        )

    print(f"JPG saved: {output_path}")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Convert a rendered PPM image to JPEG."
    )
    parser.add_argument("input", type=Path, help="Input PPM file")
    parser.add_argument("output", type=Path, help="Output JPG file")
    args = parser.parse_args()

    convert_ppm_to_jpg(args.input, args.output)


if __name__ == "__main__":
    main()
