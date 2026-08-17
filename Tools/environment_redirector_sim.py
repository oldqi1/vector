"""Deterministic mirror of the H11 environment redirector budget and gates."""

import math


def redirect(incoming, exit_direction, efficiency):
    if not 0.0 <= efficiency <= 1.0:
        return None
    if not all(math.isfinite(value) for value in incoming + exit_direction):
        return None
    input_speed = math.sqrt(sum(value * value for value in incoming))
    exit_size = math.sqrt(sum(value * value for value in exit_direction))
    if input_speed <= 1.0e-4 or exit_size <= 1.0e-4:
        return None
    output_speed = input_speed * efficiency
    output = tuple(value / exit_size * output_speed for value in exit_direction)
    return input_speed, output_speed, output


def should_consume(impulse_driven, consumed, speed, minimum_speed):
    return (
        impulse_driven
        and not consumed
        and math.isfinite(speed)
        and math.isfinite(minimum_speed)
        and minimum_speed >= 0.0
        and speed >= minimum_speed
    )


def close(a, b, tolerance=1.0e-6):
    return abs(a - b) <= tolerance


def main():
    horizontal = redirect((600.0, 800.0, 0.0), (0.0, 1.0, 0.0), 0.88)
    assert horizontal is not None
    assert close(horizontal[0], 1000.0)
    assert close(horizontal[1], 880.0)
    assert all(close(a, b) for a, b in zip(horizontal[2], (0.0, 880.0, 0.0)))

    vertical = redirect((0.0, -1200.0, 500.0), (1.0, 0.0, 1.0), 0.75)
    assert vertical is not None
    assert close(vertical[0], 1300.0)
    assert close(vertical[1], 975.0)
    assert math.sqrt(sum(value * value for value in vertical[2])) <= vertical[0]

    assert not should_consume(False, False, 900.0, 220.0)
    assert not should_consume(True, True, 900.0, 220.0)
    assert not should_consume(True, False, 219.9, 220.0)
    assert should_consume(True, False, 220.0, 220.0)
    assert redirect((1000.0, 0.0, 0.0), (1.0, 0.0, 0.0), 1.01) is None
    assert redirect((math.nan, 0.0, 0.0), (1.0, 0.0, 0.0), 0.88) is None
    print("environment redirector deterministic budget/gates check=PASS")


if __name__ == "__main__":
    main()
