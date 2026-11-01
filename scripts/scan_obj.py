import math
import csv
import sys

obj_path = r"assets/anthurium_botany_01_2k.blend/anthurium_botany_01_2k.obj"
report_path = "anthurium_scan_report.csv"

AREA_EPS = 1e-12
COORD_EPS = 1e-12

vertices = []
issues = []
face_count = 0

def cross2(a, b, c):
    return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])

def sign_eps(v):
    if v > AREA_EPS:
        return 1
    if v < -AREA_EPS:
        return -1
    return 0

def on_segment(a, b, p):
    return (
        min(a[0], b[0]) - COORD_EPS <= p[0] <= max(a[0], b[0]) + COORD_EPS
        and min(a[1], b[1]) - COORD_EPS <= p[1] <= max(a[1], b[1]) + COORD_EPS
    )

def segments_intersect(a, b, c, d):
    o1 = cross2(a, b, c)
    o2 = cross2(a, b, d)
    o3 = cross2(c, d, a)
    o4 = cross2(c, d, b)

    s1, s2, s3, s4 = map(sign_eps, (o1, o2, o3, o4))

    if s1 * s2 < 0 and s3 * s4 < 0:
        return True

    if s1 == 0 and on_segment(a, b, c):
        return True
    if s2 == 0 and on_segment(a, b, d):
        return True
    if s3 == 0 and on_segment(c, d, a):
        return True
    if s4 == 0 and on_segment(c, d, b):
        return True

    return False

def inspect_face(indices, line_number):
    polygon = list(range(len(indices)))

    if len(indices) < 3:
        return "fewer than 3 vertices"

    if indices[0] == indices[-1]:
        polygon.pop()

    if len(polygon) < 3:
        return "degenerate polygon"

    points3 = [vertices[indices[i]] for i in polygon]

    for i in range(len(points3)):
        a = points3[i]
        b = points3[(i + 1) % len(points3)]

        if a == b:
            return "consecutive duplicate vertices"

        if not all(math.isfinite(x) for x in a):
            return "non-finite vertex"

    minimum = [min(p[k] for p in points3) for k in range(3)]
    maximum = [max(p[k] for p in points3) for k in range(3)]
    extent = max(maximum[k] - minimum[k] for k in range(3))

    if not math.isfinite(extent) or extent <= 0:
        return "invalid 3D extent"

    origin = points3[0]
    normal = [0.0, 0.0, 0.0]

    for i in range(len(points3)):
        a = [(points3[i][k] - origin[k]) / extent for k in range(3)]
        b = [(points3[(i + 1) % len(points3)][k] - origin[k]) / extent for k in range(3)]

        normal[0] += (a[1] - b[1]) * (a[2] + b[2])
        normal[1] += (a[2] - b[2]) * (a[0] + b[0])
        normal[2] += (a[0] - b[0]) * (a[1] + b[1])

    normal_length = math.hypot(*normal)

    if not math.isfinite(normal_length) or normal_length <= 1e-12:
        return "degenerate or invalid normal"

    ax, ay, az = map(abs, normal)

    dropped = 0
    if ay > ax and ay >= az:
        dropped = 1
    elif az > ax and az > ay:
        dropped = 2

    projected = []

    for p in points3:
        relative = [(p[k] - origin[k]) / extent for k in range(3)]

        if dropped == 0:
            projected.append((relative[1], relative[2]))
        elif dropped == 1:
            projected.append((relative[0], relative[2]))
        else:
            projected.append((relative[0], relative[1]))

    projected_extent = max(
        max(p[0] for p in projected) - min(p[0] for p in projected),
        max(p[1] for p in projected) - min(p[1] for p in projected)
    )

    if not math.isfinite(projected_extent) or projected_extent <= 0:
        return "degenerate projection"

    for i in range(len(projected)):
        i_next = (i + 1) % len(projected)

        for j in range(i + 1, len(projected)):
            j_next = (j + 1) % len(projected)

            if i == j or i_next == j or j_next == i:
                continue

            if segments_intersect(
                projected[i], projected[i_next],
                projected[j], projected[j_next]
            ):
                return (
                    f"self-intersection: edge {i}-{i_next} "
                    f"crosses edge {j}-{j_next}"
                )

    signed_area = 0.0

    for i in range(1, len(projected) - 1):
        signed_area += cross2(projected[0], projected[i], projected[i + 1])

    if not math.isfinite(signed_area) or abs(signed_area) <= AREA_EPS:
        return "zero projected area"

    return None

print("Reading OBJ:", obj_path)

try:
    with open(obj_path, "r", encoding="utf-8", errors="replace") as f:
        for line_number, line in enumerate(f, 1):
            stripped = line.strip()

            if not stripped or stripped.startswith("#"):
                continue

            parts = stripped.split()

            if parts[0] == "v":
                if len(parts) < 4:
                    issues.append({
                        "line": line_number,
                        "type": "invalid vertex declaration",
                        "face": stripped
                    })
                    continue

                try:
                    vertices.append(tuple(float(x) for x in parts[1:4]))
                except ValueError:
                    issues.append({
                        "line": line_number,
                        "type": "invalid vertex coordinates",
                        "face": stripped
                    })

            elif parts[0] == "f":
                face_count += 1
                indices = []
                invalid = None

                for token in parts[1:]:
                    try:
                        raw = int(token.split("/")[0])

                        if raw == 0:
                            raise ValueError("zero index")

                        index = raw - 1 if raw > 0 else len(vertices) + raw

                        if index < 0 or index >= len(vertices):
                            raise IndexError(token)

                        indices.append(index)

                    except (ValueError, IndexError):
                        invalid = f"invalid vertex index: {token}"
                        break

                if invalid:
                    issues.append({
                        "line": line_number,
                        "type": invalid,
                        "face": stripped
                    })
                    continue

                problem = inspect_face(indices, line_number)

                if problem:
                    issues.append({
                        "line": line_number,
                        "type": problem,
                        "face": stripped
                    })

except OSError as e:
    print("Cannot read OBJ:", e)
    sys.exit(1)

with open(report_path, "w", newline="", encoding="utf-8-sig") as f:
    writer = csv.DictWriter(
        f,
        fieldnames=["line", "type", "face"]
    )
    writer.writeheader()
    writer.writerows(issues)

print()
print("========== OBJ SCAN SUMMARY ==========")
print("Vertices:", len(vertices))
print("Faces:", face_count)
print("Abnormal faces:", len(issues))
print("Report:", report_path)

counts = {}

for item in issues:
    category = item["type"].split(":")[0]
    counts[category] = counts.get(category, 0) + 1

for category, count in sorted(counts.items()):
    print(f"{category}: {count}")

print()
print("First 30 abnormal faces:")

for item in issues[:30]:
    print(
        f'Line {item["line"]}: {item["type"]}\n'
        f'    {item["face"]}'
    )

print()
print("Full report saved to:", report_path)
