"""Deterministic reference numbers for wall burst and lift-fork playtests."""

import math


PHYSICAL_MASS = {
    "Light stable": 1.25,
    "Medium stable": 2.50,
    "Heavy stable": 5.00,
    "Light staggered": 1.00,
    "Medium staggered": 1.50,
    "Heavy staggered": 2.00,
}

HAMMER_IMPULSE = 1750.0
WALL_THRESHOLD = 800.0
WALL_DAMAGE_PER_SPEED = 0.04
LIGHT_DAMAGE_MASS_MULTIPLIER = 0.8
WALL_BURST_BASE_SPEED = 1400.0
WALL_CONTACT_TRAVEL = 458.0
GROUND_FRICTION_RATE = 0.5
BRAKING_DECELERATION = 400.0
LIFT_BASE_SPEED = 1900.0
LANDING_THRESHOLD = 600.0
GRAVITY = 980.0
BUOYANT_MULTIPLIER = 0.35


def mass_speed(base_speed, mass):
    if not math.isfinite(base_speed) or not math.isfinite(mass):
        return 0.0
    if base_speed <= 0.0 or mass <= 0.0:
        return 0.0
    return base_speed / max(0.1, mass)


def wall_damage(speed):
    if speed <= WALL_THRESHOLD:
        return 0.0
    return min(
        40.0,
        (speed - WALL_THRESHOLD)
        * WALL_DAMAGE_PER_SPEED
        * LIGHT_DAMAGE_MASS_MULTIPLIER,
    )


def speed_after_ground_travel(initial_speed, distance, time_step=1.0 / 2000.0):
    speed = max(0.0, initial_speed)
    traveled = 0.0
    while speed > 0.0 and traveled < distance:
        acceleration = GROUND_FRICTION_RATE * speed + BRAKING_DECELERATION
        speed = max(0.0, speed - acceleration * time_step)
        traveled += speed * time_step
    return speed


def main():
    wall_source_speed = mass_speed(HAMMER_IMPULSE, PHYSICAL_MASS["Light stable"])
    wall_impact_speed = speed_after_ground_travel(
        wall_source_speed, WALL_CONTACT_TRAVEL)
    print("Wall-burst fixture")
    print("  light source launch speed = %.0f cm/s" % wall_source_speed)
    print("  estimated wall speed      = %.0f cm/s" % wall_impact_speed)
    print("  threshold                 = %.0f cm/s" % WALL_THRESHOLD)
    print("  estimated AOE damage      = %.1f" % wall_damage(wall_impact_speed))
    for name in ("Light stable", "Medium stable", "Heavy stable"):
        print(
            "  %-16s burst speed = %4.0f cm/s"
            % (name, mass_speed(WALL_BURST_BASE_SPEED, PHYSICAL_MASS[name]))
        )

    print("Lift-fork fixture")
    for name in PHYSICAL_MASS:
        speed = mass_speed(LIFT_BASE_SPEED, PHYSICAL_MASS[name])
        normal_apex_seconds = speed / GRAVITY
        buoyant_apex_seconds = speed / (GRAVITY * BUOYANT_MULTIPLIER)
        print(
            "  %-17s lift=%4.0f shock=%-3s apex=%.2fs buoyant=%.2fs"
            % (
                name,
                speed,
                "YES" if speed > LANDING_THRESHOLD else "no",
                normal_apex_seconds,
                buoyant_apex_seconds,
            )
        )

    checks = [
        wall_source_speed > WALL_THRESHOLD,
        math.isclose(wall_source_speed, 1400.0),
        wall_impact_speed > WALL_THRESHOLD,
        1000.0 < wall_impact_speed < 1040.0,
        6.0 < wall_damage(wall_impact_speed) < 8.0,
        mass_speed(LIFT_BASE_SPEED, PHYSICAL_MASS["Medium stable"])
        > LANDING_THRESHOLD,
        mass_speed(LIFT_BASE_SPEED, PHYSICAL_MASS["Heavy stable"])
        < LANDING_THRESHOLD,
        mass_speed(LIFT_BASE_SPEED, PHYSICAL_MASS["Heavy staggered"])
        > LANDING_THRESHOLD,
    ]
    print("check=%s" % ("PASS" if all(checks) else "FAIL"))
    raise SystemExit(0 if all(checks) else 1)


if __name__ == "__main__":
    main()
