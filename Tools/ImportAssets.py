"""
Gem Roller - one-step asset import for the Unreal Editor.

Run it from the editor:   Tools > Execute Python Script...  >  Tools/ImportAssets.py
(or headless:  UnrealEditor-Cmd GemRoller.uproject -run=pythonscript -script=Tools/ImportAssets.py)

What it does
  1. Imports every Assets/FBX/*.fbx (made by Blender/generate_assets.py) as a static mesh in /Game/GemRoller/Meshes.
  2. Imports every Assets/Audio/*.wav (made by Tools/make_sounds.py) as SW_<name> in /Game/GemRoller/Audio.
  3. Builds /Game/GemRoller/Materials/M_GR_Master: vertex colour x Tint, plus Emissive/Roughness/Metallic.
  4. Creates the empty level /Game/GemRoller/Maps/Arena (the game mode builds the whole course at runtime).
  5. Checks that the meshes arrived at the right size and says so loudly if they did not.

Safe to run again: it replaces what it made before.
"""

import glob
import os

import unreal

ASSET_ROOT = "/Game/GemRoller"
MESH_DIR = ASSET_ROOT + "/Meshes"
AUDIO_DIR = ASSET_ROOT + "/Audio"
MATERIAL_DIR = ASSET_ROOT + "/Materials"
MAP_DIR = ASSET_ROOT + "/Maps"

# The FBX files are exported in "FBX units" so 1 Blender metre = 100 cm and no extra scale is needed.
# If the size check at the end says the meshes are 100x too small or large, change this.
IMPORT_UNIFORM_SCALE = 1.0

PROJECT_DIR = unreal.Paths.project_dir()
FBX_DIR = os.path.normpath(os.path.join(PROJECT_DIR, "Assets", "FBX"))
AUDIO_SRC_DIR = os.path.normpath(os.path.join(PROJECT_DIR, "Assets", "Audio"))


def log(msg):
    unreal.log("[GemRoller] " + msg)


def warn(msg):
    unreal.log_warning("[GemRoller] " + msg)


def set_props(obj, **props):
    """set_editor_property for several properties; a property that does not exist on this engine version is skipped."""
    for key, value in props.items():
        try:
            obj.set_editor_property(key, value)
        except Exception as exc:  # noqa: BLE001 - engine versions differ, never fail the whole import over one flag
            warn("  could not set %s (%s)" % (key, exc))


def make_task(filename, dest_dir, dest_name, options=None):
    task = unreal.AssetImportTask()
    set_props(task, filename=filename, destination_path=dest_dir, destination_name=dest_name,
              replace_existing=True, replace_existing_settings=True, automated=True, save=True)
    if options is not None:
        set_props(task, options=options)
    return task


# --------------------------------------------------------------------------------------------------
# 1. meshes
# --------------------------------------------------------------------------------------------------
def fbx_options():
    ui = unreal.FbxImportUI()
    set_props(ui,
              import_mesh=True,
              import_as_skeletal=False,
              import_animations=False,
              import_materials=False,
              import_textures=False,
              mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH)
    set_props(ui.static_mesh_import_data,
              combine_meshes=True,
              auto_generate_collision=False,   # collision comes from engine primitives in code
              generate_lightmap_u_vs=False,    # every light is movable, no lightmaps
              remove_degenerates=True,
              import_uniform_scale=IMPORT_UNIFORM_SCALE,
              vertex_color_import_option=unreal.VertexColorImportOption.REPLACE)
    return ui


def import_meshes():
    files = sorted(glob.glob(os.path.join(FBX_DIR, "*.fbx")))
    if not files:
        warn("No FBX files in %s. Run: python Blender/generate_assets.py" % FBX_DIR)
        return []
    unreal.EditorAssetLibrary.make_directory(MESH_DIR)
    tasks = [make_task(path, MESH_DIR, os.path.splitext(os.path.basename(path))[0], fbx_options()) for path in files]
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    names = [os.path.splitext(os.path.basename(p))[0] for p in files]
    log("Imported %d meshes into %s" % (len(names), MESH_DIR))
    return names


# --------------------------------------------------------------------------------------------------
# 2. audio
# --------------------------------------------------------------------------------------------------
def import_audio():
    files = sorted(glob.glob(os.path.join(AUDIO_SRC_DIR, "*.wav")))
    if not files:
        warn("No WAV files in %s. Run: python Tools/make_sounds.py (the game is silent without them)" % AUDIO_SRC_DIR)
        return
    unreal.EditorAssetLibrary.make_directory(AUDIO_DIR)
    tasks = [make_task(path, AUDIO_DIR, "SW_" + os.path.splitext(os.path.basename(path))[0]) for path in files]
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    log("Imported %d sounds into %s" % (len(files), AUDIO_DIR))


