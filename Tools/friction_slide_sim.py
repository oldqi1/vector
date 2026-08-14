"""低摩擦/润滑剂停止距离离线对比（灰盒 CharacterMovement 近似）。"""


DT = 1.0 / 120.0


def simulate(initial_speed, braking_friction, braking_deceleration):
    speed = initial_speed
    distance = 0.0
    elapsed = 0.0
    while speed > 1.0 and elapsed < 30.0:
        deceleration = braking_friction * speed + braking_deceleration
        speed = max(0.0, speed - deceleration * DT)
        distance += speed * DT
        elapsed += DT
    return distance, elapsed


def main():
    cases = (
        ("Normal dummy", 0.50, 400.0),
        ("Lubricated", 0.01, 400.0 * (0.35 ** 0.5)),
        ("Low-friction zone", 0.01, 400.0 * (0.10 ** 0.5)),
    )
    results = []
    for label, friction, deceleration in cases:
        distance, elapsed = simulate(1400.0, friction, deceleration)
        results.append(distance)
        print(f"{label:18s} distance={distance:7.0f}cm stopTime={elapsed:5.2f}s")
    assert results[0] < results[1] < results[2]
    print("check=PASS")


if __name__ == "__main__":
    main()
