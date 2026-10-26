// Automation tests for the control desk.
//
// What they pin down: the camera's end point follows the number of seats (a
// small audience brings the camera closer, a large one holds it back, and
// either way every seat is in the picture), the tablet's commands reach the
// actors the desk drives, and a preset gives back exactly what was saved.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "CameraRig_Rail.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Components/SplineComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"
#include "Osc/VibH2OOscTypes.h"
#include "VibH2ORegie.h"
#include "VibH2OSettings.h"
#include "VibH2OStageActor.h"
#include "VibH2OSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

// ---------------------------------------------------------------------------
// The framing test, as pure geometry.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2ORegieFramingMathTest,
	"VibH2O.Regie.CadrageGeometrie",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FVibH2ORegieFramingMathTest::RunTest(const FString& Parameters)
{
	// A room 100 wide and 50 deep, seen from straight above by a 90° camera:
	// half-width 50 needs a distance of 50, the 16:9 height 28.1.
	FVector Corners[8];
	FBox(FVector(-50.0, -25.0, 0.0), FVector(50.0, 25.0, 0.0)).GetVertices(Corners);
	const FVector Forward(0.0, 0.0, -1.0);
	const FVector Right(1.0, 0.0, 0.0);
	const FVector Up(0.0, -1.0, 0.0);

	TestTrue(TEXT("A 60 cm au-dessus, toute la salle tient"),
		AVibH2ORegie::IsFramed(Corners, FVector(0.0, 0.0, 60.0), Forward, Right, Up, 90.0f, 16.0f / 9.0f, 0.0f));
	TestFalse(TEXT("A 40 cm, les bords sortent de l'image"),
		AVibH2ORegie::IsFramed(Corners, FVector(0.0, 0.0, 40.0), Forward, Right, Up, 90.0f, 16.0f / 9.0f, 0.0f));
	TestFalse(TEXT("La marge recule la camera : 20 % de marge ne tient plus a 55 cm"),
		AVibH2ORegie::IsFramed(Corners, FVector(0.0, 0.0, 55.0), Forward, Right, Up, 90.0f, 16.0f / 9.0f, 0.2f));
	TestFalse(TEXT("Une camera sous la salle ne la voit pas"),
		AVibH2ORegie::IsFramed(Corners, FVector(0.0, 0.0, -60.0), Forward, Right, Up, 90.0f, 16.0f / 9.0f, 0.0f));
	return true;
}

#if WITH_EDITOR

namespace VibH2ORegieTestUtils
{
	static FVibH2OOscMessage MakeInt(const FString& Address, int32 Value)
	{
		FVibH2OOscMessage Message(Address);
		Message.Args.Add(FVibH2OOscValue::MakeInt(Value));
		return Message;
	}

	static FVibH2OOscMessage MakeText(const FString& Address, const TArray<FString>& Words)
	{
		FVibH2OOscMessage Message(Address);
		for (const FString& Word : Words)
		{
			Message.Args.Add(FVibH2OOscValue::MakeString(Word));
		}
		return Message;
	}

	/** Sends a fresh room plan of Columns x Rows, every seat taken. */
	static void SendRoom(UVibH2OSubsystem* Subsystem, int32 Columns, int32 Rows)
	{
		Subsystem->InjectOscMessage(MakeInt(TEXT("/RoomMapping/columns/"), Columns));
		Subsystem->InjectOscMessage(MakeInt(TEXT("/RoomMapping/rows/"), Rows));
		for (int32 SeatIndex = 1; SeatIndex <= Columns * Rows; ++SeatIndex)
		{
			Subsystem->InjectOscMessage(MakeInt(FString::Printf(TEXT("/RoomMapping/%d/"), SeatIndex), 100 + SeatIndex));
		}
	}
}

