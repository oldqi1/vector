# 冲量荒原 · 灰盒竞技场一键生成脚本（Editor Python）
#
# 用法：编辑器 Python 控制台（菜单 Tools → Execute Python Script...）运行本文件，
# 或控制台执行 exec(open(r"C:/workspace/Vector/Tools/Greybox/setup_greybox_arena.py").read())
#
# 生成内容（对齐 Design/原型设计基线_v0.1.md §4 六模块布局）：
#   1. 地面 + 竞技场边界墙
#   2. 狭窄巷道（双墙）、坡道高台（旋转薄块）、低摩擦带（蓝色地面标记）
#   3. 敌人 9 只：开放区（跳囊虫3+角槌兽1+甲壳犀1）、巷道（跳囊虫2+甲壳犀1）、坡道（跳囊虫1+角槌兽1）
#   4. NavMeshBoundsVolume 覆盖全图（之后在编辑器顶部 Build → Build Paths 烘焙导航）
#
# 可重跑：每次先清理旧生成物（Label 前缀 "GA_"），再重建。

import unreal

PREFIX = "GA_"  # 本脚本生成的 Actor 标签前缀，重跑时按此前缀清理

# ---------------------------------------------------------------- 工具函数

def log(msg):
    unreal.log("GREYBOX: " + msg)

def get_editor_subsystem():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

def clear_previous(level_actor_subsystem):
    """删除上一次运行生成的 GA_* Actor（重跑幂等）。"""
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    removed = 0
    for actor in actors:
        if actor.get_actor_label().startswith(PREFIX):
            level_actor_subsystem.destroy_actor(actor)
            removed += 1
    if removed:
        log("cleared %d previous actors" % removed)

def spawn_cube(location, scale, color=None, label="GA_cube"):
    """生成立方体静态网格 Actor（默认 BasicShapes Cube）。"""
    mesh = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
    actor = get_editor_subsystem().spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(location[0], location[1], location[2]))
    actor.static_mesh_component.set_static_mesh(mesh)
    actor.set_actor_scale3d(unreal.Vector(scale[0], scale[1], scale[2]))
    actor.set_actor_label(label)
    if color is not None:
        mat = unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")
        mi = unreal.MaterialInstanceConstant()  # 灰盒期不建 MI，直接跳过着色（保持默认灰）
    return actor

def spawn_floor_and_walls():
    """地面 + 边界墙 + 巷道 + 坡道 + 低摩擦带。"""
    # 地面：8000x8000 薄板（Cube 拉伸，中心 0,0,0）
    spawn_cube((0, 0, -50), (80, 80, 0.5), label="GA_Floor")
    # 边界墙（高 300，厚 100）：四边
    half = 4000
    wall_h = 3.0   # 300cm
    wall_t = 1.0   # 100cm
    wall_l = 80.0  # 8000cm 长墙
    spawn_cube((0, half + 50, 100), (wall_l, wall_t, wall_h), label="GA_Wall_N")
    spawn_cube((0, -half - 50, 100), (wall_l, wall_t, wall_h), label="GA_Wall_S")
    spawn_cube((half + 50, 0, 100), (wall_t, wall_l, wall_h), label="GA_Wall_E")
    spawn_cube((-half - 50, 0, 100), (wall_t, wall_l, wall_h), label="GA_Wall_W")

    # 狭窄巷道：东南区（x 1000~3000, y -3000~-1000），两条墙成 6m 宽通道
    # 巷道墙 1：沿 X 方向，y = -1300
    spawn_cube((2000, -1300, 100), (25.0, 1.0, 3.0), label="GA_LaneWall_1")
    # 巷道墙 2：沿 X 方向，y = -1900
    spawn_cube((2000, -1900, 100), (25.0, 1.0, 3.0), label="GA_LaneWall_2")
    # 巷道末端墙（东侧封闭，西侧开口）
    spawn_cube((3250, -1600, 100), (1.0, 7.0, 3.0), label="GA_LaneEnd")

    # 坡道高台：西北区，旋转 15° 的薄块（x -2500~-1500, y 1500~2500）
    ramp = spawn_cube((-2000, 2000, 100), (10.0, 20.0, 0.6), label="GA_Ramp")
    ramp.set_actor_rotation(unreal.Rotator(0.0, 0.0, 0.0), False)  # 平放；斜坡用台阶简化（灰盒）
    # 用两级台阶近似斜坡（简单可靠，避免旋转碰撞问题）
    spawn_cube((-1600, 2000, 80), (5.0, 20.0, 0.8), label="GA_RampStep1")
    spawn_cube((-1200, 2000, 130), (5.0, 20.0, 0.8), label="GA_RampStep2")

    # 低摩擦带：西南区标记（蓝色地面块，物理摩擦后续用物理材质接入）
    spawn_cube((-2500, -2000, -45), (12.0, 12.0, 0.15), label="GA_LowFrictionZone")

def spawn_enemy(archetype_enum, location, label):
    """生成一只敌人并按三型配置。"""
    cls = unreal.load_class("/Script/Vector.VectorEnemy")
    actor = get_editor_subsystem().spawn_actor_from_class(
        cls, unreal.Vector(location[0], location[1], location[2]))
    actor.set_actor_label(label)
    actor.set_editor_property("Archetype", archetype_enum)
    return actor

def spawn_enemies():
    """开放区 6 + 巷道 3 + 坡道 2 = 11 只（验收 #7 同屏 8-12 敌）。"""
    light = unreal.EVectorEnemyArchetype.LIGHT_HOPPPER
    heavy = unreal.EVectorEnemyArchetype.HEAVY_RHINO_BEETLE
    charger = unreal.EVectorEnemyArchetype.CHARGER_RAMMER
    # 开放竞技场（中央偏东）
    spawn_enemy(light, (1000, 500, 100), "GA_Enemy_Open_Light1")
    spawn_enemy(light, (1400, 800, 100), "GA_Enemy_Open_Light2")
    spawn_enemy(light, (900, 1000, 100), "GA_Enemy_Open_Light3")
    spawn_enemy(charger, (1500, 300, 100), "GA_Enemy_Open_Charger1")
    spawn_enemy(heavy, (600, 600, 100), "GA_Enemy_Open_Heavy1")
    # 狭窄巷道
    spawn_enemy(light, (2200, -1600, 100), "GA_Enemy_Lane_Light1")
    spawn_enemy(light, (2700, -1700, 100), "GA_Enemy_Lane_Light2")
    spawn_enemy(heavy, (2000, -1400, 100), "GA_Enemy_Lane_Heavy1")
    # 坡道高台
    spawn_enemy(light, (-1600, 2100, 100), "GA_Enemy_Ramp_Light1")
    spawn_enemy(charger, (-1900, 1900, 100), "GA_Enemy_Ramp_Charger1")

def spawn_navmesh():
    """NavMeshBoundsVolume 覆盖全图（之后需 Build Paths 烘焙）。"""
    cls = unreal.load_class("/Script/NavigationSystem.NavMeshBoundsVolume")
    actor = get_editor_subsystem().spawn_actor_from_class(
        cls, unreal.Vector(0, 0, 300))
    actor.set_actor_label("GA_NavMeshBounds")
    actor.set_actor_scale3d(unreal.Vector(85.0, 85.0, 8.0))
    return actor

# ---------------------------------------------------------------- 主流程

def main():
    level_subsystem = get_editor_subsystem()
    clear_previous(level_subsystem)
    spawn_floor_and_walls()
    spawn_enemies()
    spawn_navmesh()
    log("greybox arena generated. Next: Build -> Build Paths (P) to bake navmesh, then Play.")

if __name__ == "__main__":
    main()
