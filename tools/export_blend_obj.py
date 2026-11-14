"""Blender headless OBJ export for ACG Path Tracer assets.

Run:
    blender.exe -b <model.blend> --python tools/export_blend_obj.py -- <out.obj>

Exports every mesh object (modifiers applied, world transforms baked)
as one Y-up OBJ with UVs, normals and a companion MTL, then prints a
report of object names, bounding boxes and materials for scene
integration.
"""

import sys

import bpy


def report() -> None:
    print("=== OBJECT REPORT ===")

    for obj in bpy.data.objects:
        if obj.type != "MESH":
            continue

        world = obj.matrix_world
        corners = [
            world @ obj.matrix_world @ __import__("mathutils").Vector(c)
            for c in obj.bound_box
        ]

        xs = [c.x for c in corners]
        ys = [c.y for c in corners]
        zs = [c.z for c in corners]

        materials = [
            slot.material.name if slot.material else "(none)"
            for slot in obj.material_slots
        ]

        print(
            f"OBJECT {obj.name!r} "
            f"verts={len(obj.data.vertices)} "
            f"materials={materials}"
        )

        print(
            f"  BBOX blender-space x[{min(xs):.4f},{max(xs):.4f}] "
            f"y[{min(ys):.4f},{max(ys):.4f}] z[{min(zs):.4f},{max(zs):.4f}]"
        )

    print("=== MATERIAL REPORT ===")

    for material in bpy.data.materials:
        if not material.use_nodes:
            continue

        base_color = (1.0, 1.0, 1.0)
        alpha_mode = "OPAQUE"
        textures = []

        for node in material.node_tree.nodes:
            if node.type == "BSDF_PRINCIPLED":
                base_color = tuple(node.inputs["Base Color"].default_value)
                alpha_mode = node.inputs["Alpha"].linked
                for input_socket in ("Base Color", "Alpha", "Roughness", "Normal"):
                    if node.inputs[input_socket].linked:
                        source = node.inputs[input_socket].links[0].from_node
                        if source.type == "TEX_IMAGE" and source.image:
                            textures.append(
                                f"{input_socket}={source.image.filepath or source.image.name}"
                            )

        print(
            f"MATERIAL {material.name!r} "
            f"base_color=({base_color[0]:.3f},{base_color[1]:.3f},{base_color[2]:.3f}) "
            f"textures={textures}"
        )


def export(output_path: str, name_filter: str) -> None:
    if name_filter:
        for obj in list(bpy.data.objects):
            if obj.type != "MESH" or name_filter not in obj.name:
                bpy.data.objects.remove(obj, do_unlink=True)

    bpy.ops.wm.obj_export(
        filepath=output_path,
        export_selected_objects=False,
        apply_modifiers=True,
        export_materials=True,
        export_uv=True,
        export_normals=True,
        forward_axis="NEGATIVE_Z",
        up_axis="Y",
    )

    print(f"EXPORTED {output_path}")


def main() -> None:
    argv = sys.argv[sys.argv.index("--") + 1:]
    export(argv[0], argv[1] if len(argv) > 1 else "")
    report()


main()
