// Automation tests for tableau 2: the division and the school.
//
// What they pin down are the promises made to the stage: the division is
// continuous and runs backwards, a family keeps its mother's volume, the
// school mills as a vortex when the room is in synchrony and breaks into
// readable families when it is not.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "Engine/World.h"
#include "Layout/VibH2OPositionDriver.h"
#include "Layout/VibH2OSchoolMath.h"
#include "Misc/ScopeExit.h"
#include "Osc/VibH2OOscTypes.h"
#include "UObject/StrongObjectPtr.h"
#include "VibH2OBubbleActor.h"
#include "VibH2OSettings.h"
#include "VibH2OStageActor.h"
#include "VibH2OSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

// ---------------------------------------------------------------------------
// The division, as pure arithmetic.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2ODivisionMathTest,
	"VibH2O.Tableau2.Division",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FVibH2ODivisionMathTest::RunTest(const FString& Parameters)
{
	// The ceiling.
	TestEqual(TEXT("176 sieges, 3 filles, plafond 1000 : familles de 4"), FVibH2OSchoolMath::ComputeFamilySize(3, 176, 1000), 4);
	TestEqual(TEXT("300 sieges : le plafond retire une fille"), FVibH2OSchoolMath::ComputeFamilySize(3, 300, 1000), 3);
	TestEqual(TEXT("Plafond depasse par les meres seules : aucune fille"), FVibH2OSchoolMath::ComputeFamilySize(3, 2000, 1000), 1);
	TestEqual(TEXT("Zero fille : pas de division"), FVibH2OSchoolMath::ComputeFamilySize(0, 50, 1000), 1);

	// The window.
	TestEqual(TEXT("Avant la fenetre : rien"), FVibH2OSchoolMath::ComputeDivisionProgress(0.0f, 0.0f, 0.35f), 0.0f);
	TestEqual(TEXT("Apres la fenetre : achevee"), FVibH2OSchoolMath::ComputeDivisionProgress(0.5f, 0.0f, 0.35f), 1.0f);
	TestEqual(TEXT("Sans filles, la migration EST le blend"), FVibH2OSchoolMath::ComputeMigrationBlend(0.3f, 0.0f, 0.35f, 1), 0.3f);
	TestEqual(TEXT("Les familles restent au siege pendant la premiere moitie de la division"),
		FVibH2OSchoolMath::ComputeMigrationBlend(0.17f, 0.0f, 0.35f, 4), 0.0f);
	TestEqual(TEXT("Au blend 1, les familles sont au banc"), FVibH2OSchoolMath::ComputeMigrationBlend(1.0f, 0.0f, 0.35f, 4), 1.0f);

	const float Spread = 35.0f;
	for (int32 FamilySize = 2; FamilySize <= 9; ++FamilySize)
	{
		// Start: the mother whole and in place, the daughters not yet born.
		for (int32 Member = 0; Member < FamilySize; ++Member)
		{
			const FVibH2ODivisionPose Pose = FVibH2OSchoolMath::EvaluateDivision(Member, FamilySize, 17, 0.0f, Spread);
			if (!Pose.Offset.IsNearlyZero(1e-4) || !FMath::IsNearlyEqual(Pose.ScaleFactor, Member == 0 ? 1.0f : 0.0f, 1e-5f))
			{
				AddError(FString::Printf(TEXT("Famille de %d, membre %d : a progression nulle, la mere doit etre entiere et seule."), FamilySize, Member));
				return false;
			}
		}

		// All along: the family's volume is the mother's, and nothing jumps.
		float MaxStep = 0.0f;
		TArray<FVibH2ODivisionPose> Previous;
		for (int32 Step = 0; Step <= 1000; ++Step)
		{
			const float P = Step / 1000.0f;
			float Volume = 0.0f;
			TArray<FVibH2ODivisionPose> Poses;
			for (int32 Member = 0; Member < FamilySize; ++Member)
			{
				const FVibH2ODivisionPose Pose = FVibH2OSchoolMath::EvaluateDivision(Member, FamilySize, 17, P, Spread);
				Volume += Pose.ScaleFactor * Pose.ScaleFactor * Pose.ScaleFactor;
				Poses.Add(Pose);
				if (Previous.Num() == FamilySize)
				{
					MaxStep = FMath::Max(MaxStep, static_cast<float>((Pose.Offset - Previous[Member].Offset).Size()));
					MaxStep = FMath::Max(MaxStep, FMath::Abs(Pose.ScaleFactor - Previous[Member].ScaleFactor) * Spread);
				}
			}
			if (!FMath::IsNearlyEqual(Volume, 1.0f, 1e-3f))
			{
				AddError(FString::Printf(TEXT("Famille de %d a la progression %.3f : volume %.4f au lieu de 1."), FamilySize, P, Volume));
				return false;
			}
			Previous = MoveTemp(Poses);
		}
		TestTrue(FString::Printf(TEXT("Famille de %d : aucun saut (pas maximal %.3f cm pour un millieme)"), FamilySize, MaxStep), MaxStep < 0.2f);

		// End: equal sisters, centred on the seat, none overlapping another.
		const float Equal = FMath::Pow(1.0f / FamilySize, 1.0f / 3.0f);
		FVector Centre = FVector::ZeroVector;
		float Closest = TNumericLimits<float>::Max();
		for (int32 Member = 0; Member < FamilySize; ++Member)
		{
			const FVibH2ODivisionPose A = FVibH2OSchoolMath::EvaluateDivision(Member, FamilySize, 17, 1.0f, Spread);
			TestTrue(TEXT("Soeurs de taille egale"), FMath::IsNearlyEqual(A.ScaleFactor, Equal, 1e-4f));
			Centre += A.Offset;
			for (int32 Other = Member + 1; Other < FamilySize; ++Other)
			{
				const FVibH2ODivisionPose B = FVibH2OSchoolMath::EvaluateDivision(Other, FamilySize, 17, 1.0f, Spread);
				Closest = FMath::Min(Closest, static_cast<float>((A.Offset - B.Offset).Size()));
			}
		}
		TestTrue(FString::Printf(TEXT("Famille de %d centree sur son siege"), FamilySize), (Centre / FamilySize).Size() < 0.01f);
		TestTrue(FString::Printf(TEXT("Famille de %d : soeurs distinctes (%.1f cm au plus pres)"), FamilySize, Closest), Closest > Spread * 0.5f);
	}

	// The order: two thresholds, a smooth fade, and a filter.
	TestEqual(TEXT("Sous le seuil bas : dispersion"), FVibH2OSchoolMath::ComputeOrderTarget(0.2f, 0.35f, 0.7f), 0.0f);
	TestEqual(TEXT("Au-dessus du seuil haut : vortex"), FVibH2OSchoolMath::ComputeOrderTarget(0.8f, 0.35f, 0.7f), 1.0f);
	TestTrue(TEXT("A mi-chemin : moitie"), FMath::IsNearlyEqual(FVibH2OSchoolMath::ComputeOrderTarget(0.525f, 0.35f, 0.7f), 0.5f, 1e-4f));
	float Order = 0.0f;
	for (int32 Frame = 0; Frame < 60 * 2; ++Frame)
	{
		Order = FVibH2OSchoolMath::SmoothOrder(Order, 1.0f, 1.0f / 60.0f, 2.0f);
	}
	TestTrue(FString::Printf(TEXT("Lissage de 2 s : 63 %% du chemin en 2 s (%.3f)"), Order), FMath::IsNearlyEqual(Order, 0.632f, 0.01f));

	return true;
}

