"""
Gem Roller - procedural asset generator for Blender.

Builds every mesh used by the Unreal project from code (no manual modelling),
paints flat vertex colours, and exports one FBX per asset.

Run it either way:

    blender -b -P Blender/generate_assets.py -- [options]      # Blender installed
    python Blender/generate_assets.py [options]                 # `pip install bpy`

Options:
    --out DIR        FBX output directory          (default: Assets/FBX)
    --blend FILE     also save a showcase .blend   (default: Blender/GemRoller_Assets.blend)
    --preview DIR    also render preview PNGs      (default: off)
    --no-fbx         skip the FBX export

Conventions (they matter for the Unreal import):
  * 1 Blender unit = 1 metre. FBX is exported with "FBX Units Scale" so the
    meshes arrive in Unreal at the right size (1 m = 100 cm).
  * Blender +X maps to Unreal +X, so "forward" for a directional mesh is +X.
  * Colours live in a float colour attribute named "Col" and are exported as
    LINEAR values, which is how Unreal's VertexColor node reads them.
  * Walkable surfaces have their top at z = 0 so Unreal can place them by
    their surface height.
"""

import argparse
import math
import os
import random
import sys

import bpy
import bmesh
from mathutils import Vector, Matrix

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

# --------------------------------------------------------------------------
# Palette (authored as sRGB hex, stored linear)
# --------------------------------------------------------------------------


def hexcol(h, a=1.0):
    h = h.lstrip("#")
    out = []
    for i in (0, 2, 4):
        c = int(h[i:i + 2], 16) / 255.0
        out.append(c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4)
    return (out[0], out[1], out[2], a)


C = {
    "lawn": hexcol("#7FD36B"),
    "lawn_light": hexcol("#B6EE86"),
    "trim": hexcol("#FFD166"),
    "wall": hexcol("#C98B57"),
    "wall_dark": hexcol("#8E5E3C"),
    "under": hexcol("#6B4A3A"),
    "rock_a": hexcol("#A98467"),
    "rock_b": hexcol("#7C5A47"),
    "rock_c": hexcol("#5E4638"),
    "stone_top": hexcol("#5CD6C0"),
    "stone_rim": hexcol("#F4FBFF"),
    "stone_side": hexcol("#3E8F94"),
    "ball_a": hexcol("#FF5E57"),
    "ball_b": hexcol("#FFF1D6"),
    "ball_pole": hexcol("#2F4BFF"),
    "white": hexcol("#FFFFFF"),
    "gem_hi": hexcol("#FFFFFF"),
    "gem_mid": hexcol("#E6E6E6"),
    "gem_lo": hexcol("#BDBDBD"),
    "glass": hexcol("#7FE9FF"),
    "gold": hexcol("#FFC933"),
    "hazard_y": hexcol("#FFC400"),
    "hazard_k": hexcol("#1B1B24"),
    "hub": hexcol("#E63946"),
    "pad_ring": hexcol("#26324A"),
    "pad_top": hexcol("#FFFFFF"),
    "pole": hexcol("#E9EEF2"),
    "pole_base": hexcol("#5C6B7A"),
    "trunk": hexcol("#7A4B2A"),
    "leaf_a": hexcol("#3FBF5A"),
    "leaf_b": hexcol("#2E9E4B"),
    "cloud": hexcol("#FFFFFF"),
    "cloud_shade": hexcol("#CFE0FF"),
}


def mul(c, t):
    return (c[0] * t[0], c[1] * t[1], c[2] * t[2], c[3])


# --------------------------------------------------------------------------
# bmesh helpers
# --------------------------------------------------------------------------

COLOR_LAYER = "Col"


