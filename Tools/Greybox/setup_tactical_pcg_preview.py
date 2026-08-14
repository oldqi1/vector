"""Build a deterministic tactical-PCG greybox in the current Unreal level.

Compile the project first, then run from Tools -> Execute Python Script.
Change SEED and rerun to compare layouts. Only actors prefixed ``PCG_`` are
replaced. Encounter positions are TargetPoint markers on purpose: later wave
activation can consume them without waking every room at BeginPlay.
"""

import unreal


SEED = 4417
PREFIX = "PCG_"
MODULE_SPACING = 2800.0


def log(message):
    unreal.log("VECTOR_PCG: " + message)


def subsystem():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def load_native_class(path):
    actor_class = unreal.load_class(None, path)
    if actor_class is None:
        raise RuntimeError("could not load native class: %s" % path)
    return actor_class


def clear_previous():
    removed = 0
    editor = subsystem()
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor.get_actor_label().startswith(PREFIX):
            editor.destroy_actor(actor)
            removed += 1
    log("cleared %d previous preview actors" % removed)


def spawn_cube(location, scale, label, rotation=None):
    mesh = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
    if mesh is None:
        raise RuntimeError("could not load engine cube mesh")
    actor = subsystem().spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(*location))
    if actor is None:
        raise RuntimeError("failed to spawn cube: %s" % label)
    actor.static_mesh_component.set_static_mesh(mesh)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    if rotation is not None:
        actor.set_actor_rotation(unreal.Rotator(*rotation), False)
    actor.set_actor_label(PREFIX + label)
    return actor


def spawn_marker(target_point_class, location, label):
    marker = subsystem().spawn_actor_from_class(
        target_point_class, unreal.Vector(*location))
    if marker is None:
        raise RuntimeError("failed to spawn marker: %s" % label)
    marker.set_actor_label(PREFIX + label)
    return marker


def spawn_floor(center_x, width=2400.0, depth=2200.0, suffix="Floor"):
    return spawn_cube(
        (center_x, 0.0, -50.0),
        (width / 100.0, depth / 100.0, 0.5),
        suffix,
    )


def spawn_boundary(center_x, half_x=1200.0, half_y=1100.0):
    spawn_cube((center_x, half_y, 125.0),
               (half_x / 50.0, 1.0, 3.5), "BoundaryNorth")
    spawn_cube((center_x, -half_y, 125.0),
               (half_x / 50.0, 1.0, 3.5), "BoundarySouth")


def build_safe_start(center_x, target_point_class, player_start_class):
    spawn_floor(center_x, 1800.0, 1800.0, "SafeStart_Floor")
    spawn_boundary(center_x, 900.0, 900.0)
    start = subsystem().spawn_actor_from_class(
        player_start_class, unreal.Vector(center_x - 450.0, 0.0, 120.0))
    if start is None:
        raise RuntimeError("failed to spawn PlayerStart")
    start.set_actor_label(PREFIX + "PlayerStart")
    spawn_marker(target_point_class, (center_x + 500.0, 0.0, 80.0),
                 "SafeStart_Recovery")


def build_open_bowl(center_x, target_point_class):
    spawn_floor(center_x, suffix="OpenBowl_Floor")
    spawn_boundary(center_x)
    spawn_cube((center_x + 1050.0, 0.0, 125.0),
               (1.0, 22.0, 3.5), "OpenBowl_ImpactWall")
    for index, offset in enumerate(((-650, -350), (-200, 350), (300, -250), (650, 300))):
        spawn_marker(target_point_class, (center_x + offset[0], offset[1], 80.0),
                     "OpenBowl_Enemy_%02d" % index)


def build_hard_lane(center_x, target_point_class):
    spawn_floor(center_x, suffix="HardLane_Floor")
    spawn_cube((center_x, 500.0, 125.0), (24.0, 1.0, 3.5), "HardLane_WallNorth")
    spawn_cube((center_x, -500.0, 125.0), (24.0, 1.0, 3.5), "HardLane_WallSouth")
    spawn_cube((center_x + 1150.0, 0.0, 125.0), (1.0, 11.0, 3.5), "HardLane_Receiver")
    spawn_cube((center_x - 900.0, 0.0, 75.0), (4.0, 8.0, 1.5), "HardLane_LaunchShelf")
    for index, offset in enumerate((-600.0, 0.0, 650.0)):
        spawn_marker(target_point_class, (center_x + offset, 0.0, 120.0),
                     "HardLane_Enemy_%02d" % index)


def build_height_shelf(center_x, target_point_class):
    spawn_floor(center_x, suffix="HeightShelf_Floor")
    spawn_boundary(center_x)
    spawn_cube((center_x + 500.0, 0.0, 175.0), (10.0, 18.0, 3.0), "HeightShelf_Upper")
    spawn_cube((center_x - 50.0, 0.0, 25.0), (1.5, 18.0, 1.0), "HeightShelf_Step1")
    spawn_cube((center_x + 100.0, 0.0, 75.0), (1.5, 18.0, 2.0), "HeightShelf_Step2")
    spawn_cube((center_x - 1050.0, 0.0, 125.0), (1.0, 22.0, 3.5), "HeightShelf_Receiver")
    spawn_marker(target_point_class, (center_x + 450.0, -350.0, 380.0), "HeightShelf_Enemy_High")
    spawn_marker(target_point_class, (center_x - 500.0, 300.0, 80.0), "HeightShelf_Enemy_Low")


