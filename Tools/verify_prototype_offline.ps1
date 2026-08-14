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
    if ($RegistrationCount -ne 40) {
        throw "Expected 40 Automation registrations, found $RegistrationCount"
    }
    Write-Output "Automation registrations: $RegistrationCount"

    Write-Output "[3/4] Parsing Editor Python greybox scripts..."
    $GreyboxScripts = Get-ChildItem "Tools/Greybox" -Filter "*.py" -File
    foreach ($Script in $GreyboxScripts) {
        & python -B -c 'import ast, pathlib, sys; ast.parse(pathlib.Path(sys.argv[1]).read_bytes())' $Script.FullName
        if ($LASTEXITCODE -ne 0) {
            throw "Python syntax check failed: $($Script.FullName)"
        }
    }
    Write-Output "Greybox Python scripts: $($GreyboxScripts.Count) parsed"

    Write-Output "[4/4] Running deterministic physics simulations..."
    $Simulations = @(
        "Tools/collision_chain_1d_sim.py",
        "Tools/friction_slide_sim.py",
        "Tools/gravity_hook_sim.py",
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
        @{ Path = "Vector/Source/Vector/Private/VectorHUD.cpp"; Text = "EXIT LOCKED - CLEAR ALL" },
        @{ Path = "Tools/Greybox/setup_greybox_arena.py"; Text = "GA_ExtractionZone" },
        @{ Path = "Vector/Source/Vector/Private/PCG/VectorTacticalGenerationLibrary.cpp"; Text = "FVectorTacticalGenerator::Generate" },
        @{ Path = "Tools/Greybox/setup_tactical_pcg_preview.py"; Text = "generate_module_sequence" },
        @{ Path = "Tools/Greybox/setup_tactical_pcg_preview.py"; Text = "BossRing_BossSpawn" },
        @{ Path = "Tools/Greybox/setup_tactical_pcg_preview.py"; Text = "encounter_wave_one_spawns" },
        @{ Path = "Vector/Source/Vector/Private/PCG/VectorPCGEncounterDirector.cpp"; Text = "BeginDynamicEncounter" },
        @{ Path = "Vector/Source/Vector/Private/PCG/VectorPCGEncounterDirector.cpp"; Text = "SealDynamicEncounter" },
        @{ Path = "Vector/Source/Vector/Private/VectorHUD.cpp"; Text = "MAGNET-SHELL BEAST" },
        @{ Path = "Vector/Source/Vector/Private/VectorHUD.cpp"; Text = "WAVE %d / 3" },
        @{ Path = "Vector/Source/Vector/Private/Boss/VectorPhysicsBoss.cpp"; Text = "QueueDirectionalVelocityOverride" },
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
