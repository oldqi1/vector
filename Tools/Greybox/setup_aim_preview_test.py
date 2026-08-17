"""Create a dedicated lift/vector targeting preview fixture map.

Run from Unreal Editor: Tools -> Execute Python Script, then choose this file.
The script creates or rebuilds /Game/Prototype/L_AimPreviewTest and saves it.
It does not edit the PCG map. Existing actors with the AP_* prefix in the test
map are replaced so the fixture is safe to regenerate.
"""

import unreal


MAP_PATH = "/Game/Prototype/L_AimPreviewTest"
PREFIX = "AP_"


def log(message):
    unreal.log("AIM_PREVIEW_TEST: " + message)


def load_native_class(path):
    actor_class = unreal.load_class(None, path)
    if actor_class is None:
        raise RuntimeError("could not load native class: %s" % path)
    return actor_class


def resolve_mass_classes():
    enum_type = getattr(unreal, "VectorMassClass", None)
    if enum_type is None:
        enum_type = getattr(unreal, "EVectorMassClass", None)
    if enum_type is None:
        raise RuntimeError("VectorMassClass is not exposed to Unreal Python")
    return {
        "Light": enum_type.LIGHT,
        "Medium": enum_type.MEDIUM,
        "Heavy": enum_type.HEAVY,
    }


def open_or_create_test_map():
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        loaded = unreal.EditorLevelLibrary.load_level(MAP_PATH)
        if not loaded:
            raise RuntimeError("failed to load existing map: %s" % MAP_PATH)
        log("loaded existing map: %s" % MAP_PATH)
        return
    created = unreal.EditorLevelLibrary.new_level(MAP_PATH)
    if not created:
        raise RuntimeError("failed to create map: %s" % MAP_PATH)
    log("created map: %s" % MAP_PATH)


def clear_previous(subsystem):
    removed = 0
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor.get_actor_label().startswith(PREFIX):
            subsystem.destroy_actor(actor)
            removed += 1
    if removed:
        log("cleared %d previous fixture actors" % removed)


def spawn_cube(subsystem, mesh, location, scale, label):
    actor = subsystem.spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(*location))
    if actor is None:
        raise RuntimeError("failed to spawn cube: %s" % label)
    actor.static_mesh_component.set_static_mesh(mesh)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    actor.set_actor_label(PREFIX + label)
    return actor


def spawn_dummy(subsystem, dummy_class, mass_class, location, label):
    actor = subsystem.spawn_actor_from_class(
        dummy_class, unreal.Vector(*location))
    if actor is None:
        raise RuntimeError("failed to spawn dummy: %s" % label)
    actor.set_actor_label(PREFIX + label)
    actor.set_editor_property("mass_class", mass_class)
    return actor


def configure_world(game_mode_class):
    world = unreal.EditorLevelLibrary.get_editor_world()
    if world is None or world.get_world_settings() is None:
        raise RuntimeError("could not resolve test world settings")
    world.get_world_settings().set_editor_property(
        "default_game_mode", game_mode_class)


def spawn_lighting(subsystem):
    key = subsystem.spawn_actor_from_class(
        unreal.DirectionalLight, unreal.Vector(0.0, 0.0, 800.0))
    if key is not None:
        key.set_actor_label(PREFIX + "KeyLight")
        key.set_actor_rotation(unreal.Rotator(-55.0, -35.0, 0.0), False)
        key.directional_light_component.set_editor_property("intensity", 6.0)
    fill = subsystem.spawn_actor_from_class(
        unreal.PointLight, unreal.Vector(450.0, 0.0, 850.0))
    if fill is not None:
        fill.set_actor_label(PREFIX + "FillLight")
        fill.point_light_component.set_editor_property("intensity", 9000.0)
        fill.point_light_component.set_editor_property(
            "attenuation_radius", 2400.0)


def main():
    # Resolve reflected dependencies before changing levels.
    dummy_class = load_native_class("/Script/Vector.VectorTestDummy")
    game_mode_class = load_native_class("/Script/Vector.VectorGameMode")
    player_start_class = load_native_class("/Script/Engine.PlayerStart")
    masses = resolve_mass_classes()
    cube_mesh = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
    if cube_mesh is None:
        raise RuntimeError("could not load engine cube mesh")

    open_or_create_test_map()
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    clear_previous(subsystem)
    configure_world(game_mode_class)

    spawn_cube(subsystem, cube_mesh, (450.0, 0.0, -50.0),
               (24.0, 16.0, 0.5), "Floor")
    spawn_cube(subsystem, cube_mesh, (450.0, -825.0, 160.0),
               (24.0, 0.5, 3.2), "WallSouth")
    spawn_cube(subsystem, cube_mesh, (450.0, 825.0, 160.0),
               (24.0, 0.5, 3.2), "WallNorth")
    spawn_cube(subsystem, cube_mesh, (1650.0, 0.0, 160.0),
               (0.5, 16.0, 3.2), "WallEast")
    spawn_cube(subsystem, cube_mesh, (-750.0, 0.0, 160.0),
               (0.5, 16.0, 3.2), "WallWest")

    # H07 first-blocking fixtures, placed beyond the selectable target pairs.
    # The south lane terminates at a wall, the center lane crosses a raised
    # platform, and the north lane passes under a low ceiling slab.
    spawn_cube(subsystem, cube_mesh, (920.0, -360.0, 210.0),
               (0.5, 1.4, 4.2), "PredictionWall")
    spawn_cube(subsystem, cube_mesh, (900.0, 0.0, 70.0),
               (3.0, 1.4, 0.7), "PredictionPlatform")
    spawn_cube(subsystem, cube_mesh, (760.0, 360.0, 430.0),
               (3.0, 1.4, 0.35), "PredictionCeiling")

    player_start = subsystem.spawn_actor_from_class(
        player_start_class, unreal.Vector(0.0, 0.0, 120.0))
    if player_start is None:
        raise RuntimeError("failed to spawn PlayerStart")
    player_start.set_actor_label(PREFIX + "PlayerStart")
    player_start.set_actor_rotation(unreal.Rotator(0.0, 0.0, 0.0), False)

    # The front target is inside the lift fork's 650 cm reach. Near the apex of
    # its 700 cm/s lift, it visually crosses the rear grounded target from the
    # default 55-degree camera. Each lane verifies a different mass class.
    lane_data = (
        ("Light", -360.0),
        ("Medium", 0.0),
        ("Heavy", 360.0),
    )
    for mass_name, lane_y in lane_data:
        spawn_dummy(
            subsystem, dummy_class, masses[mass_name],
            (350.0, lane_y, 120.0), "%s_LiftFront" % mass_name)
        spawn_dummy(
            subsystem, dummy_class, masses["Medium"],
            (525.0, lane_y, 120.0), "%s_GroundRear" % mass_name)

    # A clearly separated target proves that moving the cursor away releases
    # the airborne overlap preference instead of creating a hidden hard lock.
    spawn_dummy(
        subsystem, dummy_class, masses["Light"],
        (450.0, 680.0, 120.0), "CursorReleaseControl")
    spawn_lighting(subsystem)

    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("fixture built but current level could not be saved")
    log("SUCCESS: saved %s" % MAP_PATH)
    log("Three lanes: front target x=350, rear target x=525, y=-360/0/360")
    log("H07 receivers: south wall, center raised platform, north ceiling")
    log("Play -> aim at a front target -> 5/LMB -> 1 -> verify cyan airborne selection and shadow")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        unreal.log_error("AIM_PREVIEW_TEST FAILED: %s" % exc)
        raise