// ---------------------------------------------------------------------------
// The end of the descent follows the audience.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2ORegieFramingRailTest,
	"VibH2O.Regie.CadrageSelonLaSalle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibH2ORegieFramingRailTest::RunTest(const FString& Parameters)
{
	using namespace VibH2ORegieTestUtils;

	UVibH2OSettings* Settings = GetMutableDefault<UVibH2OSettings>();
	const float SavedSettle = Settings->RoomMappingSettleSeconds;
	Settings->RoomMappingSettleSeconds = 0.0f;
	ON_SCOPE_EXIT
	{
		Settings->RoomMappingSettleSeconds = SavedSettle;
	};

	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false, TEXT("VibH2O_TestRegieCadrage"));
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

	// The show's layout: a vertical rail 2 km long, high above a stage at
	// scale 8, the camera hanging under it and looking down.
	AVibH2OStageActor* Stage = World->SpawnActor<AVibH2OStageActor>(FVector(0.0, 0.0, -60000.0), FRotator::ZeroRotator);
	ACameraRig_Rail* Rail = World->SpawnActor<ACameraRig_Rail>(FVector(0.0, 0.0, 70000.0), FRotator(-90.0, 0.0, 0.0));
	ACineCameraActor* Camera = World->SpawnActor<ACineCameraActor>(FVector(0.0, 0.0, 70000.0), FRotator(-90.0, -90.0, 0.0));
	if (!TestNotNull(TEXT("Scene"), Stage) || !TestNotNull(TEXT("Rail"), Rail) || !TestNotNull(TEXT("Camera"), Camera))
	{
		return false;
	}
	Stage->SetActorScale3D(FVector(8.0f));
	Stage->FloatAmplitude = 0.0f;
	Stage->DriftIntensity = 0.0f;
	Rail->GetRailSplineComponent()->SetSplinePoints({ FVector::ZeroVector, FVector(200000.0, 0.0, 0.0) }, ESplineCoordinateSpace::Local);
	Camera->AttachToActor(Rail, FAttachmentTransformRules::KeepWorldTransform);

	AVibH2ORegie* Desk = World->SpawnActor<AVibH2ORegie>();
	if (!TestNotNull(TEXT("Regie"), Desk))
	{
		return false;
	}
	Desk->Stage = Stage;
	Desk->Rail = Rail;
	Desk->Reglages.FramingMargin = 0.1f;

	const float Dt = 1.0f / 60.0f;
	SendRoom(Subsystem, 12, 8);
	Subsystem->Tick(0.5f);
	Stage->Tick(Dt);
	Stage->Tick(Dt);
	Desk->RefreshFraming();
	const float LargeRoom = Desk->FramedRailPosition;

	SendRoom(Subsystem, 4, 3);
	// The plan is closed by a silence on the subsystem's clock: run it.
	Subsystem->Tick(0.5f);
	Stage->Tick(Dt);
	Stage->Tick(Dt);
	Desk->RefreshFraming();
	const float SmallRoom = Desk->FramedRailPosition;

	TestTrue(FString::Printf(TEXT("Une fin de descente est trouvee (%.3f, %.3f)"), LargeRoom, SmallRoom), LargeRoom > 0.0f && SmallRoom > 0.0f);
	TestEqual(TEXT("La petite salle est bien construite"), Stage->GetBubbles().Num(), 12);
	TestTrue(TEXT("Moins de spectateurs : la camera descend plus pres"), SmallRoom > LargeRoom + 0.01f);

	// At that point every bubble is in the picture, and a little further
	// down some leave it: the end point is as close as the room allows.
	const FBox Room = Stage->GetRoomBounds();
	FVector Corners[8];
	Room.GetVertices(Corners);
	USplineComponent* Spline = Rail->GetRailSplineComponent();
	const float Length = Spline->GetSplineLength();
	const FVector Offset = Camera->GetActorLocation() - Spline->GetLocationAtDistanceAlongSpline(Rail->CurrentPositionOnRail * Length, ESplineCoordinateSpace::World);
	const FQuat View = Camera->GetActorQuat();
	const UCineCameraComponent* Cine = Camera->GetCineCameraComponent();
	const float Fov = Cine->FieldOfView;
	const float Aspect = Cine->Filmback.SensorWidth / Cine->Filmback.SensorHeight;
	TestTrue(FString::Printf(TEXT("La camera a pris le format du tulle face, 4:3 (%.3f)"), Aspect), FMath::IsNearlyEqual(Aspect, 4.0f / 3.0f, 0.01f));
	auto FramedAt = [&](float P)
	{
		return AVibH2ORegie::IsFramed(Corners, Spline->GetLocationAtDistanceAlongSpline(P * Length, ESplineCoordinateSpace::World) + Offset,
			View.GetForwardVector(), View.GetRightVector(), View.GetUpVector(), Fov, Aspect, 0.1f);
	};
	TestTrue(TEXT("A la fin calculee, toute la salle est dans l'image"), FramedAt(SmallRoom));
	TestFalse(TEXT("Un peu plus bas, elle deborde"), FramedAt(SmallRoom + 0.01f));
	return true;
}

