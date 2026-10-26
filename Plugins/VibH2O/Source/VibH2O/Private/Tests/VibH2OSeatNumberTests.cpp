// Automation test for the seat numbers shown to the audience.
//
// What it pins down: one number per spectator, the right number, written over
// the middle of the bubble as the viewer sees it - in front of the opaque
// surface, facing the viewer, sized to the bubble.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"
#include "Osc/VibH2OOscTypes.h"
#include "VibH2OBubbleActor.h"
#include "VibH2OSettings.h"
#include "VibH2OStageActor.h"
#include "VibH2OSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

#if WITH_EDITOR

namespace VibH2OSeatNumberTestUtils
{
	static FVibH2OOscMessage MakeInt(const FString& Address, int32 Value)
	{
		FVibH2OOscMessage Message(Address);
		Message.Args.Add(FVibH2OOscValue::MakeInt(Value));
		return Message;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2OSeatNumberTest,
	"VibH2O.Integration.NumerosDeSiege",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibH2OSeatNumberTest::RunTest(const FString& Parameters)
{
	using namespace VibH2OSeatNumberTestUtils;

	UVibH2OSettings* Settings = GetMutableDefault<UVibH2OSettings>();
	const float SavedSettle = Settings->RoomMappingSettleSeconds;
	Settings->RoomMappingSettleSeconds = 0.0f;
	ON_SCOPE_EXIT
	{
		Settings->RoomMappingSettleSeconds = SavedSettle;
	};

	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false, TEXT("VibH2O_TestNumeros"));
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
	// The show's scale: the label must be sized in world centimetres, not
	// scaled a second time by the stage.
	Stage->SetActorScale3D(FVector(8.0f));
	Stage->DaughtersPerBubble = 2;

	// A 3 x 2 room with one empty seat: five spectators.
	Subsystem->InjectOscMessage(MakeInt(TEXT("/RoomMapping/columns/"), 3));
	Subsystem->InjectOscMessage(MakeInt(TEXT("/RoomMapping/rows/"), 2));
	for (int32 SeatIndex = 1; SeatIndex <= 6; ++SeatIndex)
	{
		Subsystem->InjectOscMessage(MakeInt(FString::Printf(TEXT("/RoomMapping/%d/"), SeatIndex), SeatIndex == 4 ? 0 : 40 + SeatIndex));
	}

	const float Dt = 1.0f / 60.0f;
	Stage->bShowSeatNumbers = true;
	Stage->Tick(Dt);
	Stage->Tick(Dt);

	const TArray<AVibH2OBubbleActor*> Mothers = Stage->GetBubbles();
	if (!TestEqual(TEXT("Cinq bulles meres"), Mothers.Num(), 5))
	{
		return false;
	}

	for (const AVibH2OBubbleActor* Mother : Mothers)
	{
		TestTrue(TEXT("Chaque spectateur a son numero"), Mother->GetSeatLabel()->IsVisible());
		TestEqual(TEXT("Le numero est l'identifiant du spectateur"),
			Mother->GetSeatLabel()->Text.ToString(), FString::FromInt(Mother->IndividualId));
	}
	for (const AVibH2OBubbleActor* Daughter : Stage->GetDaughters())
	{
		TestFalse(TEXT("Un seul numero par spectateur : aucun sur les filles"), Daughter->GetSeatLabel()->IsVisible());
	}

	Stage->SeatNumberSource = EVibH2OSeatNumber::Seat;
	Stage->Tick(Dt);
	const AVibH2OBubbleActor* const* AfterGap = Mothers.FindByPredicate([](const AVibH2OBubbleActor* Mother)
	{
		return Mother->IndividualId == 45;
	});
	if (TestNotNull(TEXT("Le spectateur 45"), AfterGap))
	{
		TestEqual(TEXT("Numero de siege dans le plan, siege vide compte"), (*AfterGap)->GetSeatLabel()->Text.ToString(), FString(TEXT("5")));
	}

	// Seen from straight above, far away - the rail camera.
	AVibH2OBubbleActor* Bubble = Mothers[0];
	const UTextRenderComponent* Label = Bubble->GetSeatLabel();
	FVector Centre;
	float Radius = 0.0f;
	Bubble->GetDrawnSphere(Centre, Radius);
	TestTrue(FString::Printf(TEXT("Rayon mesure a l'echelle du spectacle (%.1f cm)"), Radius), Radius > 50.0f);
	const FVector View = Centre + FVector(0.0f, 0.0f, 30000.0f);
	Bubble->PlaceSeatLabel(View);

	const FVector Offset = Label->GetComponentLocation() - Centre;
	TestTrue(FString::Printf(TEXT("Devant la bulle, cote spectateur (%.1f cm pour un rayon de %.1f)"), Offset.Z, Radius),
		Offset.Z > Radius && Offset.Z < Radius * 1.2f);
	TestTrue(TEXT("Au milieu de la bulle vue d'en haut"), Offset.Size2D() < 1.0f);
	TestTrue(TEXT("Tourne vers la camera, pas vers le fond (chiffres a l'endroit)"),
		FVector::DotProduct(Label->GetForwardVector(), FVector::UpVector) > 0.99f);
	const float Height = Label->GetComponentScale().X * Label->WorldSize;
	TestTrue(FString::Printf(TEXT("Hauteur des chiffres = 0,55 x diametre (%.1f cm pour %.1f)"), Height, 2.0f * Radius),
		FMath::IsNearlyEqual(Height, 2.0f * Radius * Stage->SeatNumberSize, 0.5f));

	Stage->bShowSeatNumbers = false;
	Stage->Tick(Dt);
	TestFalse(TEXT("Decoche : les numeros disparaissent"), Bubble->GetSeatLabel()->IsVisible());

	return true;
}

#endif // WITH_EDITOR

#endif // WITH_DEV_AUTOMATION_TESTS
