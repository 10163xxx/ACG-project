import csv
from collections import defaultdict
from pathlib import Path

OBJ = Path("assets/anthurium_botany_01_2k.blend/anthurium_botany_01_2k.obj")
REPORT = Path("anthurium_scan_report.csv")
OUTPUT = Path("anthurium_neighbor_report.txt")

vertices = []
faces = []
abnormal_lines = set()

with REPORT.open(newline="", encoding="utf-8-sig") as f:
    for row in csv.DictReader(f):
        abnormal_lines.add(int(row["line"]))

edge_faces = defaultdict(list)

with OBJ.open("r", encoding="utf-8", errors="replace") as f:
    for line_number, line in enumerate(f, 1):
        parts = line.split()

        if not parts:
            continue

        if parts[0] == "v":
            vertices.append(tuple(map(float, parts[1:4])))

        elif parts[0] == "f":
            indices = []

            for token in parts[1:]:
                index = int(token.split("/")[0])

                if index > 0:
                    index -= 1
                else:
                    index = len(vertices) + index

                indices.append(index)

            face = {
                "line": line_number,
                "indices": indices,
                "source": line.strip()
            }

            faces.append(face)

            for i in range(len(indices)):
                a = indices[i]
                b = indices[(i + 1) % len(indices)]
                edge = tuple(sorted((a, b)))
                edge_faces[edge].append(face)

output = []

for face in faces:
    if face["line"] not in abnormal_lines:
        continue

    output.append("")
    output.append("=" * 80)
    output.append(f"ABNORMAL FACE: LINE {face['line']}")
    output.append(face["source"])

    indices = face["indices"]

    for i in range(len(indices)):
        a = indices[i]
        b = indices[(i + 1) % len(indices)]
        edge = tuple(sorted((a, b)))

        output.append("")
        output.append(f"EDGE {a + 1} -- {b + 1}")

        neighbors = [
            other for other in edge_faces[edge]
            if other["line"] != face["line"]
        ]

        if not neighbors:
            output.append("  No adjacent face")
            continue

        for other in neighbors:
            output.append(
                f"  LINE {other['line']}: {other['source']}"
            )

            for index in other["indices"]:
                x, y, z = vertices[index]
                output.append(
                    f"    v{index + 1}: {x:.9f} {y:.9f} {z:.9f}"
                )

OUTPUT.write_text("\n".join(output), encoding="utf-8")

print(f"Faces scanned: {len(faces)}")
print(f"Abnormal faces analyzed: {len(abnormal_lines)}")
print(f"Report written to: {OUTPUT}")