// ---------------------------------------------------------------------------
// The tablet's commands and the presets.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2ORegieCommandsTest,
	"VibH2O.Regie.CommandesEtPreReglages",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibH2ORegieCommandsTest::RunTest(const FString& Parameters)
{
	using namespace VibH2ORegieTestUtils;

	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false, TEXT("VibH2O_TestRegieCommandes"));
	if (World == nullptr)
	{
		AddError(TEXT("Impossible de creer un monde editeur de test."));
		return false;
	}
	ON_SCOPE_EXIT
	{
		World->DestroyWorld(false);
	};

	AVibH2OStageActor* Stage = World->SpawnActor<AVibH2OStageActor>();
	AVibH2ORegie* Desk = World->SpawnActor<AVibH2ORegie>();
	if (!TestNotNull(TEXT("Scene"), Stage) || !TestNotNull(TEXT("Regie"), Desk))
	{
		return false;
	}
	Desk->Stage = Stage;

	// The setting as the address, a number argument.
	FVibH2OOscMessage Light(TEXT("/Regie/BubbleBrightness"));
	Light.Args.Add(FVibH2OOscValue::MakeFloat(4.5f));
	Desk->HandleRegieMessage(Light);
	TestEqual(TEXT("La lumiere des bulles arrive a la scene"), Stage->BubbleBrightness, 4.5f);

	// "/Regie/Set <id> <value>", a colour as text.
	Desk->HandleRegieMessage(MakeText(TEXT("/Regie/Set"), { TEXT("ExcitedColorHigh"), TEXT("#FF0000") }));
	TestTrue(TEXT("La couleur choisie sur la tablette arrive a la palette"),
		Stage->MoodPalette.Excited.ColorHigh.Equals(FLinearColor(1.0f, 0.0f, 0.0f, 1.0f), 1e-3f));

	// Out of range is clamped, unknown is ignored.
	Desk->HandleRegieMessage(MakeText(TEXT("/Regie/Set"), { TEXT("BubbleBrightness"), TEXT("999") }));
	TestEqual(TEXT("Une valeur hors bornes est ramenee dans les bornes"), Stage->BubbleBrightness, 10.0f);
	AddExpectedError(TEXT("reglage inconnu"), EAutomationExpectedErrorFlags::Contains, 1);
	Desk->HandleRegieMessage(MakeText(TEXT("/Regie/Set"), { TEXT("Inexistant"), TEXT("1") }));

	// The tablet's state carries every control and its value.
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Desk->WriteRemoteState(Root);
	const TSharedPtr<FJsonObject>* Regie = nullptr;
	if (TestTrue(TEXT("L'etat de la tablette contient la regie"), Root->TryGetObjectField(TEXT("regie"), Regie)))
	{
		TestEqual(TEXT("Toutes les commandes sont publiees"), (*Regie)->GetArrayField(TEXT("controls")).Num(), AVibH2ORegie::GetControls().Num());
		TestEqual(TEXT("La couleur est publiee en hexadecimal"), (*Regie)->GetObjectField(TEXT("values"))->GetStringField(TEXT("ExcitedColorHigh")), FString(TEXT("#FF0000")));
	}

	// The image: starts from what the scene shows, moves from there, and
	// comes back to it in one press.
	APostProcessVolume* Volume = World->SpawnActor<APostProcessVolume>();
	Volume->bUnbound = true;
	Volume->Priority = 5.0f;
	Volume->Settings.bOverride_ColorSaturation = true;
	Volume->Settings.ColorSaturation = FVector4(1.0, 0.8, 1.2, 0.9);
	Desk->ExposureVolume = Volume;
	// The scene changed after the desk was placed: "read the scene" takes it in.
	Desk->CaptureFromScene();
	Desk->ResetImage();
	TestEqual(TEXT("L'image d'origine reprend la saturation de la scene"), Volume->Settings.ColorSaturation.W, 0.9);
	Desk->HandleRegieMessage(MakeText(TEXT("/Regie/Set"), { TEXT("ImageSaturation"), TEXT("0.5") }));
	TestEqual(TEXT("Le curseur agit sur la saturation globale"), Volume->Settings.ColorSaturation.W, 0.45, 1e-4);
	TestEqual(TEXT("La teinte par canal de l'artiste reste"), Volume->Settings.ColorSaturation.Y, 0.8, 1e-4);
	Desk->HandleRegieMessage(MakeText(TEXT("/Regie/Set"), { TEXT("ImageWarmth"), TEXT("1") }));
	const float Warm = Volume->Settings.WhiteTemp;
	FVibH2OOscMessage Reset(TEXT("/Regie/Image/Reset"));
	Desk->HandleRegieMessage(Reset);
	TestEqual(TEXT("Retour a l'image d'origine : saturation"), Volume->Settings.ColorSaturation.W, 0.9, 1e-4);
	TestTrue(TEXT("Retour a l'image d'origine : chaud / froid"), Volume->Settings.WhiteTemp < Warm - 1000.0f);

	// A preset gives back exactly what was saved, and only that.
	const FString Name = TEXT("_test_automatique");
	Desk->Reglages.Exposure = 11.25f;
	Desk->Reglages.WaterColor = FLinearColor(0.2f, 0.4f, 0.6f, 1.0f);
	TestTrue(TEXT("Preset enregistre"), Desk->SavePreset(Name));
	TestTrue(TEXT("Preset liste"), Desk->ListPresets().Contains(Name));
	Desk->Reglages.Exposure = 7.0f;
	Desk->Reglages.WaterColor = FLinearColor::Black;
	Desk->HandleRegieMessage(MakeText(TEXT("/Regie/Preset/Load"), { Name }));
	TestEqual(TEXT("Le preset rend l'exposition"), Desk->Reglages.Exposure, 11.25f);
	TestTrue(TEXT("Le preset rend la couleur de l'eau"), Desk->Reglages.WaterColor.Equals(FLinearColor(0.2f, 0.4f, 0.6f, 1.0f), 1e-4f));
	Desk->HandleRegieMessage(MakeText(TEXT("/Regie/Preset/Delete"), { Name }));
	TestFalse(TEXT("Preset supprime"), Desk->ListPresets().Contains(Name));
	return true;
}

#endif // WITH_EDITOR

#endif // WITH_DEV_AUTOMATION_TESTS
