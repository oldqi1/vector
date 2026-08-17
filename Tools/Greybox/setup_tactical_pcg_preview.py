"""Build a deterministic tactical-PCG greybox in the current Unreal level.

Compile the project first, then run from Tools -> Execute Python Script.
Change SEED and rerun to compare layouts. Only actors prefixed ``PCG_`` are
replaced. Encounter positions are TargetPoint markers on purpose: later wave
activation can consume them without waking every room at BeginPlay.
"""

import math
import unreal


SEED = 4417
PREFIX = "PCG_"
MODULE_SPACING = 2800.0
ENCOUNTER_WAVES = []
BOSS_SPAWN_POINT = None
BOSS_ADD_SPAWN_POINTS = []
BOSS_OVERLOAD_FRICTION_ZONE = None
WAVE_GATES = []
ROOM_TRIGGERS = []


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
        # Internal tuples use UE/C++ reading order (pitch, yaw, roll), while
        # Unreal Python's constructor is (roll, pitch, yaw).
        pitch, yaw, roll = rotation
        actor.set_actor_rotation(
            unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw), False)
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


def spawn_split_receiver_wall(center_x, half_width, exit_gap, label):
    """Keep two impact receivers while reserving a permanent central route."""
    if exit_gap < 650.0 or exit_gap >= half_width * 2.0:
        raise RuntimeError("invalid receiver exit gap: %s %.0f" %
                           (label, exit_gap))
    wing_width = half_width - exit_gap * 0.5
    wing_center_y = exit_gap * 0.5 + wing_width * 0.5
    for suffix, sign in (("North", 1.0), ("South", -1.0)):
        spawn_cube(
            (center_x, sign * wing_center_y, 125.0),
            (1.0, wing_width / 100.0, 3.5),
            "%s_%s" % (label, suffix),
        )
    log("receiver=%s permanentExitGap=%.0f" % (label, exit_gap))


def spawn_ramp_x(start_x, center_y, direction, run, rise,
                 width=500.0, base_z=0.0, thickness=40.0, label="Ramp"):
    """Build one broad walkable slope whose upper edge meets a combat deck."""
    if run <= 0.0 or rise <= 0.0 or direction not in (-1.0, 1.0):
        raise RuntimeError("invalid ramp geometry: %s" % label)
    length = math.sqrt(run * run + rise * rise)
    pitch = math.degrees(math.atan2(rise, run)) * direction
    if abs(pitch) > 32.0:
        raise RuntimeError("ramp exceeds conservative nav slope: %s %.1fdeg" %
                           (label, abs(pitch)))
    center_x = start_x + direction * run * 0.5
    center_z = base_z + rise * 0.5 - thickness * 0.5
    spawn_cube(
        (center_x, center_y, center_z),
        (length / 100.0, width / 100.0, thickness / 100.0),
        label,
        (pitch, 0.0, 0.0),
    )
    # Wide seam pads overlap both authored solids. They remove the tiny collision
    # lips that can split Recast polygons even when the visible ramp looks flush.
    seam_depth = max(70.0, thickness * 1.5)
    spawn_cube(
        (start_x, center_y, base_z - seam_depth * 0.5),
        (1.4, width / 100.0, seam_depth / 100.0),
        label + "_LowerSeamPad",
    )
    spawn_cube(
        (start_x + direction * run, center_y,
         base_z + rise - seam_depth * 0.5),
        (1.4, width / 100.0, seam_depth / 100.0),
        label + "_UpperSeamPad",
    )
    log("ramp=%s rise=%.0f run=%.0f slope=%.1fdeg" %
        (label, rise, run, abs(pitch)))


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
    # Authoring circuit (not player-facing labels):
    # charger source -> angled wall / tether pillar converter -> heavy anchor receiver.
    # The room remains open enough for a direct bait route and a recovery pocket.
    spawn_cube((center_x - 100.0, -780.0, 125.0),
               (8.0, 0.55, 3.5), "OpenBowl_Converter_AngledWallSouth",
               (0.0, 24.0, 0.0))
    spawn_cube((center_x - 100.0, 780.0, 125.0),
               (8.0, 0.55, 3.5), "OpenBowl_Converter_AngledWallNorth",
               (0.0, -24.0, 0.0))
    spawn_cube((center_x - 120.0, -430.0, 175.0),
               (1.25, 1.25, 4.5), "OpenBowl_Converter_TetherPillarSouth")
    spawn_cube((center_x - 120.0, 430.0, 175.0),
               (1.25, 1.25, 4.5), "OpenBowl_Converter_TetherPillarNorth")
    spawn_split_receiver_wall(
        center_x + 1050.0, 1100.0, 700.0, "OpenBowl_ImpactReceiver")
    # Intentionally flat: its tactical verb is horizontal redirection.
    markers = []
    # Runtime index contract: 0=heavy receiver, 1=charger source, remaining=light ammo.
    offsets = ((380, 0, 80), (-720, -520, 80), (-420, 560, 80),
               (180, -520, 80), (650, 480, 80))
    for index, offset in enumerate(offsets):
        markers.append(spawn_marker(
            target_point_class, (center_x + offset[0], offset[1], offset[2]),
            "OpenBowl_Enemy_%02d" % index))
    ENCOUNTER_WAVES.append(markers)
    log("OpenBowl circuit: source=charger converter=2 angled walls+2 tether pillars "
        "receiver=two anchor groups recovery=entry-west routes=direct/wall/tether")


