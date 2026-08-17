"""Import the CC0 Quaternius monster OBJ files with deterministic asset names.

Run from Unreal Editor: Tools > Execute Python Script. This is intentionally a
one-shot, presentation-only import; collision and gameplay remain C++ owned.
"""

from pathlib import Path

import unreal


SOURCE_NAMES = ("Bat", "Skeleton", "Slime", "Dragon")
DESTINATION_ROOT = "/Game/Vector/Art/PrototypeMonsters"


def log(message):
    unreal.log("[PrototypeMonsters] " + message)


def source_root():
    project_dir = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
    return (project_dir.parent / "ArtSource" / "QuaterniusAnimatedMonsterPack").resolve()


def import_one(asset_tools, root, source_name):
    destination_dir = f"{DESTINATION_ROOT}/{source_name}"
    final_asset = f"{destination_dir}/SM_Prototype_{source_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(final_asset):
        log(f"already imported: {final_asset}")
        return final_asset

    source_file = root / f"{source_name}.obj"
    if not source_file.is_file():
        raise RuntimeError(f"Missing source model: {source_file}")

    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = True
    options.import_textures = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    options.static_mesh_import_data.combine_meshes = True
    options.static_mesh_import_data.import_uniform_scale = 100.0
    options.static_mesh_import_data.convert_scene = True
    options.static_mesh_import_data.convert_scene_unit = True

    task = unreal.AssetImportTask()
    task.automated = True
    task.destination_name = source_name
    task.destination_path = destination_dir
    task.filename = str(source_file)
    task.options = options
    task.replace_existing = False
    task.save = True
    asset_tools.import_asset_tasks([task])

    imported_mesh_paths = []
    for object_path in task.imported_object_paths:
        asset = unreal.load_asset(object_path)
        if isinstance(asset, unreal.StaticMesh):
            imported_mesh_paths.append(object_path.split(".")[0])
    if len(imported_mesh_paths) != 1:
        raise RuntimeError(
            f"Expected one combined StaticMesh for {source_name}, got {imported_mesh_paths}"
        )

    imported_mesh = imported_mesh_paths[0]
    if imported_mesh != final_asset:
        if not unreal.EditorAssetLibrary.rename_asset(imported_mesh, final_asset):
            raise RuntimeError(f"Could not rename {imported_mesh} to {final_asset}")
    unreal.EditorAssetLibrary.save_directory(destination_dir, only_if_is_dirty=False, recursive=True)
    log(f"imported {source_file.name} -> {final_asset}")
    return final_asset


def main():
    root = source_root()
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    imported = [import_one(asset_tools, root, name) for name in SOURCE_NAMES]
    log("PASS: " + ", ".join(imported))


main()
