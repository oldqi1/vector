// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCG/VectorTacticalLayout.h"

namespace VectorTacticalLayoutInternal
{
	const EVectorPhysicsOpportunity RequiredSourceMask =
		EVectorPhysicsOpportunity::LongLaunchLane
		| EVectorPhysicsOpportunity::ChargeBaitLane
		| EVectorPhysicsOpportunity::HeightDrop;
	const EVectorPhysicsOpportunity RequiredReceiverMask =
		EVectorPhysicsOpportunity::HardWallReceiver
		| EVectorPhysicsOpportunity::CrowdReceiver
		| EVectorPhysicsOpportunity::HeightDrop;

	void ConfigureCircuit(FVectorTacticalModuleDefinition& Module)
	{
		switch (Module.Type)
		{
		case EVectorTacticalModuleType::OpenBowl:
			Module.Sources = { TEXT("ChargerLane"), TEXT("VectorInjection") };
			Module.Converters = { TEXT("KineticRail"), TEXT("CablePost") };
			Module.Receivers = { TEXT("CrowdWell"), TEXT("SplitWall") };
			Module.RecoveryPockets = { TEXT("SouthPocket") };
			Module.SupportedToolVerbs = { TEXT("BaitCharge"), TEXT("CableRoute"), TEXT("VectorInject") };
			Module.EnemyFunctionalSlots = { TEXT("Source"), TEXT("Ammunition"), TEXT("Receiver") };
			Module.Recipes = {
				TEXT("BaitCharge>KineticRail>CrowdWell"),
				TEXT("CableRoute>ReceiverHold>SplitWall") };
			Module.RewardBias = TEXT("Impulse");
			break;
		case EVectorTacticalModuleType::HardLane:
			Module.Sources = { TEXT("ChargerLane"), TEXT("VectorInjection") };
			Module.Converters = { TEXT("AnchorPost"), TEXT("UpperRamp") };
			Module.Receivers = { TEXT("FractureWall"), TEXT("DropReceiver") };
			Module.RecoveryPockets = { TEXT("SideAlcove") };
			Module.SupportedToolVerbs = { TEXT("BaitCharge"), TEXT("AnchorTear"), TEXT("LiftConvert") };
			Module.EnemyFunctionalSlots = { TEXT("Source"), TEXT("Receiver"), TEXT("Ammunition") };
			Module.Recipes = {
				TEXT("BaitCharge>AnchorPost>FractureWall"),
				TEXT("AnchorTear>UpperRamp>DropReceiver") };
			Module.RewardBias = TEXT("Recharge");
			break;
		case EVectorTacticalModuleType::HeightShelf:
			Module.Sources = { TEXT("ChargerLane"), TEXT("VectorInjection") };
			Module.Converters = { TEXT("EnvironmentRedirector"), TEXT("LiftFork") };
			Module.Receivers = { TEXT("UpperImpactDeck"), TEXT("LowerCrowd") };
			Module.RecoveryPockets = { TEXT("UpperLanding") };
			Module.SupportedToolVerbs = { TEXT("LiftConvert"), TEXT("CableRoute"), TEXT("VectorInject") };
			Module.EnemyFunctionalSlots = { TEXT("Receiver"), TEXT("Source"), TEXT("Ammunition") };
			Module.Recipes = {
				TEXT("BaitCharge>EnvironmentRedirector>UpperImpactDeck"),
				TEXT("VectorInject>LiftFork>DirectedSlam>LowerCrowd") };
			Module.RewardBias = TEXT("Range");
			break;
		case EVectorTacticalModuleType::SlickCross:
			Module.Sources = { TEXT("ChargerLane"), TEXT("VectorInjection") };
			Module.Converters = { TEXT("LowFrictionCross"), TEXT("CablePost") };
			Module.Receivers = { TEXT("CrossTraffic"), TEXT("HeavyReceiver") };
			Module.RecoveryPockets = { TEXT("DryIsland") };
			Module.SupportedToolVerbs = { TEXT("ModifyFriction"), TEXT("CableRoute"), TEXT("BaitCharge") };
			Module.EnemyFunctionalSlots = { TEXT("Source"), TEXT("Ammunition"), TEXT("Receiver") };
			Module.Recipes = {
				TEXT("ModifyFriction>CrossTraffic>HeavyReceiver"),
				TEXT("BaitCharge>CableRoute>CrossTraffic") };
			Module.RewardBias = TEXT("Capacity");
			break;
		case EVectorTacticalModuleType::BossRing:
			Module.Sources = { TEXT("BossCharge"), TEXT("KineticOrb") };
			Module.Converters = { TEXT("AnchorModule"), TEXT("HeightModule") };
			Module.Receivers = { TEXT("BossAnchor"), TEXT("ExposedCore") };
			Module.RecoveryPockets = { TEXT("OuterRing") };
			Module.SupportedToolVerbs = { TEXT("AnchorTear"), TEXT("LiftConvert"), TEXT("VectorInject") };
			Module.EnemyFunctionalSlots = { TEXT("Source"), TEXT("Ammunition"), TEXT("Receiver") };
			Module.Recipes = {
				TEXT("BossCharge>AnchorModule>BossAnchor"),
				TEXT("KineticOrb>HeightModule>ExposedCore") };
			Module.RewardBias = TEXT("CoreSample");
			break;
		default:
			Module.RecoveryPockets = { TEXT("SafeFloor") };
			break;
		}
	}