def build_hard_lane(center_x, target_point_class):
    spawn_floor(center_x, suffix="HardLane_Floor")
    spawn_cube((center_x, 500.0, 125.0), (24.0, 1.0, 3.5), "HardLane_WallNorth")
    spawn_cube((center_x, -500.0, 125.0), (24.0, 1.0, 3.5), "HardLane_WallSouth")
    spawn_split_receiver_wall(
        center_x + 1150.0, 550.0, 650.0, "HardLane_ImpactReceiver")
    # A broad chute, not decorative stairs: the low charger owns the straight
    # bait line while the upper heavy can be knocked into that line/crowd.
    spawn_cube((center_x + 350.0, 250.0, 160.0),
               (11.0, 4.0, 3.6), "HardLane_UpperLane")
    spawn_ramp_x(center_x - 950.0, 250.0, 1.0, 800.0, 340.0,
                 width=350.0, label="HardLane_MomentumRamp")
    markers = []
    offsets = ((-850, -260, 80), (150, 250, 430), (-500, -100, 80),
               (-150, -260, 80), (-50, 250, 400), (300, -150, 80),
               (550, 250, 400), (750, -250, 80))
    for index, offset in enumerate(offsets):
        markers.append(spawn_marker(
            target_point_class, (center_x + offset[0], offset[1], offset[2]),
            "HardLane_Enemy_%02d" % index))
    ENCOUNTER_WAVES.append(markers)


def build_height_shelf(center_x, target_point_class):
    spawn_floor(center_x, suffix="HeightShelf_Floor")
    spawn_boundary(center_x)
    spawn_cube((center_x + 650.0, 0.0, 250.0),
               (9.0, 18.0, 5.5), "HeightShelf_Upper")
    spawn_cube((center_x - 50.0, 0.0, 100.0),
               (4.0, 18.0, 2.5), "HeightShelf_Middle")
    spawn_ramp_x(center_x - 850.0, -450.0, 1.0, 650.0, 225.0,
                 width=420.0, label="HeightShelf_LowerMomentumRamp")
    spawn_ramp_x(center_x - 100.0, 450.0, 1.0, 750.0, 300.0,
                 width=420.0, base_z=225.0,
                 label="HeightShelf_UpperMomentumRamp")
    spawn_cube((center_x - 1050.0, 0.0, 125.0), (1.0, 22.0, 3.5), "HeightShelf_Receiver")
    markers = []
    offsets = ((450, -600, 650), (450, -200, 650), (650, 250, 650), (650, 650, 650),
               (-700, -600, 80), (-700, -200, 80), (-700, 250, 80), (-700, 650, 80))
    for index, offset in enumerate(offsets):
        markers.append(spawn_marker(
            target_point_class, (center_x + offset[0], offset[1], offset[2]),
            "HeightShelf_Enemy_%02d" % index))
    ENCOUNTER_WAVES.append(markers)


def build_slick_cross(center_x, target_point_class, friction_class):
    spawn_floor(center_x, suffix="SlickCross_Floor")
    spawn_boundary(center_x)
    spawn_cube((center_x, 0.0, -42.0), (18.0, 5.0, 0.12), "SlickCross_MarkerX")
    spawn_cube((center_x, 0.0, -40.0), (5.0, 18.0, 0.12), "SlickCross_MarkerY")
    # Intentionally flat: this room's unique verb is preserving horizontal
    # momentum through the low-friction cross, not climbing an arbitrary deck.
    zone = subsystem().spawn_actor_from_class(
        friction_class, unreal.Vector(center_x, 0.0, 100.0))
    if zone is None:
        raise RuntimeError("failed to spawn low-friction zone")
    zone.set_actor_label(PREFIX + "SlickCross_LowFrictionZone")
    markers = []
    offsets = ((-700, 0, 80), (-450, -450, 80), (-450, 450, 80), (0, -700, 80),
               (250, 550, 80), (500, 750, 80), (750, 550, 80), (700, -200, 80))
    for index, offset in enumerate(offsets):
        markers.append(spawn_marker(
            target_point_class, (center_x + offset[0], offset[1], offset[2]),
            "SlickCross_Enemy_%02d" % index))
    ENCOUNTER_WAVES.append(markers)


