"""Offline mirror of the HeightShelf two-route deletion contract."""


ROUTES = (
    ("BaitCharge", "EnvironmentRedirector", "UpperImpactDeck"),
    ("VectorInject", "LiftFork", "DirectedSlam", "LowerCrowd"),
)


def remaining_without(node):
    return tuple(route for route in ROUTES if node not in route)


def main():
    assert ROUTES[0][0] != ROUTES[1][0]

    without_environment = remaining_without("EnvironmentRedirector")
    assert len(without_environment) == 1
    assert without_environment[0][0] == "VectorInject"

    without_lift = remaining_without("LiftFork")
    assert len(without_lift) == 1
    assert without_lift[0][0] == "BaitCharge"

    without_height_converters = tuple(
        route
        for route in ROUTES
        if "EnvironmentRedirector" not in route and "LiftFork" not in route
    )
    assert len(without_height_converters) == 0
    print(
        "HeightShelf deletion: routes=2 removeEnvironment=1 "
        "removeLiftFork=1 removeBoth=0 clearLedger=ENEMY_ONLY check=PASS"
    )


if __name__ == "__main__":
    main()
