"""Place a repeatable combat-physics test cluster in the current editor level.

Run with Unreal Editor: Tools -> Execute Python Script, then choose this file.
The script only replaces actors whose labels start with ``GA_PhysicsTest_``.
It intentionally uses inert VectorTestDummy actors so each reaction can be
triggered by the player without enemy AI moving the fixtures first.
"""

import unreal


PREFIX = "GA_PhysicsTest_"


def log(message):
    unreal.log("PHYSICS_COMBO_TEST: " + message)


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
    return enum_type.LIGHT, enum_type.HEAVY


def clear_previous(subsystem):
    removed = 0
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor.get_actor_label().startswith(PREFIX):
            subsystem.destroy_actor(actor)
            removed += 1
    if removed:
        log("cleared %d previous test actors" % removed)


def spawn_dummy(subsystem, dummy_class, location, suffix):
    actor = subsystem.spawn_actor_from_class(dummy_class, unreal.Vector(*location))
    if actor is None:
        raise RuntimeError("failed to spawn dummy: %s" % suffix)
    actor.set_actor_label(PREFIX + suffix)
    return actor


def spawn_wall(subsystem, location, scale, suffix):
    mesh = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
    if mesh is None:
        raise RuntimeError("could not load engine cube mesh")
    actor = subsystem.spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(*location))
    if actor is None:
        raise RuntimeError("failed to spawn wall: %s" % suffix)
    actor.static_mesh_component.set_static_mesh(mesh)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    actor.set_actor_label(PREFIX + suffix)
    return actor


def configure_game_mode(game_mode_class):
    world = unreal.EditorLevelLibrary.get_editor_world()
    if world is None or world.get_world_settings() is None:
        raise RuntimeError("could not resolve current editor world settings")
    world.get_world_settings().set_editor_property(
        "default_game_mode", game_mode_class)


def ensure_player_start(subsystem, player_start_class, actors):
    for actor in actors:
        if isinstance(actor, unreal.PlayerStart):
            log("reusing existing PlayerStart: %s" % actor.get_actor_label())
            return actor
    actor = subsystem.spawn_actor_from_class(
        player_start_class, unreal.Vector(-500.0, 0.0, 120.0))
    if actor is None:
        raise RuntimeError("failed to spawn PlayerStart")
    actor.set_actor_label(PREFIX + "PlayerStart")
    log("spawned missing PlayerStart at (-500,0,120)")
    return actor


def ensure_floor(subsystem, actors):
    if any(actor.get_actor_label() == "GA_Floor" for actor in actors):
        log("reusing GA_Floor from the main arena")
        return None
    return spawn_wall(
        subsystem,
        (200.0, 0.0, -50.0),
        (30.0, 30.0, 0.5),
        "Floor",
    )


def main():
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    dummy_class = load_native_class("/Script/Vector.VectorTestDummy")
    game_mode_class = load_native_class("/Script/Vector.VectorGameMode")
    player_start_class = load_native_class("/Script/Engine.PlayerStart")
    light_mass, heavy_mass = resolve_mass_classes()
    clear_previous(subsystem)
    current_actors = unreal.EditorLevelLibrary.get_all_level_actors()
    configure_game_mode(game_mode_class)
    ensure_player_start(subsystem, player_start_class, current_actors)
    ensure_floor(subsystem, current_actors)

    # Cable pair: manually shoot Light then Heavy. Their mass-weighted meeting
    # point should be visibly closer to Heavy. Both also remain Q/E targets.
    cable_light = spawn_dummy(
        subsystem, dummy_class, (250.0, -220.0, 120.0), "CablePairLight")
    cable_light.set_editor_property("mass_class", light_mass)
    cable_heavy = spawn_dummy(
        subsystem, dummy_class, (850.0, 220.0, 120.0), "CablePairHeavy")
    cable_heavy.set_editor_property("mass_class", heavy_mass)
    # Dedicated north wall for hold-to-reel player traversal.
    spawn_wall(subsystem, (-500.0, 900.0, 180.0), (5.0, 0.5, 4.0), "CableAnchorWall")

    # Wall-burst cluster. Stand west of Source, charge the hammer while facing +X.
    wall_source = spawn_dummy(
        subsystem, dummy_class, (350.0, -800.0, 120.0), "WallBurstSource")
    # A stable light dummy receives 1400 cm/s from a full hammer charge, safely
    # above the 800 cm/s wall-burst threshold. A medium dummy would only get 700.
    wall_source.set_editor_property("mass_class", light_mass)
    # Positioned near the expected wall contact point, but outside Source's lane.
    spawn_dummy(subsystem, dummy_class, (700.0, -500.0, 120.0), "WallBurstNeighbor")
    spawn_wall(subsystem, (900.0, -800.0, 150.0), (1.0, 7.0, 4.0), "WallBurstWall")

    # Lift/landing cluster. R lifts Source; Neighbor is inside the landing radius.
    spawn_dummy(subsystem, dummy_class, (250.0, 850.0, 120.0), "LiftSource")
    spawn_dummy(subsystem, dummy_class, (550.0, 850.0, 120.0), "LandingNeighbor")

    log("SUCCESS: player/game mode/floor ready; spawned 6 dummies + 2 test walls")
    log("Cable pair=(250,-220)/(850,220); cable wall=(-500,900); wall burst y=-800; lift y=850")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        unreal.log_error("PHYSICS_COMBO_TEST FAILED: %s" % exc)
        raise