def build_boss_ring(center_x, target_point_class, friction_class):
    global BOSS_SPAWN_POINT, BOSS_ADD_SPAWN_POINTS, BOSS_OVERLOAD_FRICTION_ZONE
    spawn_floor(center_x, 3200.0, 3000.0, "BossRing_Floor")
    spawn_boundary(center_x, 1600.0, 1500.0)
    for index, offset in enumerate(((-1350, -1250), (-1350, 1250), (1350, -1250), (1350, 1250))):
        spawn_cube((center_x + offset[0], offset[1], 175.0),
                   (2.5, 2.5, 4.5), "BossRing_Pillar_%02d" % index)
    # Three layers make the Boss' ballistic add attack and landing shock
    # interact with actual traversal: basin, middle deck, high launch terrace.
    spawn_cube((center_x, 1150.0, 175.0),
               (10.0, 4.0, 4.0), "BossRing_MiddleShelf")
    spawn_cube((center_x, -1150.0, 300.0),
               (10.0, 4.0, 6.5), "BossRing_HighShelf")
    spawn_ramp_x(center_x - 1150.0, 1150.0, 1.0, 1050.0, 375.0,
                 width=420.0, label="BossRing_MiddleAmmoRamp")
    spawn_ramp_x(center_x - 1400.0, -1150.0, 1.0, 1500.0, 625.0,
                 width=420.0, label="BossRing_HighAmmoRamp")
    BOSS_SPAWN_POINT = spawn_marker(
        target_point_class, (center_x, 0.0, 100.0), "BossRing_BossSpawn")
    BOSS_ADD_SPAWN_POINTS = [
        spawn_marker(target_point_class, (center_x - 350.0, 1150.0, 450.0), "BossRing_AddSpawn_A"),
        spawn_marker(target_point_class, (center_x + 350.0, -1150.0, 700.0), "BossRing_AddSpawn_B"),
    ]
    BOSS_OVERLOAD_FRICTION_ZONE = subsystem().spawn_actor_from_class(
        friction_class, unreal.Vector(center_x, 0.0, 100.0))
    if BOSS_OVERLOAD_FRICTION_ZONE is None:
        raise RuntimeError("failed to spawn BossRing overload friction zone")
    BOSS_OVERLOAD_FRICTION_ZONE.set_actor_label(PREFIX + "BossRing_OverloadFriction")
    BOSS_OVERLOAD_FRICTION_ZONE.set_editor_property("start_active", False)


