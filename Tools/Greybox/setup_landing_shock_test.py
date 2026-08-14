"""在当前灰盒关卡布置可重复的落地震荡测试。

使用 Unreal Editor 菜单 Tools -> Execute Python Script... 运行本文件，
然后使用顶部普通 Play。高处测试靶会自由落地，并震到旁边的地面测试靶。
"""

import unreal


PREFIX = "GA_LandingTest_"
SOURCE_LOCATION = unreal.Vector(-3000.0, -250.0, 950.0)
TARGET_LOCATION = unreal.Vector(-2700.0, -250.0, 120.0)


def log(message):
    unreal.log("LANDING_TEST: " + message)


def load_native_class(path):
    cls = unreal.load_class(None, path)
    if cls is None:
        raise RuntimeError("could not load native class: %s" % path)
    return cls


def clear_previous(subsystem):
    removed = 0
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor.get_actor_label().startswith(PREFIX):
            subsystem.destroy_actor(actor)
            removed += 1
    if removed:
        log("cleared %d previous test actors" % removed)


def spawn_dummy(subsystem, dummy_class, location, label):
    actor = subsystem.spawn_actor_from_class(dummy_class, location)
    if actor is None:
        raise RuntimeError("failed to spawn %s" % label)
    actor.set_actor_label(label)
    return actor


def main():
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    dummy_class = load_native_class("/Script/Vector.VectorTestDummy")
    clear_previous(subsystem)

    spawn_dummy(
        subsystem,
        dummy_class,
        TARGET_LOCATION,
        "GA_LandingTest_GroundTarget")
    spawn_dummy(
        subsystem,
        dummy_class,
        SOURCE_LOCATION,
        "GA_LandingTest_FallingSource")

    log("SUCCESS: source Z=950, target offset=300cm. Press normal Play and watch LogVectorImpact.")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        unreal.log_error("LANDING_TEST FAILED: %s" % exc)
        raise