# --------------------------------------------------------------------------------------------------
# 3. master material
# --------------------------------------------------------------------------------------------------
def build_master_material():
    lib = unreal.MaterialEditingLibrary
    name = "M_GR_Master"
    path = MATERIAL_DIR + "/" + name
    unreal.EditorAssetLibrary.make_directory(MATERIAL_DIR)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)

    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, MATERIAL_DIR, unreal.Material, unreal.MaterialFactoryNew())
    if mat is None:
        warn("Could not create %s" % path)
        return

    def node(cls, x, y):
        return lib.create_material_expression(mat, cls, x, y)

    def scalar(label, default, x, y):
        n = node(unreal.MaterialExpressionScalarParameter, x, y)
        set_props(n, parameter_name=label, default_value=default)
        return n

    def link(src, dst, pin):
        if not lib.connect_material_expressions(src, "", dst, pin):
            warn("  could not connect %s" % pin)

    vertex_color = node(unreal.MaterialExpressionVertexColor, -1100, -260)
    vertex_gamma = scalar("VertexGamma", 1.0, -1100, -120)   # raise to 2.2 if the Blender colours look washed out
    gamma = node(unreal.MaterialExpressionPower, -840, -220)
    tint = node(unreal.MaterialExpressionVectorParameter, -840, 0)
    set_props(tint, parameter_name="Tint", default_value=unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    base = node(unreal.MaterialExpressionMultiply, -580, -110)
    emissive = scalar("Emissive", 0.0, -580, 120)
    glow = node(unreal.MaterialExpressionMultiply, -330, 60)
    roughness = scalar("Roughness", 0.5, -330, 220)
    metallic = scalar("Metallic", 0.0, -330, 320)

    link(vertex_color, gamma, "Base")
    link(vertex_gamma, gamma, "Exponent")
    link(gamma, base, "A")
    link(tint, base, "B")
    link(base, glow, "A")
    link(emissive, glow, "B")

    lib.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    lib.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    lib.connect_material_property(metallic, "", unreal.MaterialProperty.MP_METALLIC)

    lib.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    log("Built %s" % path)


# --------------------------------------------------------------------------------------------------
# 4. level
# --------------------------------------------------------------------------------------------------
def create_map():
    asset = MAP_DIR + "/Arena"
    if unreal.EditorAssetLibrary.does_asset_exist(asset):
        log("Map %s already exists" % asset)
        return
    unreal.EditorAssetLibrary.make_directory(MAP_DIR)
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    if world is not None and unreal.EditorLoadingAndSavingUtils.save_map(world, asset):
        log("Created %s (empty on purpose: the game mode builds the course)" % asset)
    else:
        warn("Could not create the map. Any empty level works: open one and press Play.")


# --------------------------------------------------------------------------------------------------
# 5. checks
# --------------------------------------------------------------------------------------------------
EXPECTED_HALF_EXTENT_CM = {   # SM_name -> (x, y, z) half extents of the bounding box at scale 1, in cm
    "SM_Marble": (50.0, 50.0, 50.0),
    "SM_Island": (350.0, 350.0, 30.0),
    "SM_StepStone": (120.0, 120.0, 25.0),
    "SM_Gem": (34.0, 34.0, 41.0),
}


def verify_scale():
    ok = True
    for name, expected in EXPECTED_HALF_EXTENT_CM.items():
        mesh = unreal.EditorAssetLibrary.load_asset("%s/%s" % (MESH_DIR, name))
        if mesh is None:
            warn("  %s did not import" % name)
            ok = False
            continue
        ext = mesh.get_bounds().box_extent
        got = (ext.x, ext.y, ext.z)
        ratio = got[0] / expected[0]
        status = "ok"
        if not 0.9 < ratio < 1.1:
            status = "WRONG SIZE (x%.3g)" % ratio
            ok = False
        log("  %-13s half extent %7.1f %7.1f %7.1f   expected ~%5.0f %5.0f %5.0f   %s" % ((name,) + got + expected + (status,)))
    if not ok:
        warn("Some meshes are the wrong size. Set IMPORT_UNIFORM_SCALE at the top of this script "
             "(100 if they came in 100x too small, 0.01 if 100x too large) and run it again.")
    return ok


def main():
    log("Importing Gem Roller assets from %s" % PROJECT_DIR)
    step_errors = 0
    for step in (import_meshes, import_audio, build_master_material, create_map):
        try:
            step()
        except Exception as exc:  # noqa: BLE001 - keep going so one failing step does not hide the others
            step_errors += 1
            unreal.log_error("[GemRoller] %s failed: %s" % (step.__name__, exc))
    try:
        verify_scale()
    except Exception as exc:  # noqa: BLE001
        warn("Size check skipped: %s" % exc)
    try:
        unreal.EditorAssetLibrary.save_directory(ASSET_ROOT, True, True)
    except Exception:  # noqa: BLE001
        pass
    if step_errors:
        warn("Finished with %d failing step(s); see the log above. The game still runs with placeholder shapes." % step_errors)
    else:
        log("Done. Open %s/Arena and press Play." % MAP_DIR)


main()