def build_extraction(center_x, extraction_class, contract_exit_class):
    spawn_floor(center_x, 1400.0, 1400.0, "Extraction_Floor")
    spawn_cube((center_x, 300.0, 125.0),
               (14.0, 1.0, 3.5), "Extraction_WallNorth")
    spawn_cube((center_x, -300.0, 125.0),
               (14.0, 1.0, 3.5), "Extraction_WallSouth")
    zone = subsystem().spawn_actor_from_class(
        extraction_class, unreal.Vector(center_x, 0.0, 140.0))
    if zone is None:
        raise RuntimeError("failed to spawn extraction zone")
    zone.set_actor_label(PREFIX + "ExtractionZone")
    gate = subsystem().spawn_actor_from_class(
        contract_exit_class, unreal.Vector(center_x - 650.0, 0.0, 180.0))
    if gate is None:
        raise RuntimeError("failed to spawn contract exit")
    gate.set_actor_label(PREFIX + "ContractExit")


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
    global ENCOUNTER_WAVES, BOSS_SPAWN_POINT, BOSS_ADD_SPAWN_POINTS
    global BOSS_OVERLOAD_FRICTION_ZONE
    global WAVE_GATES
    global ROOM_TRIGGERS
    ENCOUNTER_WAVES = []
    BOSS_SPAWN_POINT = None
    BOSS_ADD_SPAWN_POINTS = []
    BOSS_OVERLOAD_FRICTION_ZONE = None
    WAVE_GATES = []
    ROOM_TRIGGERS = []
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
        "contract_exit": load_native_class("/Script/Vector.VectorContractExit"),
        "director": load_native_class("/Script/Vector.VectorPCGEncounterDirector"),
        "wave_gate": load_native_class("/Script/Vector.VectorPCGWaveGate"),
        "room_trigger": load_native_class("/Script/Vector.VectorPCGRoomTrigger"),
        "navmesh": load_native_class("/Script/NavigationSystem.NavMeshBoundsVolume"),
        "game_mode": load_native_class("/Script/Vector.VectorGameMode"),
    }
    clear_previous()
    configure_game_mode(classes["game_mode"])

    route_length = (len(sequence) - 1) * MODULE_SPACING + 1800.0
    spawn_cube(
        ((len(sequence) - 1) * MODULE_SPACING * 0.5, 0.0, -50.0),
        (route_length / 100.0, 7.0, 1.0),
        "RouteSpine",
    )

    builders = {
        "SafeStart": lambda x: build_safe_start(x, classes["target_point"], classes["player_start"]),
        "OpenBowl": lambda x: build_open_bowl(x, classes["target_point"]),
        "HardLane": lambda x: build_hard_lane(x, classes["target_point"]),
        "HeightShelf": lambda x: build_height_shelf(x, classes["target_point"]),
        "SlickCross": lambda x: build_slick_cross(x, classes["target_point"], classes["friction"]),
        "BossRing": lambda x: build_boss_ring(
            x, classes["target_point"], classes["friction"]),
        "Extraction": lambda x: build_extraction(
            x, classes["extraction"], classes["contract_exit"]),
    }
    for index, module_name in enumerate(sequence):
        center_x = index * MODULE_SPACING
        builders[module_name](center_x)
        log("module[%d]=%s center_x=%.0f" % (index, module_name, center_x))

    for gate_index in (1, 2):
        gate_x = (gate_index + 0.5) * MODULE_SPACING
        gate = subsystem().spawn_actor_from_class(
            classes["wave_gate"], unreal.Vector(gate_x, 0.0, 180.0))
        if gate is None:
            raise RuntimeError("failed to spawn PCG wave gate %d" % gate_index)
        gate.set_actor_label(PREFIX + "WaveGate_%d" % gate_index)
        WAVE_GATES.append(gate)

    trigger_positions = (0.5 * MODULE_SPACING,
                         1.5 * MODULE_SPACING + 250.0,
                         2.5 * MODULE_SPACING + 250.0)
    for room_index, trigger_x in enumerate(trigger_positions):
        trigger = subsystem().spawn_actor_from_class(
            classes["room_trigger"], unreal.Vector(trigger_x, 0.0, 160.0))
        if trigger is None:
            raise RuntimeError("failed to spawn PCG room trigger %d" % room_index)
        trigger.set_actor_label(PREFIX + "RoomTrigger_%d" % room_index)
        trigger.set_editor_property("room_index", room_index)
        ROOM_TRIGGERS.append(trigger)

    build_navmesh(classes["navmesh"], len(sequence))
    if (len(ENCOUNTER_WAVES) != 2
            or any(len(wave) < 3 or len(wave) > 8
                   for wave in ENCOUNTER_WAVES)
            or BOSS_SPAWN_POINT is None
            or BOSS_OVERLOAD_FRICTION_ZONE is None or len(WAVE_GATES) != 2
            or len(ROOM_TRIGGERS) != 3):
        raise RuntimeError(
            "runtime encounter wiring incomplete: waves=%d boss=%s"
            % (len(ENCOUNTER_WAVES), BOSS_SPAWN_POINT))
    director = subsystem().spawn_actor_from_class(
        classes["director"], unreal.Vector(0.0, 0.0, 100.0))
    if director is None:
        raise RuntimeError("failed to spawn PCG encounter director")
    director.set_actor_label(PREFIX + "EncounterDirector")
    director.set_editor_property("generation_seed", seed)
    director.set_editor_property("module_sequence", sequence)
    director.set_editor_property("encounter_wave_one_spawns", ENCOUNTER_WAVES[0])
    director.set_editor_property("encounter_wave_two_spawns", ENCOUNTER_WAVES[1])
    director.set_editor_property("boss_spawn_point", BOSS_SPAWN_POINT)
    director.set_editor_property("boss_add_spawn_points", BOSS_ADD_SPAWN_POINTS)
    director.set_editor_property(
        "boss_overload_friction_zone", BOSS_OVERLOAD_FRICTION_ZONE)
    director.set_editor_property("wave_one_exit_gate", WAVE_GATES[0])
    director.set_editor_property("wave_two_exit_gate", WAVE_GATES[1])
    director.set_editor_property("room_activation_triggers", ROOM_TRIGGERS)
    log("layout: " + library.describe_layout(seed))
    log("SUCCESS: deterministic PCG route built with sequential %d+%d+Boss waves; Build Paths before Play"
        % (len(ENCOUNTER_WAVES[0]), len(ENCOUNTER_WAVES[1])))


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        unreal.log_error("VECTOR_PCG FAILED: %s" % exc)
        raise