	FVectorTacticalModuleDefinition MakeModule(
		const TCHAR* Id,
		const EVectorTacticalModuleType Type,
		const EVectorPhysicsOpportunity Opportunities,
		const int32 EnemyBudget,
		const int32 HeightLayerCount = 1,
		const double MaximumHeightDifferenceCm = 0.0)
	{
		FVectorTacticalModuleDefinition Module;
		Module.ModuleId = FName(Id);
		Module.Type = Type;
		Module.Opportunities = Opportunities;
		Module.EnemyBudget = EnemyBudget;
		Module.HeightLayerCount = FMath::Max(1, HeightLayerCount);
		Module.MaximumHeightDifferenceCm = FMath::Max(0.0, MaximumHeightDifferenceCm);
		ConfigureCircuit(Module);
		return Module;
	}

	FVectorTacticalModuleDefinition MakeSafeStart()
	{
		return MakeModule(TEXT("SafeStart"), EVectorTacticalModuleType::SafeStart,
			EVectorPhysicsOpportunity::RecoveryPocket, 0);
	}

	FVectorTacticalModuleDefinition MakeBossRing()
	{
		return MakeModule(TEXT("BossRing"), EVectorTacticalModuleType::BossRing,
			EVectorPhysicsOpportunity::LongLaunchLane
			| EVectorPhysicsOpportunity::HardWallReceiver
			| EVectorPhysicsOpportunity::CrowdReceiver
			| EVectorPhysicsOpportunity::ChargeBaitLane
			| EVectorPhysicsOpportunity::TetherSwingArc
			| EVectorPhysicsOpportunity::HeightDrop
			| EVectorPhysicsOpportunity::RecoveryPocket,
			0, 3, 650.0);
	}

	FVectorTacticalModuleDefinition MakeExtraction()
	{
		return MakeModule(TEXT("Extraction"), EVectorTacticalModuleType::Extraction,
			EVectorPhysicsOpportunity::RecoveryPocket, 0);
	}

	bool IsEncounterType(const EVectorTacticalModuleType Type)
	{
		return Type == EVectorTacticalModuleType::OpenBowl
			|| Type == EVectorTacticalModuleType::HardLane
			|| Type == EVectorTacticalModuleType::HeightShelf
			|| Type == EVectorTacticalModuleType::SlickCross;
	}

