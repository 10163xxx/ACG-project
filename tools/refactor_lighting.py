
from pathlib import Path
import re

path = Path("src/integrator/path_tracer.h")
text = path.read_text(encoding="utf-8")

methods = [
    "collect_light_indices",
    "light_solid_angle_pdf_for_hit",
    "estimate_volume_point_lighting",
    "estimate_volume_environment_lighting",
    "estimate_volume_area_lighting",
    "estimate_environment_lighting",
    "estimate_point_lighting",
    "estimate_direct_lighting",
    "estimate_direct_lighting_single",
]

def extract_method(source, name):
    match = re.search(
        rf"^[ \t]*static\s+[\w:<>]+\s+{name}\s*\(",
        source,
        re.MULTILINE,
    )
    if not match:
        raise RuntimeError(f"Cannot find method: {name}")

    start = match.start()
    brace = source.find("{", match.end())
    if brace < 0:
        raise RuntimeError(f"Cannot find method body: {name}")

    depth = 0
    for i in range(brace, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                end = i + 1
                while end < len(source) and source[end] in " \t":
                    end += 1
                if end < len(source) and source[end] == "\n":
                    end += 1
                return start, end, source[start:end].strip()

    raise RuntimeError(f"Unclosed method body: {name}")

extracted = []

for name in methods:
    start, end, body = extract_method(text, name)
    extracted.append((name, start, end, body))

# Remove methods from the original file, from bottom to top.
for name, start, end, body in sorted(
    extracted, key=lambda item: item[1], reverse=True
):
    text = text[:start] + text[end:]

# Rewrite calls remaining in PathTracer.
for name in methods:
    text = re.sub(
        rf"(?<![\w:]){name}\s*\(",
        f"DirectLighting::{name}(",
        text,
    )

include_line = '#include "integrator/direct_lighting.h"'
if include_line not in text:
    text = text.replace(
        '#include "integrator/mis.h"',
        '#include "integrator/mis.h"\n' + include_line,
        1,
    )

header = """#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "core/ray.h"
#include "core/interval.h"
#include "core/vec3.h"
#include "core/random.h"
#include "geometry/hittable.h"
#include "geometry/hittable_list.h"
#include "materials/point_light.h"
#include "materials/material.h"
#include "materials/environment_map.h"
#include "integrator/mis.h"

class DirectLighting {
public:
"""

# Preserve the original method implementations and their indentation.
header += "\n\n".join(body for _, _, _, body in extracted)
header += "\n};\n"

Path("src/integrator/direct_lighting.h").write_text(
    header, encoding="utf-8"
)
path.write_text(text, encoding="utf-8")

print("Created src/integrator/direct_lighting.h")
print("Updated src/integrator/path_tracer.h")