// ---------------------------------------------------------------------------
// The school: a vortex in synchrony, readable families out of it.
// ---------------------------------------------------------------------------

namespace VibH2OSchoolTestUtils
{
	struct FSchoolMeasure
	{
		/** Mean share of each fish's horizontal velocity that turns round the axis, -1 to 1. */
		float Milling = 0.0f;
		/** Share of fish on the wall of the cone. */
		float OnCone = 0.0f;
		/** Mean distance from a sister to her family's centre, cm. */
		float FamilyRadius = 0.0f;
		/** Mean distance from a family's centre to the nearest other family's, cm. */
		float FamilyGap = 0.0f;
	};

	static FSchoolMeasure Measure(const UVibH2OFlockDriver& Flock, const FVibH2OSchoolFrame& School)
	{
		FSchoolMeasure Result;
		const int32 Count = Flock.GetAgentCount();
		if (Count == 0)
		{
			return Result;
		}

		// The centre of the school, not the origin: the axis sways.
		FVector Mean = FVector::ZeroVector;
		for (int32 Agent = 0; Agent < Count; ++Agent)
		{
			Mean += Flock.GetAgentPosition(Agent);
		}
		Mean /= Count;

		int32 OnCone = 0;
		for (int32 Agent = 0; Agent < Count; ++Agent)
		{
			const FVector P = Flock.GetAgentPosition(Agent);
			const FVector V = Flock.GetAgentVelocity(Agent);
			const FVector Rel(P.X - Mean.X, P.Y - Mean.Y, 0.0f);
			const FVector Horizontal(V.X, V.Y, 0.0f);
			if (Rel.SizeSquared() > 1.0 && Horizontal.SizeSquared() > 1.0)
			{
				const FVector Tangent = FVector(-Rel.Y, Rel.X, 0.0f).GetSafeNormal() * School.SpinDirection;
				Result.Milling += static_cast<float>(FVector::DotProduct(Tangent, Horizontal.GetSafeNormal()));
			}

			const float H01 = FMath::Clamp(static_cast<float>(P.Z - School.ConeZOffset) / School.ConeHeight, 0.0f, 1.0f);
			const float ConeR = FMath::Lerp(School.ConeBaseRadius, School.ConeTopRadius, H01);
			if (FMath::Abs(static_cast<float>(Rel.Size()) - ConeR) < ConeR * Flock.ShellThickness * 0.5f + 60.0f)
			{
				++OnCone;
			}
		}
		Result.Milling /= Count;
		Result.OnCone = static_cast<float>(OnCone) / Count;

		const int32 Size = School.FamilySize;
		TArray<FVector> Centres;
		for (int32 Family = 0; Family < School.FamilyCount; ++Family)
		{
			FVector C = FVector::ZeroVector;
			for (int32 Member = 0; Member < Size; ++Member)
			{
				C += Flock.GetAgentPosition(Family * Size + Member);
			}
			C /= Size;
			Centres.Add(C);
			for (int32 Member = 0; Member < Size; ++Member)
			{
				Result.FamilyRadius += static_cast<float>((Flock.GetAgentPosition(Family * Size + Member) - C).Size());
			}
		}
		Result.FamilyRadius /= Count;

		for (int32 Family = 0; Family < Centres.Num(); ++Family)
		{
			float Nearest = TNumericLimits<float>::Max();
			for (int32 Other = 0; Other < Centres.Num(); ++Other)
			{
				if (Other != Family)
				{
					Nearest = FMath::Min(Nearest, static_cast<float>((Centres[Family] - Centres[Other]).Size()));
				}
			}
			Result.FamilyGap += Nearest;
		}
		Result.FamilyGap /= FMath::Max(Centres.Num(), 1);
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2OSchoolVortexTest,
	"VibH2O.Tableau2.BancVortex",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FVibH2OSchoolVortexTest::RunTest(const FString& Parameters)
{
	using namespace VibH2OSchoolTestUtils;

	TStrongObjectPtr<UVibH2OFlockDriver> Flock(NewObject<UVibH2OFlockDriver>());

	// The real scale of the show: 176 spectators, four sisters each.
	FVibH2OSchoolFrame School;
	School.DeltaSeconds = 1.0f / 60.0f;
	School.FamilyCount = 176;
	School.FamilySize = 4;
	School.AverageExcitation = 0.3f;

	auto Run = [&](float Order, float Seconds)
	{
		School.Order = Order;
		const int32 Frames = FMath::RoundToInt(Seconds * 60.0f);
		const double Start = FPlatformTime::Seconds();
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			School.TimeSeconds += School.DeltaSeconds;
			Flock->StepSchool(School);
		}
		return static_cast<float>((FPlatformTime::Seconds() - Start) * 1000.0 / FMath::Max(Frames, 1));
	};

	// Out of synchrony first, as a show would begin.
	const float MsDispersed = Run(0.0f, 25.0f);
	const FSchoolMeasure Dispersed = Measure(*Flock, School);

	const float MsVortex = Run(1.0f, 25.0f);
	const FSchoolMeasure Vortex = Measure(*Flock, School);

	const float MsBack = Run(0.0f, 25.0f);
	const FSchoolMeasure Back = Measure(*Flock, School);

	AddInfo(FString::Printf(TEXT("Disperse : rotation %.2f, sur le cone %.0f %%, rayon des familles %.0f cm, ecart entre familles %.0f cm."),
		Dispersed.Milling, Dispersed.OnCone * 100.0f, Dispersed.FamilyRadius, Dispersed.FamilyGap));
	AddInfo(FString::Printf(TEXT("Vortex : rotation %.2f, sur le cone %.0f %%, rayon des familles %.0f cm."),
		Vortex.Milling, Vortex.OnCone * 100.0f, Vortex.FamilyRadius));
	AddInfo(FString::Printf(TEXT("Retour a la dispersion : rotation %.2f, rayon des familles %.0f cm, ecart %.0f cm."),
		Back.Milling, Back.FamilyRadius, Back.FamilyGap));
	AddInfo(FString::Printf(TEXT("Cout d'un pas de 704 bulles : %.3f ms (disperse), %.3f ms (vortex)."), MsDispersed, MsVortex));

	TestTrue(TEXT("En synchronie, le banc tourne d'un seul sens"), Vortex.Milling > 0.75f);
	TestTrue(TEXT("En synchronie, le banc epouse le cone"), Vortex.OnCone > 0.8f);
	TestTrue(TEXT("Hors synchronie, plus de rotation d'ensemble"), FMath::Abs(Dispersed.Milling) < 0.35f && FMath::Abs(Back.Milling) < 0.35f);
	TestTrue(TEXT("Hors synchronie, chaque famille reste groupee"), Dispersed.FamilyRadius < 110.0f && Back.FamilyRadius < 110.0f);
	TestTrue(TEXT("Hors synchronie, les familles restent distinctes les unes des autres"),
		Dispersed.FamilyGap > Dispersed.FamilyRadius * 2.0f && Back.FamilyGap > Back.FamilyRadius * 2.0f);
	TestTrue(TEXT("Un pas du banc tient dans le budget d'une image (< 4 ms)"), FMath::Max3(MsDispersed, MsVortex, MsBack) < 4.0f);

	return true;
}

// ---------------------------------------------------------------------------
// The whole stage: daughters born, divided, sent to the school, folded back.
// ---------------------------------------------------------------------------

#if WITH_EDITOR

namespace VibH2OSchoolTestUtils
{
	static FVibH2OOscMessage MakeInt(const FString& Address, int32 Value)
	{
		FVibH2OOscMessage Message(Address);
		Message.Args.Add(FVibH2OOscValue::MakeInt(Value));
		return Message;
	}

