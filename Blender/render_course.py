"""
Render a Gem Roller course exactly as the game would assemble it.

The C++ course generator (Source/GemRoller/GRLayout.h) can dump any seed as JSON; this
script rebuilds that course out of the Blender-made assets and renders it. It is a quick
way to eyeball a layout without opening Unreal.

    g++ -std=c++17 -I Source/GemRoller Tools/LayoutTests/layout_dump.cpp -o /tmp/layout_dump
    /tmp/layout_dump 2024 > /tmp/course.json
    python Blender/render_course.py /tmp/course.json Assets/Preview/course_seed2024.jpg

Unreal units are centimetres, Blender's are metres, hence the /100.
"""

import json
import math
import os
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import generate_assets as g  # noqa: E402

CYAN = g.hexcol("#38E8FF")
PINK = g.hexcol("#FF5AD6")
GOLD = g.hexcol("#FFD54A")
ORANGE = g.hexcol("#FFB000")
MINT = g.hexcol("#7DFF9B")


def m(v):
    return (v[0] / 100.0, v[1] / 100.0, v[2] / 100.0)


def build(data, col):
    mk = lambda name, loc, yaw=0.0, scale=1.0, tint=None: g.make_object(  # noqa: E731
        name, col, m(loc), math.radians(yaw), scale, tint)

    for i, isl in enumerate(data["islands"]):
        c = isl["c"]
        s = isl["scale"]
        mk("SM_Island", c, isl["yaw"], s)
        mk("SM_Rock_" + "ABC"[isl["rock"]], [c[0], c[1], c[2] - 50.0 * s], isl["yaw"], s)
        for t in isl["trees"]:
            mk("SM_Tree", t, i * 37.0, 0.8)
        mk("SM_Checkpoint", isl["pole"], 0.0, 1.0, MINT if i == 0 else None)
        if isl["spinner"]:
            mk("SM_Spinner", c, 0.0, 1.0)
    for ln in data["links"]:
        if ln["kind"] == "pad":
            mk("SM_JumpPad", ln["pad"], 0.0, 1.0, PINK)
        elif ln["kind"] == "stones":
            for st in ln["stones"]:
                mk("SM_StepStone", st)
        else:
            a, b = ln["a"], ln["b"]
            mk("SM_StepStone", a, 0.0, 1.8)
            mk("SM_StepStone", b, 0.0, 1.8, (1, 1, 1, 0.5))
    for k, gem in enumerate(data["gems"]):
        if gem["gold"]:
            mk("SM_Gem", gem["p"], k * 17.0, 1.5, GOLD)
        else:
            mk("SM_Gem", gem["p"], k * 17.0, 1.0, CYAN if k % 2 == 0 else PINK)
    for cr in data["crystals"]:
        mk("SM_TimeCrystal", cr)
    for b in data["boost"]:
        mk("SM_BoostPad", b["p"], b["yaw"], 1.0, ORANGE)
    for r in data["rocks"]:
        mk("SM_Rock_" + "ABC"[r["v"]], r["p"], r["yaw"], r["s"])
    for c in data["clouds"]:
        mk("SM_Cloud_" + "AB"[c["v"]], c["p"], c["yaw"], c["s"])
    mk("SM_Marble", data["start"])
    ring = mk("SM_Goal", data["goal"])
    ring.rotation_euler = (math.pi / 2, 0.0, math.radians(data["goalYaw"]) + math.pi / 2)


def main(argv):
    src, out = argv[0], argv[1]
    samples = int(argv[2]) if len(argv) > 2 else 32
    with open(src) as f:
        data = json.load(f)

    sc = g.reset_scene()
    col = bpy.data.collections.new("Course")
    sc.collection.children.link(col)
    build(data, col)

    pts = [Vector(m(i["c"])) for i in data["islands"]]
    centre = sum(pts, Vector()) / len(pts)
    span = max((p - centre).length for p in pts)
    g.setup_render(sc, 1600, 900, samples)
    # look from behind and above the start, down the course
    first, last = pts[0], pts[-1]
    forward = (last - first)
    forward.z = 0
    forward.normalize()
    side = Vector((-forward.y, forward.x, 0))
    cam_loc = centre - forward * span * 1.15 + side * span * 0.55 + Vector((0, 0, span * 0.95 + centre.z))
    g.look_camera(sc, cam_loc, centre + Vector((0, 0, -2.0)), lens=28)
    sc.render.filepath = out
    bpy.ops.render.render(write_still=True)
    print("rendered", out)


if __name__ == "__main__":
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    main(argv)