	FString GetTypeLabel(const EVectorTacticalModuleType Type)
	{
		switch (Type)
		{
		case EVectorTacticalModuleType::SafeStart: return TEXT("SafeStart");
		case EVectorTacticalModuleType::OpenBowl: return TEXT("OpenBowl");
		case EVectorTacticalModuleType::HardLane: return TEXT("HardLane");
		case EVectorTacticalModuleType::HeightShelf: return TEXT("HeightShelf");
		case EVectorTacticalModuleType::SlickCross: return TEXT("SlickCross");
		case EVectorTacticalModuleType::BossRing: return TEXT("BossRing");
		case EVectorTacticalModuleType::Extraction: return TEXT("Extraction");
		default: return TEXT("Unknown");
		}
	}

	FVectorTacticalLayout BuildCandidate(
		const int32 RequestedSeed,
		const int32 ResolvedSeed,
		const int32 AttemptNumber,
		const FVectorTacticalGenerationRules& Rules)
	{
		FVectorTacticalLayout Layout;
		Layout.RequestedSeed = RequestedSeed;
		Layout.ResolvedSeed = ResolvedSeed;
		Layout.GenerationAttempts = AttemptNumber;
		Layout.Modules.Add(MakeSafeStart());

		const TArray<FVectorTacticalModuleDefinition>& Catalog =
			FVectorTacticalGenerator::GetEncounterModuleCatalog();
		TArray<int32> RemainingIndices;
		for (int32 Index = 0; Index < Catalog.Num(); ++Index)
		{
			RemainingIndices.Add(Index);
		}

		FRandomStream Random(ResolvedSeed);
		const int32 Count = FMath::Clamp(Rules.EncounterModuleCount, 0, Catalog.Num());
		for (int32 Slot = 0; Slot < Count; ++Slot)
		{
			const int32 Pick = Random.RandRange(0, RemainingIndices.Num() - 1);
			Layout.Modules.Add(Catalog[RemainingIndices[Pick]]);
			RemainingIndices.RemoveAt(Pick);
		}

		Layout.Modules.Add(MakeBossRing());
		Layout.Modules.Add(MakeExtraction());
		Layout.TacticalScore = FVectorTacticalGenerator::ComputeTacticalScore(Layout);
		Layout.bValid = FVectorTacticalGenerator::Validate(Layout, Rules, Layout.FailureReason);
		return Layout;
	}

	FVectorTacticalLayout BuildFallback(
		const int32 Seed,
		const FVectorTacticalGenerationRules& Rules)
	{
		FVectorTacticalLayout Layout;
		Layout.RequestedSeed = Seed;
		Layout.ResolvedSeed = Seed;
		Layout.GenerationAttempts = 0;
		Layout.bUsedFallback = true;
		Layout.Modules.Add(MakeSafeStart());
		const TArray<FVectorTacticalModuleDefinition>& Catalog =
			FVectorTacticalGenerator::GetEncounterModuleCatalog();
		Layout.Modules.Add(Catalog[0]); // OpenBowl: tether/crowd/launch.
		Layout.Modules.Add(Catalog[2]); // HeightShelf: height/wall/crowd.
		Layout.Modules.Add(MakeBossRing());
		Layout.Modules.Add(MakeExtraction());
		Layout.TacticalScore = FVectorTacticalGenerator::ComputeTacticalScore(Layout);
		Layout.bValid = FVectorTacticalGenerator::Validate(Layout, Rules, Layout.FailureReason);
		return Layout;
	}
}

int32 FVectorTacticalModuleDefinition::CountOpportunities() const
{
	int32 Count = 0;
	for (uint16 Bit = 1; Bit <= static_cast<uint16>(EVectorPhysicsOpportunity::RecoveryPocket); Bit <<= 1)
	{
		if ((static_cast<uint16>(Opportunities) & Bit) != 0)
		{
			++Count;
		}
	}
	return Count;
}

bool FVectorTacticalModuleDefinition::HasOpportunity(
	const EVectorPhysicsOpportunity Opportunity) const
{
	return EnumHasAnyFlags(Opportunities, Opportunity);
}

bool FVectorTacticalModuleDefinition::HasCompleteCircuit() const
{
	return !Sources.IsEmpty() && !Converters.IsEmpty() && !Receivers.IsEmpty()
		&& !RecoveryPockets.IsEmpty();
}