class MeshBuilder:
    """Thin wrapper over a bmesh that paints a colour per face."""

    def __init__(self):
        self.bm = bmesh.new()
        self.col = self.bm.loops.layers.float_color.new(COLOR_LAYER)

    def face(self, verts, color, smooth=False):
        try:
            f = self.bm.faces.new(verts)
        except ValueError:  # duplicate face (degenerate profile points)
            return None
        f.smooth = smooth
        for loop in f.loops:
            loop[self.col] = color
        return f

    def lathe(self, profile, segs, colors, smooth=True, closed_profile=False,
              start_angle=0.0, jitter=None, offset=(0.0, 0.0, 0.0)):
        """Revolve a (radius, z) profile around the Z axis.

        profile  -- list of (r, z); r == 0 makes a pole (triangle fan)
        colors   -- one colour per profile segment (n-1, or n when closed_profile)
        jitter   -- optional callable(r, z, angle_index) -> (r', z') for rocks
        """
        n = len(profile)
        rings = []
        for pi, (r, z) in enumerate(profile):
            if r <= 1e-6:
                rr, zz = (r, z)
                if jitter:
                    _, zz = jitter(r, z, -1)
                rings.append([self.bm.verts.new((offset[0], offset[1], zz + offset[2]))])
            else:
                ring = []
                for k in range(segs):
                    a = start_angle + 2.0 * math.pi * k / segs
                    rr, zz = (r, z)
                    if jitter:
                        rr, zz = jitter(r, z, k + pi * 131)
                    ring.append(self.bm.verts.new((offset[0] + rr * math.cos(a),
                                                   offset[1] + rr * math.sin(a),
                                                   zz + offset[2])))
                rings.append(ring)
        count = n if closed_profile else n - 1
        for i in range(count):
            A, B, col = rings[i], rings[(i + 1) % n], colors[i]
            for k in range(segs):
                k2 = (k + 1) % segs
                if len(A) == 1 and len(B) == 1:
                    continue
                if len(A) == 1:
                    self.face([A[0], B[k2], B[k]], col, smooth)
                elif len(B) == 1:
                    self.face([A[k], A[k2], B[0]], col, smooth)
                else:
                    self.face([A[k], A[k2], B[k2], B[k]], col, smooth)

    def box(self, center, size, color_top, color_side=None):
        cx, cy, cz = center
        sx, sy, sz = size[0] / 2, size[1] / 2, size[2] / 2
        res = bmesh.ops.create_cube(self.bm, size=1.0)
        verts = res["verts"]
        for v in verts:
            v.co = (cx + v.co.x * 2 * sx, cy + v.co.y * 2 * sy, cz + v.co.z * 2 * sz)
        faces = {f for v in verts for f in v.link_faces}
        for f in faces:
            top = f.normal.z > 0.5
            c = color_top if top or color_side is None else color_side
            f.smooth = False
            for loop in f.loops:
                loop[self.col] = c

    def prism(self, outline, z0, z1, color_top, color_side):
        """Extrude a 2D outline (list of (x, y), counter-clockwise) between z0 and z1."""
        bot = [self.bm.verts.new((x, y, z0)) for x, y in outline]
        top = [self.bm.verts.new((x, y, z1)) for x, y in outline]
        n = len(outline)
        self.face(top, color_top)
        self.face(list(reversed(bot)), color_side)
        for i in range(n):
            j = (i + 1) % n
            self.face([bot[i], bot[j], top[j], top[i]], color_side)

    def finish(self, name, material, tint=None):
        bm = self.bm
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
        # Planar UVs: harmless fallback so Unreal never complains about a missing UV set.
        uv = bm.loops.layers.uv.new("UVMap")
        for f in bm.faces:
            for loop in f.loops:
                loop[uv].uv = (loop.vert.co.x * 0.25 + 0.5, loop.vert.co.y * 0.25 + 0.5)
        if tint is not None:
            for f in bm.faces:
                for loop in f.loops:
                    loop[self.col] = mul(tuple(loop[self.col]), tint)
        me = bpy.data.meshes.new(name)
        bm.to_mesh(me)
        bm.free()
        me.materials.append(material)
        return me


