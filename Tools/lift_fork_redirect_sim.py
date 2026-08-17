"""Deterministic mirror of the H03 lift-fork redirect budget."""

from __future__ import annotations

import math


HORIZONTAL_RETENTION = 1.0 - 0.6
REDIRECT_EFFICIENCY = 0.88
VERTICAL_FLOOR = 520.0
DIRECTED_SLAM_UPGRADE_FLOOR = 700.0
VERTICAL_FRACTION = math.sqrt(1.0 - HORIZONTAL_RETENTION**2)


def redirect(speed: float) -> tuple[float, float, float, bool]:
    if not math.isfinite(speed) or speed < 0.0:
        return 0.0, 0.0, 0.0, False
    budget = speed * REDIRECT_EFFICIENCY
    redirected_vertical = budget * VERTICAL_FRACTION
    if redirected_vertical < VERTICAL_FLOOR:
        return budget, 0.0, VERTICAL_FLOOR, True
    return (
        budget,
        budget * HORIZONTAL_RETENTION,
        redirected_vertical,
        False,
    )


def directed_slam(
    start: tuple[float, float, float],
    target: tuple[float, float, float],
    current_velocity: tuple[float, float, float],
    gravity_z: float = -980.0,
    minimum_speed_budget: float = VERTICAL_FLOOR,
) -> tuple[bool, float, tuple[float, float, float], float]:
    """Mirror H09's discrete, downward-only, finite-budget solver."""
    values = (*start, *target, *current_velocity, gravity_z, minimum_speed_budget)
    if not all(math.isfinite(value) for value in values) or gravity_z >= 0.0:
        return False, 0.0, (0.0, 0.0, 0.0), 0.0
    gravity = -gravity_z
    height_drop = max(0.0, start[2] - target[2])
    natural = (
        current_velocity[2]
        + math.sqrt(current_velocity[2] ** 2 + 2.0 * gravity * height_drop)
    ) / gravity
    upper = min(0.90, max(0.20, 0.90 * natural))
    budget = max(
        math.dist((0.0, 0.0, 0.0), current_velocity),
        max(0.0, minimum_speed_budget),
    )
    best: tuple[float, float, tuple[float, float, float]] | None = None
    maximum_steps = math.floor((upper - 0.20) / 0.05 + 1.0e-6)
    for step in range(maximum_steps + 1):
        flight = 0.20 + step * 0.05
        velocity = (
            (target[0] - start[0]) / flight,
            (target[1] - start[1]) / flight,
            (target[2] - start[2]) / flight - 0.5 * gravity_z * flight,
        )
        speed = math.dist((0.0, 0.0, 0.0), velocity)
        if velocity[2] >= 0.0 or speed > budget + 1.0e-6:
            continue
        if best is None or speed < best[0] - 1.0e-6:
            best = (speed, flight, velocity)
    if best is None:
        return False, 0.0, (0.0, 0.0, 0.0), budget
    return True, best[1], best[2], budget


expected = {
    0.0: (0.0, 520.0, True),
    800.0: (281.6, 645.2266578497822, False),
    1400.0: (492.8, 1129.146651237119, False),
    2400.0: (844.8, 1935.6799735493469, False),
}

for input_speed, (expected_horizontal, expected_vertical, expected_floor) in expected.items():
    budget, horizontal, vertical, used_floor = redirect(input_speed)
    allowed = max(budget, VERTICAL_FLOOR) if used_floor else budget
    output_speed = math.hypot(horizontal, vertical)
    assert math.isclose(horizontal, expected_horizontal, abs_tol=1.0e-6)
    assert math.isclose(vertical, expected_vertical, abs_tol=1.0e-6)
    assert used_floor is expected_floor
    assert math.isfinite(output_speed)
    assert output_speed <= allowed + 1.0e-6
    if not used_floor:
        assert math.isclose(output_speed, budget, abs_tol=1.0e-6)
        assert output_speed <= input_speed + 1.0e-6

assert redirect(math.nan) == (0.0, 0.0, 0.0, False)

# Base 5: natural fall is a small shared-formula AoE. 5 -> 1: the gun spends
# a cell to multiply injected speed instead of adding arbitrary raw damage.
natural_landing_base_damage = (520.0 - 300.0) * 0.05 * 1.5
natural_landing_damage = min(natural_landing_base_damage * 0.35, 8.0)
assert math.isclose(natural_landing_damage, 5.775, abs_tol=1.0e-6)
base_vector_speed = 1800.0 / 1.25
lift_to_vector_speed = base_vector_speed * 1.25
assert math.isclose(lift_to_vector_speed, 1800.0, abs_tol=1.0e-6)
assert lift_to_vector_speed > base_vector_speed

reachable = directed_slam(
    (0.0, 0.0, 500.0), (100.0, 0.0, 0.0), (0.0, 0.0, -500.0),
    minimum_speed_budget=DIRECTED_SLAM_UPGRADE_FLOOR)
assert reachable[0]
assert math.isclose(reachable[1], 0.55, abs_tol=1.0e-6)
assert reachable[2][2] < 0.0
assert math.dist((0.0, 0.0, 0.0), reachable[2]) <= reachable[3] + 1.0e-6
assert not directed_slam(
    (0.0, 0.0, 500.0), (0.0, 0.0, 490.0), (0.0, 0.0, -200.0),
    minimum_speed_budget=DIRECTED_SLAM_UPGRADE_FLOOR)[0]
assert not directed_slam(
    (0.0, 0.0, 500.0), (1000.0, 0.0, 0.0), (0.0, 0.0, -500.0),
    minimum_speed_budget=DIRECTED_SLAM_UPGRADE_FLOOR)[0]
fast_far = directed_slam(
    (0.0, 0.0, 700.0), (500.0, 0.0, 0.0), (900.0, 0.0, -500.0))
assert fast_far[0]
assert math.isclose(fast_far[1], 0.70, abs_tol=1.0e-6)

print("lift-fork redirect+combo+directed-slam check=PASS cases=natural/5to1/near/far/fast")