bool FVectorTacticalModuleDefinition::HasDistinctOpenings() const
{
	return SupportedToolVerbs.Num() >= 2
		&& SupportedToolVerbs[0] != SupportedToolVerbs[1];
}

FString FVectorTacticalModuleDefinition::DescribeCircuit() const
{
	return FString::Printf(
		TEXT("source=%s converter=%s receiver=%s recovery=%s openings=%d recipes=%d reward=%s"),
		Sources.IsEmpty() ? TEXT("NONE") : *Sources[0].ToString(),
		Converters.IsEmpty() ? TEXT("NONE") : *Converters[0].ToString(),
		Receivers.IsEmpty() ? TEXT("NONE") : *Receivers[0].ToString(),
		RecoveryPockets.IsEmpty() ? TEXT("NONE") : *RecoveryPockets[0].ToString(),
		SupportedToolVerbs.Num(), Recipes.Num(), *RewardBias.ToString());
}

bool FVectorTacticalLayout::HasOpportunity(const EVectorPhysicsOpportunity Opportunity) const
{
	for (const FVectorTacticalModuleDefinition& Module : Modules)
	{
		if (Module.HasOpportunity(Opportunity))
		{
			return true;
		}
	}
	return false;
}

int32 FVectorTacticalLayout::GetMaximumHeightLayerCount() const
{
	int32 MaximumLayers = 1;
	for (const FVectorTacticalModuleDefinition& Module : Modules)
	{
		MaximumLayers = FMath::Max(MaximumLayers, Module.HeightLayerCount);
	}
	return MaximumLayers;
}

double FVectorTacticalLayout::GetMaximumHeightDifferenceCm() const
{
	double MaximumDifference = 0.0;
	for (const FVectorTacticalModuleDefinition& Module : Modules)
	{
		MaximumDifference = FMath::Max(
			MaximumDifference, Module.MaximumHeightDifferenceCm);
	}
	return MaximumDifference;
}

FString FVectorTacticalLayout::DescribeModuleSequence() const
{
	FString ModuleSequence;
	for (int32 Index = 0; Index < Modules.Num(); ++Index)
	{
		if (Index > 0)
		{
			ModuleSequence += TEXT(">");
		}
		ModuleSequence += VectorTacticalLayoutInternal::GetTypeLabel(Modules[Index].Type);
	}
	return ModuleSequence;
}

FString FVectorTacticalLayout::Describe() const
{
	const FString ModuleSequence = DescribeModuleSequence();
	return FString::Printf(
		TEXT("seed=%d resolved=%d attempts=%d fallback=%s valid=%s score=%.1f verticalLayers=%d heightDelta=%.0f modules=%s"),
		RequestedSeed, ResolvedSeed, GenerationAttempts,
		bUsedFallback ? TEXT("YES") : TEXT("no"),
		bValid ? TEXT("YES") : TEXT("no"), TacticalScore,
		GetMaximumHeightLayerCount(), GetMaximumHeightDifferenceCm(), *ModuleSequence);
}

FVectorTacticalLayout FVectorTacticalGenerator::Generate(
	const int32 Seed,
	const FVectorTacticalGenerationRules& Rules)
{
	const int32 Attempts = FMath::Max(0, Rules.MaximumGenerationAttempts);
	for (int32 Attempt = 0; Attempt < Attempts; ++Attempt)
	{
		const int32 ResolvedSeed = Seed + Attempt * 104729;
		FVectorTacticalLayout Layout = VectorTacticalLayoutInternal::BuildCandidate(
			Seed, ResolvedSeed, Attempt + 1, Rules);
		if (Layout.bValid)
		{
			return Layout;
		}
	}
	return VectorTacticalLayoutInternal::BuildFallback(Seed, Rules);
}

