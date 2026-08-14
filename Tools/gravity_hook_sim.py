"""双端绳线枪离线复核：质量加权收绳、动量守恒与短绳角速度。"""

import math


DT = 1.0 / 120.0


def solve_pair(position_a, position_b, velocity_a, velocity_b,
               mass_a, mass_b, reel_speed, specific_h,
               max_tangent_speed=1800.0):
    dx = position_b[0] - position_a[0]
    dy = position_b[1] - position_a[1]
    distance = math.hypot(dx, dy)
    if distance <= 0.0 or mass_a <= 0.0 or mass_b <= 0.0:
        raise ValueError("invalid pair")
    radial = (dx / distance, dy / distance)
    tangent = (-radial[1], radial[0])
    relative = (velocity_b[0] - velocity_a[0],
                velocity_b[1] - velocity_a[1])
    current_radial = relative[0] * radial[0] + relative[1] * radial[1]
    desired_radial = min(current_radial, -max(0.0, reel_speed))
    tangent_speed = max(-max_tangent_speed,
                        min(max_tangent_speed, specific_h / distance))
    desired_relative = (
        radial[0] * desired_radial + tangent[0] * tangent_speed,
        radial[1] * desired_radial + tangent[1] * tangent_speed,
    )
    total_mass = mass_a + mass_b
    center_velocity = (
        (velocity_a[0] * mass_a + velocity_b[0] * mass_b) / total_mass,
        (velocity_a[1] * mass_a + velocity_b[1] * mass_b) / total_mass,
    )
    output_a = (
        center_velocity[0] - desired_relative[0] * mass_b / total_mass,
        center_velocity[1] - desired_relative[1] * mass_b / total_mass,
    )
    output_b = (
        center_velocity[0] + desired_relative[0] * mass_a / total_mass,
        center_velocity[1] + desired_relative[1] * mass_a / total_mass,
    )
    return output_a, output_b


def solve_tether(position_a, position_b, velocity_a, velocity_b,
                 mass_a, mass_b, cable_length, reel_speed,
                 specific_h, tolerance=3.0, dt=DT,
                 max_tangent_speed=1800.0, max_correction_speed=600.0):
    dx = position_b[0] - position_a[0]
    dy = position_b[1] - position_a[1]
    distance = math.hypot(dx, dy)
    if distance < cable_length - tolerance:
        return velocity_a, velocity_b, False
    radial = (dx / distance, dy / distance)
    tangent = (-radial[1], radial[0])
    relative = (velocity_b[0] - velocity_a[0],
                velocity_b[1] - velocity_a[1])
    radial_speed = relative[0] * radial[0] + relative[1] * radial[1]
    correction = min(max_correction_speed,
                     max(0.0, distance - cable_length) / max(dt, 1e-9))
    desired_radial = min(radial_speed, -max(0.0, reel_speed) - correction)
    tangent_speed = max(-max_tangent_speed,
                        min(max_tangent_speed, specific_h / distance))
    desired_relative = (
        radial[0] * desired_radial + tangent[0] * tangent_speed,
        radial[1] * desired_radial + tangent[1] * tangent_speed,
    )
    total_mass = mass_a + mass_b
    center_velocity = (
        (velocity_a[0] * mass_a + velocity_b[0] * mass_b) / total_mass,
        (velocity_a[1] * mass_a + velocity_b[1] * mass_b) / total_mass,
    )
    output_a = (
        center_velocity[0] - desired_relative[0] * mass_b / total_mass,
        center_velocity[1] - desired_relative[1] * mass_b / total_mass,
    )
    output_b = (
        center_velocity[0] + desired_relative[0] * mass_a / total_mass,
        center_velocity[1] + desired_relative[1] * mass_a / total_mass,
    )
    return output_a, output_b, True


def simulate_contact(mass_a, mass_b, start_distance=1000.0,
                     reel_speed=260.0, contact_distance=100.0):
    position_a = [0.0, 0.0]
    position_b = [start_distance, 0.0]
    velocity_a = (0.0, 0.0)
    velocity_b = (0.0, 0.0)
    cable_length = start_distance
    elapsed = 0.0
    while position_b[0] - position_a[0] > contact_distance and elapsed < 6.0:
        cable_length = max(0.0, cable_length - reel_speed * DT)
        velocity_a, velocity_b, _ = solve_tether(
            position_a, position_b, velocity_a, velocity_b,
            mass_a, mass_b, cable_length, 0.0, 0.0)
        position_a[0] += velocity_a[0] * DT
        position_b[0] += velocity_b[0] * DT
        elapsed += DT
    momentum = mass_a * velocity_a[0] + mass_b * velocity_b[0]
    return elapsed, position_a[0], position_b[0], velocity_a[0], velocity_b[0], momentum


def main():
    print("Non-elastic cable winch (start=1000cm, contact=100cm)")
    for label, masses in (("Light-Light", (1.25, 1.25)),
                          ("Light-Heavy", (1.25, 5.0))):
        result = simulate_contact(*masses)
        print(f"  {label:12s} time={result[0]:.2f}s "
              f"positions=({result[1]:.1f},{result[2]:.1f}) "
              f"velocities=({result[3]:.0f},{result[4]:.0f}) "
              f"momentum={result[5]:.6f}")
        assert abs(result[5]) < 1e-6

    specific_h = 400000.0
    far_tangent = specific_h / 800.0
    near_tangent = specific_h / 400.0
    far_omega = far_tangent / 800.0
    near_omega = near_tangent / 400.0
    print("Cable shortening angular response")
    print(f"  r=800: vt={far_tangent:.0f} omega={math.degrees(far_omega):.1f}deg/s")
    print(f"  r=400: vt={near_tangent:.0f} omega={math.degrees(near_omega):.1f}deg/s")
    assert math.isclose(near_tangent / far_tangent, 2.0)
    assert math.isclose(near_omega / far_omega, 4.0)

    slack_a, slack_b, slack_taut = solve_tether(
        (-400.0, 0.0), (400.0, 0.0),
        (-100.0, 20.0), (100.0, -20.0),
        1.0, 1.0, 1000.0, 0.0, 0.0)
    assert not slack_taut
    assert slack_a == (-100.0, 20.0) and slack_b == (100.0, -20.0)

    taut_a, taut_b, taut = solve_tether(
        (-500.0, 0.0), (500.0, 0.0),
        (-100.0, 0.0), (100.0, 0.0),
        1.0, 1.0, 1000.0, 0.0, 0.0)
    assert taut
    assert math.isclose(taut_b[0] - taut_a[0], 0.0)
    assert math.isclose(taut_a[0] + taut_b[0], 0.0)
    print("Non-elastic tether")
    print("  slack motion unchanged; taut outward speed removed; no spring rebound")

    fast_a, fast_b = solve_pair(
        (-500.0, 0.0), (500.0, 0.0),
        (1000.0, 0.0), (-1000.0, 0.0),
        1.0, 1.0, 900.0, 0.0)
    assert math.isclose(fast_a[0] - fast_b[0], 2000.0)
    print("check=PASS")


if __name__ == "__main__":
    main()