def build_slick_cross(center_x, target_point_class, friction_class):
    spawn_floor(center_x, suffix="SlickCross_Floor")
    spawn_boundary(center_x)
    spawn_cube((center_x, 0.0, -42.0), (18.0, 5.0, 0.12), "SlickCross_MarkerX")
    spawn_cube((center_x, 0.0, -40.0), (5.0, 18.0, 0.12), "SlickCross_MarkerY")
    zone = subsystem().spawn_actor_from_class(
        friction_class, unreal.Vector(center_x, 0.0, 100.0))
    if zone is None:
        raise RuntimeError("failed to spawn low-friction zone")
    zone.set_actor_label(PREFIX + "SlickCross_LowFrictionZone")
    for index, offset in enumerate(((-500, 0), (0, -500), (500, 0), (0, 500))):
        spawn_marker(target_point_class, (center_x + offset[0], offset[1], 80.0),
                     "SlickCross_Enemy_%02d" % index)


def build_boss_ring(center_x, target_point_class):
    spawn_floor(center_x, 3200.0, 3000.0, "BossRing_Floor")
    spawn_boundary(center_x, 1600.0, 1500.0)
    for index, offset in enumerate(((-1350, -1250), (-1350, 1250), (1350, -1250), (1350, 1250))):
        spawn_cube((center_x + offset[0], offset[1], 175.0),
                   (2.5, 2.5, 4.5), "BossRing_Pillar_%02d" % index)
    spawn_cube((center_x, 1150.0, 125.0), (8.0, 4.0, 3.5), "BossRing_HeightShelf")
    spawn_marker(target_point_class, (center_x, 0.0, 100.0), "BossRing_BossSpawn")
    spawn_marker(target_point_class, (center_x - 650.0, -450.0, 80.0), "BossRing_AddSpawn_A")
    spawn_marker(target_point_class, (center_x + 650.0, 450.0, 80.0), "BossRing_AddSpawn_B")


def build_extraction(center_x, extraction_class):
    spawn_floor(center_x, 1400.0, 1400.0, "Extraction_Floor")
    zone = subsystem().spawn_actor_from_class(
        extraction_class, unreal.Vector(center_x, 0.0, 140.0))
    if zone is None:
        raise RuntimeError("failed to spawn extraction zone")
    zone.set_actor_label(PREFIX + "ExtractionZone")


def configure_game_mode(game_mode_class):
    world = unreal.EditorLevelLibrary.get_editor_world()
    if world is None or world.get_world_settings() is None:
        raise RuntimeError("could not resolve editor world settings")
    world.get_world_settings().set_editor_property("default_game_mode", game_mode_class)


def build_navmesh(navmesh_class, module_count):
    center_x = (module_count - 1) * MODULE_SPACING * 0.5
    total_x = module_count * MODULE_SPACING + 1200.0
    navmesh = subsystem().spawn_actor_from_class(
        navmesh_class, unreal.Vector(center_x, 0.0, 350.0))
    if navmesh is None:
        raise RuntimeError("failed to spawn navmesh bounds")
    navmesh.set_actor_label(PREFIX + "NavMeshBounds")
    navmesh.set_actor_scale3d(unreal.Vector(total_x / 100.0, 40.0, 8.0))


def main(seed=SEED):
    library = getattr(unreal, "VectorTacticalGenerationLibrary", None)
    if library is None:
        raise RuntimeError("VectorTacticalGenerationLibrary missing; compile and restart the editor")
    sequence = list(library.generate_module_sequence(seed))
    if not sequence or sequence[0] != "SafeStart" or sequence[-1] != "Extraction":
        raise RuntimeError("invalid generated sequence: %s" % sequence)

    classes = {
        "player_start": load_native_class("/Script/Engine.PlayerStart"),
        "target_point": load_native_class("/Script/Engine.TargetPoint"),
        "friction": load_native_class("/Script/Vector.VectorLowFrictionZone"),
        "extraction": load_native_class("/Script/Vector.VectorExtractionZone"),
        "navmesh": load_native_class("/Script/NavigationSystem.NavMeshBoundsVolume"),
        "game_mode": load_native_class("/Script/Vector.VectorGameMode"),
    }
    clear_previous()
    configure_game_mode(classes["game_mode"])

    builders = {
        "SafeStart": lambda x: build_safe_start(x, classes["target_point"], classes["player_start"]),
        "OpenBowl": lambda x: build_open_bowl(x, classes["target_point"]),
        "HardLane": lambda x: build_hard_lane(x, classes["target_point"]),
        "HeightShelf": lambda x: build_height_shelf(x, classes["target_point"]),
        "SlickCross": lambda x: build_slick_cross(x, classes["target_point"], classes["friction"]),
        "BossRing": lambda x: build_boss_ring(x, classes["target_point"]),
        "Extraction": lambda x: build_extraction(x, classes["extraction"]),
    }
    for index, module_name in enumerate(sequence):
        center_x = index * MODULE_SPACING
        builders[module_name](center_x)
        log("module[%d]=%s center_x=%.0f" % (index, module_name, center_x))

    build_navmesh(classes["navmesh"], len(sequence))
    log("layout: " + library.describe_layout(seed))
    log("SUCCESS: deterministic PCG preview built; Build Paths before Play")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        unreal.log_error("VECTOR_PCG FAILED: %s" % exc)
        raise
