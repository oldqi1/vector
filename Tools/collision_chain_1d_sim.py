# 一维碰撞连锁模拟：复刻 Vector 工程当前物理参数（2026-08-14）
# 目的：验证"两只紧挨跳囊虫，满蓄推近的一只"是否应该产生连锁。
# 参数全部来自源码：
#   - 冲量锤满蓄：I=1750，轻质质量 1.25 → Δv=1400 cm/s
#   - 靶子制动（VectorTestDummy.cpp）：GroundFriction=1.0, BrakingFrictionFactor=0.5,
#     BrakingDecelerationWalking=400 → a(v) = -0.5*v - 400
#   - 碰撞（VectorImpactCollisionComponent）：阈值 300，轻质系数 0.8，上限 50，
#     双质量 + 恢复系数 e=0.7 的一维守恒解算
#   - ACharacter 默认胶囊半径 34cm

import math

FRICTION = 0.5      # BrakingFrictionFactor * GroundFriction
BRAKING = 400.0     # BrakingDecelerationWalking
DT = 1.0 / 60.0     # 帧长
RESTITUTION = 0.7

def braking(v, dt=DT):
    """UE ApplyVelocityBraking 一维近似：a = -Friction*v - Braking（v>0 时）。"""
    if v <= 0.0:
        return 0.0
    a = -FRICTION * v - BRAKING
    dv = a * dt
    if v + dv < 0.0:
        dv = -v
    return v + dv

def slide_distance(v0):
    """从 v0 开始滑行到停止的总距离。"""
    v = v0
    d = 0.0
    while v > 1.0:
        d += v * DT
        v = braking(v)
    return d

def frame_step(v, dist):
    """推进一帧，返回 (新速度, 本帧位移)。"""
    d = v * DT
    return braking(v), d

def collide(u1, u2, m1, m2, restitution=RESTITUTION):
    """与 FVectorImpactMath::SolveOneDimensionalCollision 相同的一维公式。"""
    e = max(0.0, min(1.0, restitution))
    total = m1 + m2
    v1 = ((m1 - e * m2) * u1 + (1.0 + e) * m2 * u2) / total
    v2 = ((1.0 + e) * m1 * u1 + (m2 - e * m1) * u2) / total
    return v1, v2

print("=" * 60)
print("场景：两只跳囊虫（轻质 1.25）紧挨，满蓄推近的一只 A")
print("=" * 60)

# 满蓄推 A：Δv = 1750 / 1.25 = 1400
v_a = 1750.0 / 1.25
print(f"\nA 获得冲量：Δv = {v_a:.0f} cm/s")

# 两只"紧挨"：A 到 B 的中心距（胶囊半径 34*2 = 68cm，贴脸）
gap = 68.0
print(f"A-B 间距：{gap:.0f} cm（胶囊贴脸）")

# A 飞向 B：逐帧推进直到接触
d = 0.0
v = v_a
frames = 0
while d < gap and v > 1.0:
    v, step = frame_step(v, d)
    d += step
    frames += 1

impact_v = v
print(f"A 撞到 B 时：速度 {impact_v:.0f} cm/s（{frames} 帧，位移 {d:.0f} cm）")

# 碰撞结算
threshold = 300.0
if impact_v > threshold:
    damage = min((impact_v - threshold) * 0.05 * 0.8 * 1.0, 50.0)  # 轻质系数 0.8，Body 1.0
    print(f"→ 触发碰撞伤害：({impact_v:.0f}-300)*0.05*0.8 = {damage:.1f}（上限50）")
    print(f"  B 生命：100 → {100 - damage:.1f}")
else:
    print(f"→ 速度 {impact_v:.0f} < 阈值 300，无碰撞伤害！")
    damage = 0.0

# 等质量守恒碰撞：e=0.7 时撞击者剩 15%，目标获得 85%。
mass = 1.25
v_a_after, v_b_after = collide(impact_v, 0.0, mass, mass)
momentum_before = mass * impact_v
momentum_after = mass * v_a_after + mass * v_b_after
energy_before = 0.5 * mass * impact_v**2
energy_after = 0.5 * mass * v_a_after**2 + 0.5 * mass * v_b_after**2
print(f"→ 碰撞解算：A={v_a_after:.0f} cm/s，B={v_b_after:.0f} cm/s（e={RESTITUTION}）")
print(f"→ 动量：{momentum_before:.1f} → {momentum_after:.1f}")
print(f"→ 动能：{energy_before:.0f} → {energy_after:.0f}（不得增加）")

# B 滑行距离
dist_b = slide_distance(v_b_after)
print(f"→ B 滑行距离：{dist_b:.0f} cm（{dist_b/100:.1f} m）")

print("\n" + "=" * 60)
print("对照：单只 A 自由滑行（无 B 阻挡）")
print("=" * 60)
dist_a = slide_distance(1400)
print(f"A 单独滑行距离：{dist_a:.0f} cm（{dist_a/100:.1f} m）")

print("\n" + "=" * 60)
print("对照：B 紧贴 A（间距 10cm，几乎重叠）")
print("=" * 60)
gap2 = 10.0
d = 0.0
v = 1400.0
frames = 0
while d < gap2 and v > 1.0:
    v, step = frame_step(v, d)
    d += step
    frames += 1
print(f"A 撞到 B 时：速度 {v:.0f} cm/s（{frames} 帧）")
if v > threshold:
    dmg2 = min((v - threshold) * 0.05 * 0.8, 50.0)
    v1_after, v2_after = collide(v, 0.0, mass, mass)
    print(f"→ 碰撞伤害 {dmg2:.1f}，碰后 A={v1_after:.0f} / B={v2_after:.0f} cm/s")
else:
    print(f"→ 低于阈值 300，无伤害！B 完全不受影响")

assert damage > 0.0, "expected adjacent target to receive collision damage"
assert math.isclose(momentum_before, momentum_after, rel_tol=1e-9, abs_tol=1e-6), "momentum drift"
assert energy_after <= energy_before + 1e-6, "collision gained kinetic energy"
assert dist_b >= 800.0, "chain target did not travel far enough to be useful"
print("check=PASS")
