$ErrorActionPreference = "Stop"

$RepositoryRoot = Split-Path -Parent $PSScriptRoot
Push-Location $RepositoryRoot
try {
    Write-Output "[1/4] Checking staged and unstaged patch whitespace..."
    & git diff --check
    if ($LASTEXITCODE -ne 0) {
        throw "git diff --check failed"
    }

    Write-Output "[2/4] Checking Unreal Automation registrations..."
    $TestFiles = Get-ChildItem "Vector/Source/Vector/Private/Tests" -Filter "*.cpp" -File
    $RegistrationCount = ($TestFiles | Select-String -Pattern '^IMPLEMENT_(SIMPLE|COMPLEX)_AUTOMATION_TEST\(').Count
    if ($RegistrationCount -ne 50) {
        throw "Expected 50 Automation registrations, found $RegistrationCount"
    }
    Write-Output "Automation registrations: $RegistrationCount"

    Write-Output "[3/4] Parsing Unreal Editor Python scripts..."
    $EditorScripts = @(
        Get-ChildItem "Tools/Greybox", "Tools/Art" -Filter "*.py" -File
    )
    foreach ($Script in $EditorScripts) {
        & python -B -c 'import ast, pathlib, sys; ast.parse(pathlib.Path(sys.argv[1]).read_bytes())' $Script.FullName
        if ($LASTEXITCODE -ne 0) {
            throw "Python syntax check failed: $($Script.FullName)"
        }
    }
    Write-Output "Unreal Editor Python scripts: $($EditorScripts.Count) parsed"

    Write-Output "[4/4] Running deterministic physics simulations..."
    $Simulations = @(
        "Tools/collision_chain_1d_sim.py",
        "Tools/friction_slide_sim.py",
        "Tools/gravity_hook_sim.py",
        "Tools/environment_redirector_sim.py",
        "Tools/lift_fork_redirect_sim.py",
        "Tools/physics_combo_sim.py"
    )
    foreach ($Simulation in $Simulations) {
        $Output = & python -B $Simulation
        if ($LASTEXITCODE -ne 0) {
            throw "Physics simulation failed: $Simulation"
        }
        if (-not ($Output -match 'check=PASS')) {
            throw "Physics simulation did not report PASS: $Simulation"
        }
        Write-Output "PASS: $Simulation"
    }

    $RequiredWiring = @(
        @{ Path = "Vector/Source/Vector/Private/Hunt/VectorEncounterComponent.cpp"; Text = "OnEncounterCompleted.Broadcast" },
        @{ Path = "Vector/Source/Vector/Private/Hunt/VectorExtractionZone.cpp"; Text = "Hunt run summary" },
        @{ Path = "Vector/Source/Vector/Private/Hunt/VectorExtractionZone.cpp"; Text = "Hunt PCG summary" },
        @{ Path = "Vector/Source/Vector/Private/VectorHUD.cpp"; Text = "EXIT LOCKED - CLEAR ALL" },
        @{ Path = "Vector/Source/Vector/Private/VectorHUD.cpp"; Text = "ROOM CLEAR - ADVANCE" },
        @{ Path = "Tools/Greybox/setup_greybox_arena.py"; Text = "GA_ExtractionZone" },
        @{ Path = "Vector/Source/Vector/Private/PCG/VectorTacticalGenerationLibrary.cpp"; Text = "FVectorTacticalGenerator::Generate" },
        @{ Path = "Tools/Greybox/setup_tactical_pcg_preview.py"; Text = "generate_module_sequence" },
        @{ Path = "Tools/Greybox/setup_tactical_pcg_preview.py"; Text = "BossRing_BossSpawn" },
        @{ Path = "Tools/Greybox/setup_tactical_pcg_preview.py"; Text = "encounter_wave_one_spawns" },
        @{ Path = "Vector/Source/Vector/Private/PCG/VectorPCGEncounterDirector.cpp"; Text = "BeginDynamicEncounter" },
        @{ Path = "Vector/Source/Vector/Private/PCG/VectorPCGEncounterDirector.cpp"; Text = "SealDynamicEncounter" },
        @{ Path = "Vector/Source/Vector/Private/PCG/VectorPCGEncounterDirector.cpp"; Text = "SetZoneActive" },
        @{ Path = "Tools/Greybox/setup_tactical_pcg_preview.py"; Text = "boss_overload_friction_zone" },
        @{ Path = "Tools/Greybox/setup_tactical_pcg_preview.py"; Text = "wave_one_exit_gate" },
        @{ Path = "Vector/Source/Vector/Private/PCG/VectorPCGEncounterDirector.cpp"; Text = "SetGateLocked(false)" },
        @{ Path = "Vector/Source/Vector/Private/PCG/VectorPCGWaveGate.cpp"; Text = "SetCanEverAffectNavigation(false)" },
        @{ Path = "Vector/Source/Vector/Private/PCG/VectorPCGEncounterDirector.cpp"; Text = "HandleRoomEntered" },
        @{ Path = "Vector/Source/Vector/Private/PCG/VectorPCGEncounterDirector.cpp"; Text = "configuration rejected" },
        @{ Path = "Vector/Source/Vector/Private/PCG/VectorPCGEncounterDirector.cpp"; Text = "SelectModuleArchetype" },
        @{ Path = "Vector/Source/Vector/Private/PCG/VectorPCGEncounterDirector.cpp"; Text = "PCG enemy slot: module=" },
        @{ Path = "Tools/Greybox/setup_tactical_pcg_preview.py"; Text = "room_activation_triggers" },
        @{ Path = "Vector/Source/Vector/Private/PCG/VectorPCGRoomTrigger.cpp"; Text = "SetRespawnCheckpoint" },
        @{ Path = "Vector/Source/Vector/Private/VectorHUD.cpp"; Text = "MAGNET-SHELL BEAST" },
        @{ Path = "Vector/Source/Vector/Private/VectorHUD.cpp"; Text = "WAVE %d / 3" },
        @{ Path = "Vector/Source/Vector/Private/VectorHUD.cpp"; Text = "GetActiveModuleName" },
        @{ Path = "Vector/Source/Vector/Private/Boss/VectorPhysicsBoss.cpp"; Text = "QueueDirectionalVelocityOverride" },
        @{ Path = "Vector/Source/Vector/Private/Boss/VectorPhysicsBoss.cpp"; Text = "Boss slam landed" },
        @{ Path = "Vector/Source/Vector/Private/Boss/VectorPhysicsBoss.cpp"; Text = "Boss aerial burst released" },
        @{ Path = "Vector/Source/Vector/Private/Boss/VectorPhysicsBoss.cpp"; Text = "Boss ammo arc launch" },
		@{ Path = "Vector/Source/Vector/Private/Boss/VectorPhysicsBoss.cpp"; Text = "Boss reusable ammo release" },
		@{ Path = "Vector/Source/Vector/Private/Boss/VectorPhysicsBoss.cpp"; Text = "Boss fallback ammo supply:" },
		@{ Path = "Vector/Source/Vector/Private/Boss/VectorPhysicsBoss.cpp"; Text = "Boss stagger accepted:" },
		@{ Path = "Vector/Source/Vector/Private/Boss/VectorPhysicsBoss.cpp"; Text = "Boss stagger rejected:" },
		@{ Path = "Vector/Source/Vector/Private/VectorHUD.cpp"; Text = "| RESOLVE" },
		@{ Path = "Vector/Source/Vector/Private/VectorHUD.cpp"; Text = "VECTOR CYAN ORB INTO EITHER SIDE" },
		@{ Path = "Vector/Source/Vector/Private/Combat/VectorBreakableAnchorComponent.cpp"; Text = "Structure collision:" },
        @{ Path = "Vector/Source/Vector/Private/PCG/VectorTacticalLayout.cpp"; Text = "route can collapse to one height layer" },
        @{ Path = "Tools/Greybox/setup_tactical_pcg_preview.py"; Text = "spawn_ramp_x" },
        @{ Path = "Tools/Greybox/setup_tactical_pcg_preview.py"; Text = "spawn_split_receiver_wall" },
        @{ Path = "Tools/Greybox/setup_tactical_pcg_preview.py"; Text = "permanentExitGap" },
        @{ Path = "Vector/Source/Vector/Private/Gameplay/VectorCharacterMovementComponent.cpp"; Text = "Airborne launch consumed" },
		@{ Path = "Vector/Source/Vector/Private/Combat/VectorLiftForkComponent.cpp"; Text = "FVectorLiftForkMath::ComputeVerticalRedirect" },
		@{ Path = "Vector/Source/Vector/Private/Combat/VectorLiftForkComponent.cpp"; Text = "QueueAirborneWorldVelocityOverride" },
		@{ Path = "Vector/Source/Vector/Private/Combat/VectorLiftForkComponent.cpp"; Text = "floorUsed=%s" },
		@{ Path = "Vector/Source/Vector/Private/Combat/VectorLiftForkComponent.cpp"; Text = "Directed slam queued:" },
		@{ Path = "Vector/Source/Vector/Private/Combat/VectorLiftForkComponent.cpp"; Text = "BeginForkGesture" },
		@{ Path = "Vector/Source/Vector/Private/Combat/VectorLiftForkComponent.cpp"; Text = "ReleaseForkGesture" },
		@{ Path = "Vector/Source/Vector/Private/Combat/VectorLiftForkComponent.cpp"; Text = "mode=LIFT_ONLY" },
		@{ Path = "Vector/Source/Vector/Private/Combat/VectorLiftForkComponent.cpp"; Text = "reason=UPGRADE_LOCKED" },
		@{ Path = "Vector/Source/Vector/Private/Combat/VectorGunComponent.cpp"; Text = "type=LIFT_VECTOR_COUPLER" },
		@{ Path = "Vector/Source/Vector/Private/Combat/VectorGunComponent.cpp"; Text = "type=LIFT_TO_VECTOR" },
		@{ Path = "Vector/Source/Vector/Private/Combat/VectorLiftForkComponent.cpp"; Text = "LIFT_NATURAL_FALL" },
		@{ Path = "Vector/Source/Vector/Private/Combat/VectorLiftForkComponent.cpp"; Text = "AUTO_DENSE" },
		@{ Path = "Vector/Source/Vector/Private/Progression/VectorRunProgressionTypes.cpp"; Text = "LIFT-VECTOR COUPLER" },
		@{ Path = "Vector/Source/Vector/Private/Combat/VectorLiftForkComponent.cpp"; Text = "ALREADY_USED_THIS_AIRBORNE_CYCLE" },
		@{ Path = "Vector/Source/Vector/Private/Combat/VectorLiftForkMath.cpp"; Text = "ComputeDirectedSlam" },
		@{ Path = "Vector/Source/Vector/Private/Combat/VectorLiftForkComponent.cpp"; Text = "landingSource=DIRECTED_SLAM" },
        @{ Path = "Vector/Source/Vector/Private/Combat/VectorEnemy.cpp"; Text = "Enemy void recovery" },
		@{ Path = "Vector/Source/Vector/Private/Combat/VectorEnemy.cpp"; Text = "SM_Prototype_Bat" },
		@{ Path = "Vector/Source/Vector/Private/Combat/VectorEnemy.cpp"; Text = "SM_Prototype_Slime" },
		@{ Path = "Vector/Source/Vector/Private/Combat/VectorEnemy.cpp"; Text = "SM_Prototype_Skeleton" },
		@{ Path = "Vector/Source/Vector/Private/Combat/VectorEnemy.cpp"; Text = "Enemy navigation footprint:" },
		@{ Path = "Vector/Source/Vector/Private/Boss/VectorPhysicsBoss.cpp"; Text = "SM_Prototype_Dragon" },
		@{ Path = "Vector/Source/Vector/Private/Boss/VectorKineticOrb.cpp"; Text = "role=WEAK_HOMING" },
		@{ Path = "Vector/Source/Vector/Private/Boss/VectorKineticOrb.cpp"; Text = "role=PLAYER_AMMO" },
		@{ Path = "Tools/Art/import_prototype_monsters.py"; Text = "PASS:" },
        @{ Path = "Vector/Source/Vector/Private/VectorCharacter.cpp"; Text = "VectorGun->Fire" },
		@{ Path = "Vector/Source/Vector/Private/Combat/VectorGunComponent.cpp"; Text = "Vector gun hit:" },
		@{ Path = "Vector/Source/Vector/Private/Combat/VectorGunComponent.cpp"; Text = "Vector preview target changed:" },
		@{ Path = "Vector/Source/Vector/Private/Combat/VectorCombatTargeting.cpp"; Text = "FindBestCursorMovableStableTarget" },
		@{ Path = "Tools/Greybox/setup_aim_preview_test.py"; Text = "/Game/Prototype/L_AimPreviewTest" },
		@{ Path = "Vector/Source/Vector/Private/Combat/VectorTrajectoryPreviewComponent.cpp"; Text = "SweepSingleByObjectType" },
		@{ Path = "Vector/Source/Vector/Private/Combat/VectorTrajectoryPreviewComponent.cpp"; Text = "Trajectory preview:" },
		@{ Path = "Vector/Source/Vector/Private/Combat/VectorTrajectoryPreviewComponent.cpp"; Text = "Trajectory verification:" },
		@{ Path = "Vector/Source/Vector/Private/Combat/VectorTrajectoryPreviewComponent.cpp"; Text = "Trajectory verification gate:" },
		@{ Path = "Vector/Source/Vector/Private/Combat/VectorTrajectoryPreviewComponent.cpp"; Text = "reason=DYNAMIC_INTERFERENCE" },
		@{ Path = "Vector/Source/Vector/Private/Combat/VectorTrajectoryPreviewComponent.cpp"; Text = "DYNAMIC_TARGET" },
		@{ Path = "Vector/Source/Vector/Private/Gameplay/VectorCharacterMovementComponent.cpp"; Text = "ComputeDirectionalVelocityOverride" },
		@{ Path = "Vector/Source/Vector/Private/Gameplay/VectorCharacterMovementComponent.cpp"; Text = 'BroadcastWorldStaticImpact(Hit, TEXT("LANDING"))' },
		@{ Path = "Vector/Source/Vector/Private/Combat/VectorGunComponent.cpp"; Text = "DisarmGuidance" },
		@{ Path = "Vector/Source/Vector/Private/Combat/VectorGunComponent.cpp"; Text = "structureMultiplier=" },
		@{ Path = "Vector/Source/Vector/Private/Combat/VectorEnemyRangedAttackComponent.cpp"; Text = "tracking=CONTINUOUS" },
		@{ Path = "Vector/Source/Vector/Private/Combat/VectorEnemyRangedAttackComponent.cpp"; Text = "CorrosionVolley" },
		@{ Path = "Vector/Source/Vector/Private/Combat/VectorEnemy.cpp"; Text = "EVectorEnemyArchetype::ArcShell" },
		@{ Path = "Vector/Source/Vector/Private/Combat/VectorEnemy.cpp"; Text = "EVectorEnemyArchetype::CorrosionDrone" },
        @{ Path = "Vector/Source/Vector/Private/Combat/VectorGunComponent.cpp"; Text = "type=TWIN_VECTOR" },
        @{ Path = "Vector/Source/Vector/Private/Combat/VectorGunComponent.cpp"; Text = "type=LATERAL_CUTTER" },
        @{ Path = "Vector/Source/Vector/Private/Progression/VectorRunProgressionComponent.cpp"; Text = "Run calibration installed:" },
        @{ Path = "Vector/Source/Vector/Private/Progression/VectorRunProgressionComponent.cpp"; Text = "Run rule module installed:" },
        @{ Path = "Vector/Source/Vector/Private/Combat/VectorEnemyController.cpp"; Text = "Enemy path recovery:" },
        @{ Path = "Vector/Source/Vector/Private/Combat/VectorEnemyController.cpp"; Text = "Enemy crowd steering configured:" },
        @{ Path = "Vector/Source/Vector/Private/Combat/VectorEnemyController.cpp"; Text = "SetCrowdSeparation(true)" },
        @{ Path = "Vector/Source/Vector/Private/PCG/VectorPCGEncounterDirector.cpp"; Text = "PCG combat circuit:" },
        @{ Path = "Vector/Source/Vector/Private/PCG/VectorTacticalLayout.cpp"; Text = "module lacks a two-route combat circuit" },
        @{ Path = "Tools/Greybox/setup_tactical_pcg_preview.py"; Text = "UpperSeamPad" },
        @{ Path = "Vector/Source/Vector/Private/PCG/VectorPCGEncounterDirector.cpp"; Text = "nav projection failed" },
        @{ Path = "Vector/Source/Vector/Private/Combat/VectorEnemyController.cpp"; Text = "Enemy drop attack" },
        @{ Path = "Vector/Source/Vector/Private/Hunt/VectorEncounterComponent.cpp"; Text = "recovery=COUNTED" },
        @{ Path = "Vector/Source/Vector/Private/PCG/VectorPCGEncounterDirector.cpp"; Text = "PCG room exit verified" },
		@{ Path = "Vector/Source/Vector/Private/PCG/VectorPCGEncounterDirector.cpp"; Text = "PCG Boss wave built" },
		@{ Path = "Vector/Source/Vector/Private/PCG/VectorPCGEncounterDirector.cpp"; Text = "PCG Boss void contract:" },
		@{ Path = "Vector/Source/Vector/Private/PCG/VectorPCGRoomTrigger.cpp"; Text = "1550.0" },
		@{ Path = "Vector/Source/Vector/Private/VectorCharacter.cpp"; Text = "Player void recovery:" },
		@{ Path = "Vector/Source/Vector/Private/VectorCharacter.cpp"; Text = "encounterLedger=UNCHANGED" },
        @{ Path = "Vector/Source/Vector/Private/Combat/VectorImpactCollisionComponent.cpp"; Text = "EVectorKillCause::BossRam" },
        @{ Path = "Vector/Source/Vector/Private/Gameplay/VectorCharacterMovementComponent.cpp"; Text = "OnLandedWithImpact" },
        @{ Path = "Tools/Greybox/setup_physics_boss_test.py"; Text = "/Script/Vector.VectorPhysicsBoss" }
    )
    foreach ($Requirement in $RequiredWiring) {
        $Content = Get-Content -Raw -LiteralPath $Requirement.Path
        if (-not $Content.Contains($Requirement.Text)) {
            throw "Missing wiring '$($Requirement.Text)' in $($Requirement.Path)"
        }
    }

    Write-Output "OFFLINE PROTOTYPE VERIFICATION: PASS"
}
finally {
    Pop-Location
}