def get_material():
    mat = bpy.data.materials.get("M_GR")
    if mat:
        return mat
    mat = bpy.data.materials.new("M_GR")
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = nt.nodes.get("Principled BSDF")
    vc = nt.nodes.new("ShaderNodeVertexColor")
    vc.layer_name = COLOR_LAYER
    nt.links.new(vc.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.55
    return mat


# --------------------------------------------------------------------------
# Asset recipes. Each returns a MeshBuilder; sizes are in metres.
# --------------------------------------------------------------------------


def build_marble():
    """Beach-ball marble, r = 0.5 m, centred. The stripes make the roll readable."""
    mb = MeshBuilder()
    rings, segs, sectors = 14, 24, 6
    prof = [(0.5 * math.sin(math.pi * i / rings), 0.5 * math.cos(math.pi * i / rings))
            for i in range(rings + 1)]
    mb.lathe(prof, segs, [C["ball_b"]] * rings, smooth=True)
    for f in mb.bm.faces:
        c = f.calc_center_median()
        sector = int((math.atan2(c.y, c.x) % (2 * math.pi)) / (2 * math.pi) * sectors) % sectors
        col = C["ball_a"] if sector % 2 == 0 else C["ball_b"]
        if abs(c.z) > 0.48:  # the pole fans
            col = C["ball_pole"]
        for loop in f.loops:
            loop[mb.col] = col
    return mb


def build_island():
    """Round floating-island slab. Top at z=0, radius 3.5 m, 0.6 m thick."""
    mb = MeshBuilder()
    prof = [(0.0, 0.0), (2.6, 0.0), (2.75, 0.0), (3.3, 0.0), (3.5, -0.10),
            (3.5, -0.42), (3.2, -0.6), (0.0, -0.6)]
    cols = [C["lawn"], C["lawn_light"], C["lawn"], C["trim"], C["wall"], C["wall_dark"], C["under"]]
    # profile has 8 points -> 7 segments
    mb.lathe(prof, 32, cols, smooth=False)
    return mb


def build_step_stone():
    """Small round stepping stone, r = 1.2 m, top at z=0, 0.5 m thick."""
    mb = MeshBuilder()
    prof = [(0.0, 0.0), (0.85, 0.0), (1.1, 0.0), (1.2, -0.08), (1.2, -0.3), (0.95, -0.5), (0.0, -0.5)]
    cols = [C["stone_top"], C["stone_rim"], C["stone_rim"], C["stone_side"], C["stone_side"], C["stone_side"]]
    mb.lathe(prof, 24, cols, smooth=False)
    return mb


def build_gem():
    """Faceted gem, centred, 0.7 m tall. Vertex colours are greys; Unreal tints it."""
    mb = MeshBuilder()
    prof = [(0.0, 0.40), (0.17, 0.40), (0.34, 0.14), (0.34, 0.06), (0.0, -0.42)]
    cols = [C["gem_hi"], C["gem_mid"], C["gem_mid"], C["gem_lo"]]
    mb.lathe(prof, 8, cols, smooth=False)
    return mb


def build_time_crystal():
    """Hourglass pickup, centred, 1.0 m tall."""
    mb = MeshBuilder()
    prof = [(0.0, 0.5), (0.32, 0.5), (0.32, 0.42), (0.28, 0.40), (0.07, 0.04),
            (0.07, -0.04), (0.28, -0.40), (0.32, -0.42), (0.32, -0.5), (0.0, -0.5)]
    cols = [C["gold"], C["gold"], C["gold"], C["glass"], C["glass"], C["glass"], C["gold"], C["gold"], C["gold"]]
    mb.lathe(prof, 12, cols, smooth=True)
    return mb


def build_spinner():
    """Hazard bar. Hub base at z=0; arm 4.4 m long, 0.5 m tall. Spins about Z."""
    mb = MeshBuilder()
    n = 11
    seg = 4.4 / n
    for i in range(n):
        x = -2.2 + seg * (i + 0.5)
        col = C["hazard_y"] if i % 2 == 0 else C["hazard_k"]
        mb.box((x, 0, 0.25), (seg, 0.38, 0.5), col, col)
    mb.lathe([(0.0, 0.8), (0.5, 0.8), (0.55, 0.7), (0.55, 0.0), (0.0, 0.0)], 16,
             [C["hub"], C["hub"], C["hub"], C["hub"]], smooth=True)
    return mb


def build_jump_pad():
    """Bouncy dome pad, r = 1.0 m, flush with the ground."""
    mb = MeshBuilder()
    prof = [(0.0, 0.34), (0.2, 0.32), (0.45, 0.24), (0.62, 0.14), (0.7, 0.12),
            (1.0, 0.10), (1.0, 0.0), (0.0, 0.0)]
    cols = [C["pad_top"], C["pad_top"], C["pad_top"], C["pad_ring"], C["pad_ring"], C["pad_ring"], C["pad_ring"]]
    mb.lathe(prof, 20, cols, smooth=True)
    return mb


def build_boost_pad():
    """Double-chevron arrow pointing +X, 0.1 m thick, flush with the ground."""
    mb = MeshBuilder()
    for dx in (-0.55, 0.45):
        # the chevron is concave, so build it from two convex parallelograms
        a = [(dx - 0.5, -0.9), (dx, -0.9), (dx + 0.5, 0.0), (dx, 0.0)]
        b = [(dx, 0.0), (dx + 0.5, 0.0), (dx, 0.9), (dx - 0.5, 0.9)]
        mb.prism(a, 0.0, 0.1, C["white"], C["white"])
        mb.prism(b, 0.0, 0.1, C["white"], C["white"])
    return mb


def build_checkpoint():
    """Flag on a pole. Base at z=0, 2.5 m tall; flag points +X."""
    mb = MeshBuilder()
    mb.lathe([(0.0, 0.15), (0.35, 0.15), (0.4, 0.08), (0.4, 0.0), (0.0, 0.0)], 16,
             [C["pole_base"]] * 4, smooth=False)
    mb.lathe([(0.0, 2.55), (0.09, 2.5), (0.06, 2.4), (0.06, 0.15), (0.0, 0.15)], 10,
             [C["gold"], C["pole"], C["pole"], C["pole"]], smooth=True)
    flag = [(0.06, 2.35), (0.06, 1.65), (1.1, 2.0)]
    # flag outline lives in XZ; build it as a thin prism along Y
    t = 0.05
    top = [mb.bm.verts.new((x, t, z)) for x, z in flag]
    bot = [mb.bm.verts.new((x, -t, z)) for x, z in flag]
    mb.face(list(reversed(top)), C["white"])
    mb.face(bot, C["white"])
    for i in range(3):
        j = (i + 1) % 3
        mb.face([bot[i], bot[j], top[j], top[i]], C["white"])
    return mb


def build_goal():
    """Finish ring: torus in the XY plane (Unreal stands it up), R = 2.0 m, r = 0.28 m."""
    mb = MeshBuilder()
    major, minor, tube = 2.0, 0.28, 12
    prof = []
    cols = []
    for i in range(tube):
        a = 2 * math.pi * i / tube
        prof.append((major + minor * math.cos(a), minor * math.sin(a)))
        cols.append(C["gold"] if i % 2 == 0 else C["white"])
    mb.lathe(prof, 40, cols, smooth=True, closed_profile=True)
    # checker the ring: recolour alternate segments around the loop
    col = mb.col
    for f in mb.bm.faces:
        c = f.calc_center_median()
        ang = math.atan2(c.y, c.x)
        stripe = int((ang + math.pi) / (2 * math.pi) * 20) % 2
        base = C["gold"] if stripe == 0 else C["white"]
        for loop in f.loops:
            loop[col] = base
    return mb


def _rock(seed):
    """Floating-island underside: grassy cap at z=0, tapering rocky body below."""
    rng = random.Random(seed)
    mb = MeshBuilder()
    prof = [(0.0, 0.0), (3.1, 0.0), (3.15, -0.3), (2.7, -1.2), (2.2, -2.3), (1.6, -3.5),
            (1.1, -4.6), (0.6, -5.6), (0.0, -6.3)]
    cols = [C["lawn"], C["wall"], C["rock_a"], C["rock_a"], C["rock_b"], C["rock_b"], C["rock_c"], C["rock_c"]]

    def jitter(r, z, idx):
        if idx < 0 or z > -0.2:
            return r, z + (rng.uniform(-0.12, 0.12) if idx >= 0 and z < -0.01 else 0.0)
        return r * rng.uniform(0.78, 1.18), z + rng.uniform(-0.45, 0.45)
    mb.lathe(prof, 14, cols, smooth=False, start_angle=rng.uniform(0, 1), jitter=jitter)
    return mb


def build_rock_a():
    return _rock(11)


def build_rock_b():
    return _rock(23)


def build_rock_c():
    return _rock(37)


def build_tree():
    """Chunky low-poly tree, base at z=0, 3 m tall."""
    mb = MeshBuilder()
    mb.lathe([(0.0, 0.0), (0.22, 0.0), (0.16, 0.9), (0.0, 0.9)], 8, [C["trunk"]] * 3, smooth=False)
    mb.lathe([(0.0, 3.0), (0.95, 1.5), (0.0, 1.5)], 8, [C["leaf_a"], C["leaf_b"]], smooth=False)
    mb.lathe([(0.0, 2.2), (1.2, 0.8), (0.0, 0.8)], 8, [C["leaf_b"], C["leaf_a"]], smooth=False)
    return mb


def _cloud(seed):
    rng = random.Random(seed)
    mb = MeshBuilder()
    blobs = 6
    for i in range(blobs):
        x = (i - (blobs - 1) / 2) * 3.2 + rng.uniform(-0.8, 0.8)
        r = rng.uniform(2.4, 4.0) * (1.25 - abs(i - (blobs - 1) / 2) * 0.18)
        y = rng.uniform(-1.5, 1.5)
        z = rng.uniform(-0.3, 0.9)
        prof = []
        for k in range(9):
            phi = math.pi * k / 8
            prof.append((r * math.sin(phi), r * 0.55 * math.cos(phi)))
        cols = [C["cloud"] if prof[k][1] >= -0.4 else C["cloud_shade"] for k in range(8)]
        mb.lathe(prof, 14, cols, smooth=True, offset=(x, y, z))
    return mb


def build_cloud_a():
    return _cloud(5)


def build_cloud_b():
    return _cloud(17)


# name -> builder. Order is the showcase order.
ASSETS = [
    ("SM_Marble", build_marble),
    ("SM_Island", build_island),
    ("SM_StepStone", build_step_stone),
    ("SM_Gem", build_gem),
    ("SM_TimeCrystal", build_time_crystal),
    ("SM_Spinner", build_spinner),
    ("SM_JumpPad", build_jump_pad),
    ("SM_BoostPad", build_boost_pad),
    ("SM_Checkpoint", build_checkpoint),
    ("SM_Goal", build_goal),
    ("SM_Rock_A", build_rock_a),
    ("SM_Rock_B", build_rock_b),
    ("SM_Rock_C", build_rock_c),
    ("SM_Tree", build_tree),
    ("SM_Cloud_A", build_cloud_a),
    ("SM_Cloud_B", build_cloud_b),
]
BUILDERS = dict(ASSETS)


def make_object(name, collection, location=(0, 0, 0), rot_z=0.0, scale=1.0, tint=None, obj_name=None):
    mat = get_material()
    me = BUILDERS[name]().finish(obj_name or name, mat, tint)
    ob = bpy.data.objects.new(obj_name or name, me)
    collection.objects.link(ob)
    ob.location = location
    ob.rotation_euler = (0, 0, rot_z)
    ob.scale = (scale, scale, scale) if isinstance(scale, (int, float)) else scale
    return ob


# --------------------------------------------------------------------------
# Scene / export / preview
# --------------------------------------------------------------------------


def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.unit_settings.system = "METRIC"
    sc.unit_settings.scale_length = 1.0
    return sc


def export_all(out_dir):
    os.makedirs(out_dir, exist_ok=True)
    sc = bpy.context.scene
    col = bpy.data.collections.new("Export")
    sc.collection.children.link(col)
    written = []
    for name, _ in ASSETS:
        ob = make_object(name, col)
        bpy.ops.object.select_all(action="DESELECT")
        ob.select_set(True)
        bpy.context.view_layer.objects.active = ob
        path = os.path.join(out_dir, name + ".fbx")
        bpy.ops.export_scene.fbx(
            filepath=path,
            use_selection=True,
            object_types={"MESH"},
            apply_scale_options="FBX_SCALE_UNITS",   # 1 BU (m) -> 100 UU (cm) in Unreal
            global_scale=1.0,
            axis_forward="-Y",
            axis_up="Z",
            use_mesh_modifiers=True,
            mesh_smooth_type="FACE",
            colors_type="LINEAR",
            use_triangles=True,
            use_tspace=False,
            add_leaf_bones=False,
            bake_anim=False,
            path_mode="STRIP",
        )
        written.append(path)
        bpy.data.objects.remove(ob, do_unlink=True)
    bpy.data.collections.remove(col)
    return written


SHOWCASE_LAYOUT = {
    # name: (x, y, z, scale)  -- small gameplay items on the front rows, big ones behind
    "SM_Marble": (0.0, 0.0, 0.5, 1.0),
    "SM_Gem": (2.6, 0.0, 0.6, 1.0),
    "SM_TimeCrystal": (5.0, 0.0, 0.6, 1.0),
    "SM_JumpPad": (8.0, 0.0, 0.0, 1.0),
    "SM_BoostPad": (11.6, 0.0, 0.0, 1.0),
    "SM_Checkpoint": (15.0, 0.0, 0.0, 1.0),
    "SM_Tree": (18.0, 0.0, 0.0, 1.0),
    "SM_Spinner": (2.5, 6.0, 0.0, 1.0),
    "SM_StepStone": (8.5, 6.0, 0.0, 1.0),
    "SM_Goal": (15.0, 6.0, 2.3, 1.0),
    "SM_Island": (4.0, 17.0, 0.0, 1.0),
    "SM_Rock_A": (13.0, 17.0, 0.0, 0.8),
    "SM_Rock_B": (20.0, 17.0, 0.0, 0.8),
    "SM_Rock_C": (27.0, 17.0, 0.0, 0.8),
    "SM_Cloud_A": (8.0, 32.0, 2.0, 0.6),
    "SM_Cloud_B": (28.0, 32.0, 2.0, 0.6),
}


def build_showcase(sc):
    """Every asset, laid out in a grid, for the .blend file and the contact-sheet render."""
    col = bpy.data.collections.new("Showcase")
    sc.collection.children.link(col)
    objs = {}
    for name, _ in ASSETS:
        x, y, z, s = SHOWCASE_LAYOUT[name]
        ob = make_object(name, col, (x, y, z), 0.0, s)
        if name == "SM_Goal":
            ob.rotation_euler = (math.pi / 2, 0, 0)  # Unreal stands the ring up the same way
        objs[name] = ob
    return objs


def setup_render(sc, w, h, samples):
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = samples
    try:
        sc.cycles.use_denoising = True
    except Exception:
        pass
    sc.render.resolution_x = w
    sc.render.resolution_y = h
    sc.render.image_settings.file_format = "JPEG"
    sc.render.image_settings.quality = 90
    sc.view_settings.view_transform = "Standard"
    world = bpy.data.worlds.new("Sky")
    world.use_nodes = True
    bg = world.node_tree.nodes["Background"]
    bg.inputs["Color"].default_value = hexcol("#8FC9FF")
    bg.inputs["Strength"].default_value = 1.1
    sc.world = world
    sun = bpy.data.lights.new("Sun", "SUN")
    sun.energy = 4.0
    sun.angle = math.radians(6)
    so = bpy.data.objects.new("Sun", sun)
    sc.collection.objects.link(so)
    so.rotation_euler = (math.radians(50), math.radians(10), math.radians(120))


def look_camera(sc, loc, target, lens=35.0):
    cam = bpy.data.cameras.new("Cam")
    cam.lens = lens
    co = bpy.data.objects.new("Cam", cam)
    sc.collection.objects.link(co)
    co.location = loc
    direction = Vector(target) - Vector(loc)
    co.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    sc.camera = co
    return co


def render_previews(out_dir, samples=40):
    os.makedirs(out_dir, exist_ok=True)

    # contact sheets: small gameplay items, then the big scenery
    small = {"SM_Marble", "SM_Gem", "SM_TimeCrystal", "SM_JumpPad", "SM_BoostPad",
             "SM_Checkpoint", "SM_Tree", "SM_Spinner", "SM_StepStone", "SM_Goal"}
    shots = [
        ("assets_small.jpg", small, (8.5, -16.0, 8.5), (8.5, 3.0, 1.0), 30.0),
        ("assets_scenery.jpg", set(SHOWCASE_LAYOUT) - small, (17.0, -18.0, 12.0), (17.0, 22.0, -1.0), 38.0),
    ]
    for fname, names, cam_loc, cam_target, lens in shots:
        sc = reset_scene()
        objs = build_showcase(sc)
        for n, ob in objs.items():
            ob.hide_render = n not in names
        setup_render(sc, 1500, 660, samples)
        look_camera(sc, cam_loc, cam_target, lens=lens)
        sc.render.filepath = os.path.join(out_dir, fname)
        bpy.ops.render.render(write_still=True)


def main(argv):
    p = argparse.ArgumentParser(description="Generate Gem Roller assets")
    p.add_argument("--out", default=os.path.join(ROOT, "Assets", "FBX"))
    p.add_argument("--blend", default=os.path.join(HERE, "GemRoller_Assets.blend"))
    p.add_argument("--preview", default=None)
    p.add_argument("--samples", type=int, default=40)
    p.add_argument("--no-fbx", action="store_true")
    args = p.parse_args(argv)

    sc = reset_scene()
    if not args.no_fbx:
        files = export_all(args.out)
        print("Exported %d FBX files to %s" % (len(files), args.out))
    if args.blend:
        reset_scene()
        build_showcase(bpy.context.scene)
        bpy.ops.wm.save_as_mainfile(filepath=args.blend, compress=True)
        print("Saved", args.blend)
    if args.preview:
        render_previews(args.preview, args.samples)
        print("Rendered previews to", args.preview)


if __name__ == "__main__":
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    main(argv)
