"""Create an isolated playable BossRing for AVectorPhysicsBoss PIE testing."""

import unreal


PREFIX = "PCG_BossTest_"


def log(message):
    unreal.log("BOSS_TEST: " + message)


def editor():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def load_class(path):
    actor_class = unreal.load_class(None, path)
    if actor_class is None:
        raise RuntimeError("could not load native class: %s" % path)
    return actor_class


def clear_previous():
    removed = 0
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor.get_actor_label().startswith(PREFIX):
            editor().destroy_actor(actor)
            removed += 1
    log("cleared %d previous actors" % removed)


def cube(location, scale, label):
    mesh = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
    actor = editor().spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*location))
    if mesh is None or actor is None:
        raise RuntimeError("failed to spawn cube: %s" % label)
    actor.static_mesh_component.set_static_mesh(mesh)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    actor.set_actor_label(PREFIX + label)
    return actor


def spawn(actor_class, location, label):
    actor = editor().spawn_actor_from_class(actor_class, unreal.Vector(*location))
    if actor is None:
        raise RuntimeError("failed to spawn actor: %s" % label)
    actor.set_actor_label(PREFIX + label)
    return actor


def main():
    classes = {
        "boss": load_class("/Script/Vector.VectorPhysicsBoss"),
        "enemy": load_class("/Script/Vector.VectorEnemy"),
        "player_start": load_class("/Script/Engine.PlayerStart"),
        "navmesh": load_class("/Script/NavigationSystem.NavMeshBoundsVolume"),
        "game_mode": load_class("/Script/Vector.VectorGameMode"),
    }
    archetype = getattr(unreal, "VectorEnemyArchetype", None)
    if archetype is None:
        archetype = getattr(unreal, "EVectorEnemyArchetype", None)
    if archetype is None:
        raise RuntimeError("VectorEnemyArchetype is not exposed to Python")

    clear_previous()
    world = unreal.EditorLevelLibrary.get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode", classes["game_mode"])

    cube((0.0, 0.0, -50.0), (36.0, 32.0, 0.5), "Floor")
    cube((0.0, 1600.0, 150.0), (36.0, 1.0, 4.0), "WallNorth")
    cube((0.0, -1600.0, 150.0), (36.0, 1.0, 4.0), "WallSouth")
    cube((1800.0, 0.0, 150.0), (1.0, 32.0, 4.0), "ImpactWallEast")
    cube((-1800.0, 0.0, 150.0), (1.0, 32.0, 4.0), "ImpactWallWest")
    for index, offset in enumerate(((-1450, -1250), (-1450, 1250), (1450, -1250), (1450, 1250))):
        cube((offset[0], offset[1], 200.0), (2.5, 2.5, 5.0), "Pillar_%02d" % index)
    cube((700.0, 1100.0, 125.0), (8.0, 4.0, 3.5), "HeightShelf")

    spawn(classes["player_start"], (-1200.0, 0.0, 120.0), "PlayerStart")
    spawn(classes["boss"], (450.0, 0.0, 140.0), "PhysicsBoss")
    add_a = spawn(classes["enemy"], (100.0, -650.0, 100.0), "AmmoLightA")
    add_b = spawn(classes["enemy"], (850.0, 650.0, 100.0), "AmmoLightB")
    add_a.set_editor_property("archetype", archetype.LIGHT_HOPPPER)
    add_b.set_editor_property("archetype", archetype.LIGHT_HOPPPER)

    navmesh = spawn(classes["navmesh"], (0.0, 0.0, 350.0), "NavMeshBounds")
    navmesh.set_actor_scale3d(unreal.Vector(40.0, 36.0, 8.0))
    log("SUCCESS: BossRing spawned with Boss + 2 light ammo enemies; Build Paths before PIE")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        unreal.log_error("BOSS_TEST FAILED: %s" % exc)
        raise