	static FVibH2OOscMessage MakeFloat(const FString& Address, float Value)
	{
		FVibH2OOscMessage Message(Address);
		Message.Args.Add(FVibH2OOscValue::MakeFloat(Value));
		return Message;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2ODivisionStageTest,
	"VibH2O.Integration.DivisionReversible",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibH2ODivisionStageTest::RunTest(const FString& Parameters)
{
	using namespace VibH2OSchoolTestUtils;

	UVibH2OSettings* Settings = GetMutableDefault<UVibH2OSettings>();
	const float SavedSettle = Settings->RoomMappingSettleSeconds;
	Settings->RoomMappingSettleSeconds = 0.0f;
	ON_SCOPE_EXIT
	{
		Settings->RoomMappingSettleSeconds = SavedSettle;
	};

	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false, TEXT("VibH2O_TestDivision"));
	if (World == nullptr)
	{
		AddError(TEXT("Impossible de creer un monde editeur de test."));
		return false;
	}
	ON_SCOPE_EXIT
	{
		World->DestroyWorld(false);
	};

	UVibH2OSubsystem* Subsystem = UVibH2OSubsystem::Get(World);
	if (!TestNotNull(TEXT("Doublure editeur"), Subsystem))
	{
		return false;
	}

	AVibH2OStageActor* Stage = World->SpawnActor<AVibH2OStageActor>();
	if (!TestNotNull(TEXT("Acteur de scene"), Stage))
	{
		return false;
	}
	// Everything that moves with time is switched off, so tableau 1 is a pure
	// function of the room and positions can be compared exactly.
	Stage->FloatAmplitude = 0.0f;
	Stage->FloatRotationAmount = 0.0f;
	Stage->DriftIntensity = 0.0f;
	Stage->OrganicDeform = 0.0f;
	Stage->ScalePerExcitation = 0.0f;
	Stage->BeatPopAmount = 0.0f;
	// The room's mood resizes every bubble, and with no arousal injected it
	// drifts to calm over a few seconds. Held at normal (size 1), so a size
	// read here belongs to the division alone.
	Stage->SetMoodOverride(EVibH2OMoodOverride::Normal);
	Stage->DaughtersPerBubble = 3;

	// A 4 x 3 room with one empty seat: eleven spectators.
	Subsystem->InjectOscMessage(MakeInt(TEXT("/RoomMapping/columns/"), 4));
	Subsystem->InjectOscMessage(MakeInt(TEXT("/RoomMapping/rows/"), 3));
	for (int32 SeatIndex = 1; SeatIndex <= 12; ++SeatIndex)
	{
		const int32 Id = SeatIndex == 6 ? 0 : 300 + SeatIndex;
		Subsystem->InjectOscMessage(MakeInt(FString::Printf(TEXT("/RoomMapping/%d/"), SeatIndex), Id));
		if (Id != 0)
		{
			Subsystem->InjectOscMessage(MakeFloat(FString::Printf(TEXT("/BPM/%d/"), Id), 60.0f + SeatIndex * 3.0f));
			Subsystem->InjectOscMessage(MakeFloat(FString::Printf(TEXT("/Synchronie/%d/"), Id), 0.9f));
		}
	}

	const float Dt = 1.0f / 60.0f;
	Stage->BlendAlpha = 0.0f;
	Stage->Tick(Dt);
	Stage->Tick(Dt);

	const TArray<AVibH2OBubbleActor*> Mothers = Stage->GetBubbles();
	TestEqual(TEXT("Onze bulles meres"), Mothers.Num(), 11);
	TestEqual(TEXT("Trois filles chacune"), Stage->GetDaughters().Num(), 33);
	TestEqual(TEXT("Familles de quatre"), Stage->GetFamilySize(), 4);

	TMap<const AVibH2OBubbleActor*, FVector> SeatPositions;
	for (const AVibH2OBubbleActor* Mother : Mothers)
	{
		SeatPositions.Add(Mother, Mother->GetRootComponent()->GetRelativeLocation());
	}
	bool bAllHidden = true;
	for (const AVibH2OBubbleActor* Daughter : Stage->GetDaughters())
	{
		bAllHidden &= Daughter->IsHidden();
	}
	TestTrue(TEXT("Au tableau 1, les filles sont cachees"), bAllHidden);

	// Mid-division: every family holds exactly its mother's volume.
	Stage->BlendAlpha = 0.12f;
	Stage->Tick(Dt);
	float WorstVolume = 0.0f;
	bool bBeatShared = true;
	for (const AVibH2OBubbleActor* Mother : Mothers)
	{
		float Volume = FMath::Pow(static_cast<float>(Mother->GetRootComponent()->GetRelativeScale3D().X), 3.0f);
		for (const AVibH2OBubbleActor* Daughter : Stage->GetDaughtersOfSeat(Mother->SeatIndex))
		{
			Volume += FMath::Pow(static_cast<float>(Daughter->GetRootComponent()->GetRelativeScale3D().X), 3.0f);
			bBeatShared &= !Daughter->IsHidden()
				&& Daughter->IndividualId == Mother->IndividualId
				&& Daughter->State.BeatPhase == Mother->State.BeatPhase;
		}
		WorstVolume = FMath::Max(WorstVolume, FMath::Abs(Volume - FMath::Pow(Stage->BaseScale, 3.0f)));
	}
	TestTrue(FString::Printf(TEXT("Mi-division : volume de chaque famille conserve (ecart %.5f)"), WorstVolume), WorstVolume < 1e-3f);
	TestTrue(TEXT("Les filles sont visibles et battent avec leur mere"), bBeatShared);

	// The full morph and back, one small step per frame: nothing may jump.
	TMap<const AVibH2OBubbleActor*, FVector> Last;
	auto Bodies = [Stage]()
	{
		TArray<AVibH2OBubbleActor*> All = Stage->GetBubbles();
		All.Append(Stage->GetDaughters());
		return All;
	};
	float WorstJump = 0.0f;
	auto Step = [&](float Blend)
	{
		Stage->BlendAlpha = Blend;
		Stage->Tick(Dt);
		for (const AVibH2OBubbleActor* Body : Bodies())
		{
			const FVector Now = Body->GetRootComponent()->GetRelativeLocation();
			if (const FVector* Before = Last.Find(Body); Before != nullptr && !Body->IsHidden())
			{
				WorstJump = FMath::Max(WorstJump, static_cast<float>((Now - *Before).Size()));
			}
			Last.Add(Body, Now);
		}
	};
	for (int32 Frame = 0; Frame <= 300; ++Frame)
	{
		Step(0.12f + 0.88f * Frame / 300.0f);
	}
	for (int32 Frame = 0; Frame < 120; ++Frame)
	{
		Step(1.0f);
	}
	for (int32 Frame = 0; Frame <= 300; ++Frame)
	{
		Step(1.0f - Frame / 300.0f);
	}
	TestTrue(FString::Printf(TEXT("Aller-retour sans a-coup (plus grand pas %.1f cm par image)"), WorstJump), WorstJump < 40.0f);

	// Back at tableau 1: every mother on her seat, whole, her daughters gone.
	Stage->Tick(Dt);
	float WorstReturn = 0.0f;
	for (const AVibH2OBubbleActor* Mother : Mothers)
	{
		WorstReturn = FMath::Max(WorstReturn,
			static_cast<float>((Mother->GetRootComponent()->GetRelativeLocation() - SeatPositions[Mother]).Size()));
		TestTrue(TEXT("La mere retrouve sa taille"),
			FMath::IsNearlyEqual(static_cast<float>(Mother->GetRootComponent()->GetRelativeScale3D().X), Stage->BaseScale, 1e-4f));
	}
	TestTrue(FString::Printf(TEXT("Retour exact au plan de salle (ecart %.4f cm)"), WorstReturn), WorstReturn < 0.01f);
	bAllHidden = true;
	for (const AVibH2OBubbleActor* Daughter : Stage->GetDaughters())
	{
		bAllHidden &= Daughter->IsHidden();
	}
	TestTrue(TEXT("Les filles sont rentrees dans leur mere"), bAllHidden);

	// Division off: no daughter at all.
	Stage->DaughtersPerBubble = 0;
	Stage->Tick(Dt);
	TestEqual(TEXT("Zero fille par bulle : plus aucune fille"), Stage->GetDaughters().Num(), 0);

	return true;
}

#endif // WITH_EDITOR

#endif // WITH_DEV_AUTOMATION_TESTS