bool FVectorTacticalGenerator::Validate(
	const FVectorTacticalLayout& Layout,
	const FVectorTacticalGenerationRules& Rules,
	FString& OutFailureReason)
{
	OutFailureReason.Reset();
	if (Layout.Modules.Num() < 4
		|| Layout.Modules[0].Type != EVectorTacticalModuleType::SafeStart
		|| Layout.Modules.Last().Type != EVectorTacticalModuleType::Extraction)
	{
		OutFailureReason = TEXT("layout endpoints invalid");
		return false;
	}

	int32 EncounterCount = 0;
	int32 BossCount = 0;
	EVectorPhysicsOpportunity EncounterOpportunities = EVectorPhysicsOpportunity::None;
	EVectorPhysicsOpportunity PreviousOpportunities = EVectorPhysicsOpportunity::None;
	bool bHasPreviousEncounter = false;
	for (const FVectorTacticalModuleDefinition& Module : Layout.Modules)
	{
		if (Module.Type == EVectorTacticalModuleType::BossRing)
		{
			++BossCount;
			continue;
		}
		if (!VectorTacticalLayoutInternal::IsEncounterType(Module.Type))
		{
			continue;
		}

		++EncounterCount;
		if (Module.EnemyBudget < Rules.MinimumEnemyBudget
			|| Module.EnemyBudget > Rules.MaximumEnemyBudget)
		{
			OutFailureReason = FString::Printf(TEXT("enemy budget invalid: %s=%d"),
				*Module.ModuleId.ToString(), Module.EnemyBudget);
			return false;
		}
		if (!Module.HasOpportunity(EVectorPhysicsOpportunity::RecoveryPocket)
			|| Module.CountOpportunities() < 3)
		{
			OutFailureReason = FString::Printf(TEXT("module lacks tactical affordances: %s"),
				*Module.ModuleId.ToString());
			return false;
		}
		if (!Module.HasCompleteCircuit() || !Module.HasDistinctOpenings()
			|| Module.Recipes.Num() < 2)
		{
			OutFailureReason = FString::Printf(
				TEXT("module lacks a two-route combat circuit: %s %s"),
				*Module.ModuleId.ToString(), *Module.DescribeCircuit());
			return false;
		}
		if (!Module.EnemyFunctionalSlots.Contains(FName(TEXT("Source")))
			|| !Module.EnemyFunctionalSlots.Contains(FName(TEXT("Receiver"))))
		{
			OutFailureReason = FString::Printf(
				TEXT("enemy ecology does not close circuit: %s"),
				*Module.ModuleId.ToString());
			return false;
		}
		if (Module.HasOpportunity(EVectorPhysicsOpportunity::HeightDrop)
			&& (Module.HeightLayerCount < 2 || Module.MaximumHeightDifferenceCm < 150.0))
		{
			OutFailureReason = FString::Printf(
				TEXT("HeightDrop module lacks real vertical geometry: %s layers=%d delta=%.0f"),
				*Module.ModuleId.ToString(), Module.HeightLayerCount,
				Module.MaximumHeightDifferenceCm);
			return false;
		}
		if (bHasPreviousEncounter && Module.Opportunities == PreviousOpportunities)
		{
			OutFailureReason = TEXT("consecutive encounter opportunity sets repeat");
			return false;
		}
		bHasPreviousEncounter = true;
		PreviousOpportunities = Module.Opportunities;
		EncounterOpportunities |= Module.Opportunities;
	}

	if (EncounterCount != Rules.EncounterModuleCount || BossCount != 1)
	{
		OutFailureReason = FString::Printf(TEXT("module count invalid: encounters=%d boss=%d"),
			EncounterCount, BossCount);
		return false;
	}
	if (!EnumHasAnyFlags(EncounterOpportunities, EVectorPhysicsOpportunity::TetherSwingArc)
		|| !EnumHasAnyFlags(EncounterOpportunities, EVectorPhysicsOpportunity::HeightDrop)
		|| !EnumHasAnyFlags(EncounterOpportunities,
			EVectorPhysicsOpportunity::HardWallReceiver | EVectorPhysicsOpportunity::LowFrictionPath))
	{
		OutFailureReason = TEXT("global physical opportunity requirement missing");
		return false;
	}
	if (Layout.GetMaximumHeightLayerCount() < 2
		|| Layout.GetMaximumHeightDifferenceCm() < 150.0)
	{
		OutFailureReason = TEXT("route can collapse to one height layer");
		return false;
	}
	if (Layout.TacticalScore < Rules.MinimumTacticalScore)
	{
		OutFailureReason = FString::Printf(TEXT("tactical score too low: %.1f"), Layout.TacticalScore);
		return false;
	}
	return true;
}

double FVectorTacticalGenerator::ComputeTacticalScore(const FVectorTacticalLayout& Layout)
{
	EVectorPhysicsOpportunity UniqueOpportunities = EVectorPhysicsOpportunity::None;
	int32 ChainRecipeCount = 0;
	int32 RecoveryCount = 0;
	int32 ComplementaryEnemyGroupCount = 0;
	for (const FVectorTacticalModuleDefinition& Module : Layout.Modules)
	{
		if (!VectorTacticalLayoutInternal::IsEncounterType(Module.Type))
		{
			continue;
		}
		UniqueOpportunities |= Module.Opportunities;
		if (Module.HasOpportunity(EVectorPhysicsOpportunity::RecoveryPocket))
		{
			++RecoveryCount;
		}
		if (Module.EnemyBudget >= 8)
		{
			++ComplementaryEnemyGroupCount;
		}
		if (EnumHasAnyFlags(Module.Opportunities, VectorTacticalLayoutInternal::RequiredSourceMask)
			&& EnumHasAnyFlags(Module.Opportunities, VectorTacticalLayoutInternal::RequiredReceiverMask))
		{
			++ChainRecipeCount;
		}
	}

	FVectorTacticalModuleDefinition UniqueCounter;
	UniqueCounter.Opportunities = UniqueOpportunities;
	return 2.0 * ChainRecipeCount
		+ 1.5 * UniqueCounter.CountOpportunities()
		+ 1.0 * RecoveryCount
		+ 1.0 * ComplementaryEnemyGroupCount;
}

const TArray<FVectorTacticalModuleDefinition>& FVectorTacticalGenerator::GetEncounterModuleCatalog()
{
	using namespace VectorTacticalLayoutInternal;
	static const TArray<FVectorTacticalModuleDefinition> Catalog =
	{
		MakeModule(TEXT("OpenBowl"), EVectorTacticalModuleType::OpenBowl,
			EVectorPhysicsOpportunity::LongLaunchLane
			| EVectorPhysicsOpportunity::CrowdReceiver
			| EVectorPhysicsOpportunity::TetherSwingArc
			| EVectorPhysicsOpportunity::RecoveryPocket, 10),
		MakeModule(TEXT("HardLane"), EVectorTacticalModuleType::HardLane,
			EVectorPhysicsOpportunity::LongLaunchLane
			| EVectorPhysicsOpportunity::HardWallReceiver
			| EVectorPhysicsOpportunity::ChargeBaitLane
			| EVectorPhysicsOpportunity::HeightDrop
			| EVectorPhysicsOpportunity::RecoveryPocket, 9, 2, 420.0),
		MakeModule(TEXT("HeightShelf"), EVectorTacticalModuleType::HeightShelf,
			EVectorPhysicsOpportunity::HardWallReceiver
			| EVectorPhysicsOpportunity::CrowdReceiver
			| EVectorPhysicsOpportunity::HeightDrop
			| EVectorPhysicsOpportunity::RecoveryPocket, 8, 3, 650.0),
		MakeModule(TEXT("SlickCross"), EVectorTacticalModuleType::SlickCross,
			EVectorPhysicsOpportunity::CrowdReceiver
			| EVectorPhysicsOpportunity::TetherSwingArc
			| EVectorPhysicsOpportunity::LowFrictionPath
			| EVectorPhysicsOpportunity::RecoveryPocket, 9),
	};
	return Catalog;
}
