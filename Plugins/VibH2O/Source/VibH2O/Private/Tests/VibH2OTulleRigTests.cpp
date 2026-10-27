// Automation tests for the tulles in depth.
//
// What they pin down: the face tulle fills the camera at the chosen distance
// and sets the scale; every other tulle lands at its measured offset; each
// back tulle's off-axis frustum passes exactly through its four edges, seen
// from the main camera's eye; and a bubble goes to a back tulle only when it
// is both deep enough and inside that tulle's frame, without flickering at
// the boundary.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Misc/App.h"
#include "Misc/ScopeExit.h"
#include "TextureResource.h"
#include "Osc/VibH2OOscTypes.h"
#include "VibH2OBubbleActor.h"
#include "VibH2OSettings.h"
#include "VibH2OStageActor.h"
#include "VibH2OSubsystem.h"
#include "VibH2OTulleRig.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace VibH2OTulleRigTestUtils
{
	/** Where a point lands in the tulle's picture: -1..1 across, -1..1 up. */
	static FVector2D ToScreen(const FVibH2OOffAxisView& View, const FVector& Point)
	{
		const FVector ToPoint = Point - View.Eye;
		const FVector4 ViewSpace(FVector::DotProduct(ToPoint, View.Right), FVector::DotProduct(ToPoint, View.Up), FVector::DotProduct(ToPoint, View.Forward), 1.0);
		const FVector4 Clip = View.MakeProjectionMatrix(10.0).TransformFVector4(ViewSpace);
		return FVector2D(Clip.X / Clip.W, Clip.Y / Clip.W);
	}

	/** The point on the ray from the eye through Target that lies Depth metres behind the face tulle. */
	static FVector OnRayAtDepth(const FVibH2OTulleFrame& Frame, const FVector& Eye, const FVector& Target, double Depth)
	{
		const double Along = FVector::DotProduct(Target - Eye, Frame.Back);
		const double T = (Depth * Frame.Scale + FVector::DotProduct(Frame.Origin - Eye, Frame.Back)) / Along;
		return Eye + (Target - Eye) * T;
	}
}

// ---------------------------------------------------------------------------
// The geometry, without a world.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2OTulleGeometryTest,
	"VibH2O.Tulles.Geometrie",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FVibH2OTulleGeometryTest::RunTest(const FString& Parameters)
{
	using namespace VibH2OTulleRigTestUtils;

	// An eye at the origin looking along +X with a 90° lens: at 10 m the view
	// is 20 m wide, so a real metre of the 8 m face tulle is worth 250 cm.
	FVibH2OTulleFrame Frame;
	const bool bFrame = AVibH2OTulleRig::ComputeFaceFrame(FVector::ZeroVector, FRotator::ZeroRotator, 90.0f, 1000.0, 8.0f, 6.0f, Frame);
	TestTrue(TEXT("Le tulle face se construit"), bFrame);
	TestEqual(TEXT("Echelle : 250 cm virtuels par metre reel"), Frame.Scale, 250.0, 0.001);
	TestTrue(FString::Printf(TEXT("Pied du tulle face a 10 m devant, 7,5 m sous l'axe (%s)"), *Frame.Origin.ToString()),
		Frame.Origin.Equals(FVector(1000.0, 0.0, -750.0), 0.001));

	// The jardin tulle as measured: 2.45 m behind, 2.05 m toward jardin, 2.7 x 5.4 m.
	FVibH2OTulle Jardin;
	Jardin.Lateral = -2.05f;
	Jardin.Depth = 2.45f;
	Jardin.Width = 2.7f;
	Jardin.Height = 5.4f;
	FVector BottomLeft;
	FVector BottomRight;
	FVector TopLeft;
	AVibH2OTulleRig::GetTulleCorners(Frame, Jardin, BottomLeft, BottomRight, TopLeft);
	TestTrue(FString::Printf(TEXT("Coin bas gauche du tulle jardin (%s)"), *BottomLeft.ToString()), BottomLeft.Equals(FVector(1612.5, -850.0, -750.0), 0.01));
	TestTrue(FString::Printf(TEXT("Coin bas droit (%s)"), *BottomRight.ToString()), BottomRight.Equals(FVector(1612.5, -175.0, -750.0), 0.01));
	TestTrue(FString::Printf(TEXT("Coin haut gauche (%s)"), *TopLeft.ToString()), TopLeft.Equals(FVector(1612.5, -850.0, 600.0), 0.01));
	TestEqual(TEXT("Le tulle jardin est a 2,45 m derriere le tulle face"), AVibH2OTulleRig::GetRealDepth(Frame, BottomLeft), 2.45, 0.0001);

	// Its off-axis frustum passes through its edges exactly.
	FVibH2OOffAxisView View;
	TestTrue(TEXT("L'oeil voit le tulle jardin"), AVibH2OTulleRig::ComputeOffAxisView(FVector::ZeroVector, BottomLeft, BottomRight, TopLeft, View));
	const FVector TopRight = BottomRight + (TopLeft - BottomLeft);
	TestTrue(TEXT("Bas gauche en (-1, -1)"), ToScreen(View, BottomLeft).Equals(FVector2D(-1.0, -1.0), 0.0001));
	TestTrue(TEXT("Bas droit en (1, -1)"), ToScreen(View, BottomRight).Equals(FVector2D(1.0, -1.0), 0.0001));
	TestTrue(TEXT("Haut gauche en (-1, 1)"), ToScreen(View, TopLeft).Equals(FVector2D(-1.0, 1.0), 0.0001));
	TestTrue(TEXT("Haut droit en (1, 1)"), ToScreen(View, TopRight).Equals(FVector2D(1.0, 1.0), 0.0001));
	const FVector Middle = (BottomLeft + TopRight) * 0.5;
	TestTrue(TEXT("Le milieu du tulle au milieu de son image, meme vu de biais"), ToScreen(View, Middle).Equals(FVector2D::ZeroVector, 0.0001));
	TestTrue(TEXT("Ce qui est derriere, sur le rayon du milieu, tombe au meme endroit"), ToScreen(View, Middle * 3.0).Equals(FVector2D::ZeroVector, 0.0001));
	TestTrue(TEXT("Le milieu est dans le cadre"), View.Contains(Middle * 2.0));
	TestFalse(TEXT("L'axe de la camera passe a cote du tulle jardin"), View.Contains(FVector(3000.0, 0.0, 0.0)));
	TestFalse(TEXT("Rien derriere l'oeil"), View.Contains(-Middle));
	const float CullingFov = View.GetEnclosingFOV(0.5);
	TestTrue(FString::Printf(TEXT("Le champ de culling englobe le tulle (%.1f deg)"), CullingFov), CullingFov > 2.0f * FMath::RadiansToDegrees(FMath::Atan(850.0 / 1612.5)) && CullingFov < 170.0f);

	TestFalse(TEXT("Un oeil derriere le tulle ne le voit pas"),
		AVibH2OTulleRig::ComputeOffAxisView(FVector(2000.0, 0.0, 0.0), BottomLeft, BottomRight, TopLeft, View));

	// The bac lies on the floor in front: the eye, above the floor, looks down onto it.
	FVibH2OTulle Bac;
	Bac.Facing = EVibH2OTulleFacing::Floor;
	Bac.Width = 8.0f;
	Bac.Height = 1.71f;
	Bac.Depth = -0.855f;
	AVibH2OTulleRig::GetTulleCorners(Frame, Bac, BottomLeft, BottomRight, TopLeft);
	TestTrue(FString::Printf(TEXT("Le bac touche le pied du tulle face (%s)"), *TopLeft.ToString()), TopLeft.Equals(FVector(1000.0, -1000.0, -750.0), 0.01));
	TestTrue(TEXT("L'oeil voit le bac d'en haut"), AVibH2OTulleRig::ComputeOffAxisView(FVector::ZeroVector, BottomLeft, BottomRight, TopLeft, View));
	TestTrue(TEXT("Bac : bord cote public en bas de l'image"), ToScreen(View, BottomLeft).Equals(FVector2D(-1.0, -1.0), 0.0001));
	TestTrue(TEXT("Bac : bord cote tulle en haut de l'image"), ToScreen(View, TopLeft).Equals(FVector2D(-1.0, 1.0), 0.0001));
	return true;
}

// ---------------------------------------------------------------------------
// Which tulle shows a bubble.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2OTulleShareTest,
	"VibH2O.Tulles.Repartition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FVibH2OTulleShareTest::RunTest(const FString& Parameters)
{
	using namespace VibH2OTulleRigTestUtils;

	// The measured room: jardin, cour, and the bac left unused.
	const TArray<FVibH2OTulle> Tulles = GetDefault<AVibH2OTulleRig>()->Tulles;
	if (!TestEqual(TEXT("La salle mesuree a trois tulles en plus du face"), Tulles.Num(), 3))
	{
		return false;
	}
	TestFalse(TEXT("Le bac n'est pas utilise d'office"), Tulles[2].bEnabled);

	const FVector Eye = FVector::ZeroVector;
	FVibH2OTulleFrame Frame;
	AVibH2OTulleRig::ComputeFaceFrame(Eye, FRotator::ZeroRotator, 60.0f, 1000.0, 8.0f, 6.0f, Frame);
	TArray<FVibH2OOffAxisView> Views;
	TArray<bool> Valid;
	TArray<FVector> Centres;
	for (const FVibH2OTulle& Tulle : Tulles)
	{
		FVector BottomLeft;
		FVector BottomRight;
		FVector TopLeft;
		AVibH2OTulleRig::GetTulleCorners(Frame, Tulle, BottomLeft, BottomRight, TopLeft);
		Valid.Add(AVibH2OTulleRig::ComputeOffAxisView(Eye, BottomLeft, BottomRight, TopLeft, Views.AddDefaulted_GetRef()));
		Centres.Add((BottomRight + TopLeft) * 0.5);
	}
	TestTrue(TEXT("Jardin et cour sont vus"), Valid[0] && Valid[1]);

	auto Pick = [&](const FVector& Point, int32 Current)
	{
		return AVibH2OTulleRig::PickTulle(Tulles, Views, Valid, Frame, Point, Current, 0.1f);
	};

	TestEqual(TEXT("Sur le tulle jardin : jardin"), Pick(OnRayAtDepth(Frame, Eye, Centres[0], 2.45), INDEX_NONE), 0);
	TestEqual(TEXT("Loin derriere, dans le cadre de cour : cour"), Pick(OnRayAtDepth(Frame, Eye, Centres[1], 6.0), INDEX_NONE), 1);
	TestEqual(TEXT("Pas assez profond : reste sur le face"), Pick(OnRayAtDepth(Frame, Eye, Centres[0], 0.5), INDEX_NONE), INDEX_NONE);
	TestEqual(TEXT("Devant le tulle face : reste sur le face"), Pick(OnRayAtDepth(Frame, Eye, Centres[0], -1.0), INDEX_NONE), INDEX_NONE);

	// Between the two back tulles nothing can show it: it stays on the face tulle.
	const FVector Gap = Frame.Origin + Frame.Right * (0.3 * Frame.Scale) + Frame.Back * (2.45 * Frame.Scale) + Frame.Up * (2.0 * Frame.Scale);
	TestEqual(TEXT("Dans l'espace entre jardin et cour : reste sur le face"), Pick(Gap, INDEX_NONE), INDEX_NONE);

	// The boundary at 1.2 m: entering takes the full depth, leaving takes 10 cm more.
	TestEqual(TEXT("A 1,15 m, une bulle du face y reste"), Pick(OnRayAtDepth(Frame, Eye, Centres[0], 1.15), INDEX_NONE), INDEX_NONE);
	TestEqual(TEXT("A 1,15 m, une bulle du jardin y reste aussi"), Pick(OnRayAtDepth(Frame, Eye, Centres[0], 1.15), 0), 0);
	TestEqual(TEXT("A 1,05 m, elle revient au face"), Pick(OnRayAtDepth(Frame, Eye, Centres[0], 1.05), 0), INDEX_NONE);

	// The unused bac never takes anything.
	TestEqual(TEXT("Le bac inutilise ne prend rien"), Pick(OnRayAtDepth(Frame, Eye, Centres[2], -0.855), INDEX_NONE), INDEX_NONE);
	return true;
}

#if WITH_EDITOR

// ---------------------------------------------------------------------------
// In a level: the stage's bubbles shared out under a camera.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2OTulleSceneTest,
	"VibH2O.Tulles.SceneEtCamera",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibH2OTulleSceneTest::RunTest(const FString& Parameters)
{
	UVibH2OSettings* Settings = GetMutableDefault<UVibH2OSettings>();
	const float SavedSettle = Settings->RoomMappingSettleSeconds;
	Settings->RoomMappingSettleSeconds = 0.0f;
	ON_SCOPE_EXIT
	{
		Settings->RoomMappingSettleSeconds = SavedSettle;
	};

	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false, TEXT("VibH2O_TestTulles"));
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

	// A 7 x 3 room on the floor, a 90° camera 10 m above it looking down.
	AVibH2OStageActor* Stage = World->SpawnActor<AVibH2OStageActor>(FVector::ZeroVector, FRotator::ZeroRotator);
	ACameraActor* Camera = World->SpawnActor<ACameraActor>(FVector(0.0, 0.0, 1000.0), FRotator(-90.0, 0.0, 0.0));
	AVibH2OTulleRig* Rig = World->SpawnActor<AVibH2OTulleRig>();
	if (!TestNotNull(TEXT("Scene"), Stage) || !TestNotNull(TEXT("Camera"), Camera) || !TestNotNull(TEXT("Tulles"), Rig))
	{
		return false;
	}
	Stage->FloatAmplitude = 0.0f;
	Stage->DriftIntensity = 0.0f;
	Camera->GetCameraComponent()->SetFieldOfView(90.0f);
	Rig->Stage = Stage;
	Rig->Camera = Camera;

	Subsystem->InjectOscMessage([] { FVibH2OOscMessage M(TEXT("/RoomMapping/columns/")); M.Args.Add(FVibH2OOscValue::MakeInt(7)); return M; }());
	Subsystem->InjectOscMessage([] { FVibH2OOscMessage M(TEXT("/RoomMapping/rows/")); M.Args.Add(FVibH2OOscValue::MakeInt(3)); return M; }());
	for (int32 Seat = 1; Seat <= 21; ++Seat)
	{
		FVibH2OOscMessage Message(FString::Printf(TEXT("/RoomMapping/%d/"), Seat));
		Message.Args.Add(FVibH2OOscValue::MakeInt(100 + Seat));
		Subsystem->InjectOscMessage(Message);
	}
	Subsystem->Tick(0.5f);
	Stage->Tick(1.0f / 60.0f);
	Stage->Tick(1.0f / 60.0f);
	const int32 BubbleCount = Stage->GetBubbles().Num() + Stage->GetDaughters().Num();
	TestTrue(FString::Printf(TEXT("La salle est construite (%d bulles)"), BubbleCount), BubbleCount >= 21);

	// Face tulle on the stage's plane: the room lies on it, nothing is deep enough to leave.
	Rig->FacePlaneDistance = 0.0f;
	Rig->UpdateLayout(1.0f / 60.0f);
	TestEqual(TEXT("Tulle face sur la scene : 10 m de large vus a 90 deg, soit 250 cm par metre"), Rig->VirtualCmPerMetre, 250.0f, 0.01f);
	TestEqual(TEXT("Spectateur de reference a 4 m du tulle face"), Rig->ReferenceSeatDistance, 4.0f, 0.001f);
	TestEqual(TEXT("Toutes les bulles restent sur le face"), Rig->BubblesOnFace, BubbleCount);

	// Face tulle 6 m above the stage: at 100 cm per metre the room lies 4 m
	// behind it, and the bubbles that fall in the back tulles' frames go there.
	Rig->FacePlaneDistance = 400.0f;
	Rig->UpdateLayout(1.0f / 60.0f);
	int32 OnBackTulles = 0;
	for (const int32 Count : Rig->BubblesPerTulle)
	{
		OnBackTulles += Count;
	}
	TestEqual(TEXT("Chaque bulle est sur un tulle et un seul"), Rig->BubblesOnFace + OnBackTulles, BubbleCount);
	TestTrue(FString::Printf(TEXT("Des bulles passent sur les tulles du fond (%d)"), OnBackTulles), OnBackTulles > 0);
	TestTrue(FString::Printf(TEXT("Celles du milieu restent sur le face (%d)"), Rig->BubblesOnFace), Rig->BubblesOnFace > 0);
	TestEqual(TEXT("Le bac inutilise n'a rien"), Rig->BubblesPerTulle.IsValidIndex(2) ? Rig->BubblesPerTulle[2] : -1, 0);

	// Off, and in the editor anyway: no picture is rendered.
	TestNull(TEXT("Rien n'est rendu tant que le rig est eteint"), Rig->GetTulleTarget(0));
	return true;
}

// ---------------------------------------------------------------------------
// Through the real renderer: the off-axis picture is neither mirrored nor
// flipped, and what sits on a tulle's corner lands in that corner.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2OTulleRenderTest,
	"VibH2O.Tulles.RenduReel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibH2OTulleRenderTest::RunTest(const FString& Parameters)
{
	if (!FApp::CanEverRender())
	{
		AddInfo(TEXT("Lance sans rendu (-nullrhi) : test du rendu reel saute. Le relancer avec -RenderOffscreen."));
		return true;
	}

	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false, TEXT("VibH2O_TestTullesRendu"));
	if (World == nullptr || World->Scene == nullptr)
	{
		AddError(TEXT("Impossible de creer un monde de test avec une scene de rendu."));
		return false;
	}
	ON_SCOPE_EXIT
	{
		World->DestroyWorld(false);
	};

	// A 90° eye at the origin, the face tulle 4 m ahead: 100 cm per real metre.
	// The jardin tulle then spans y -340..-70 and z -300..240, 6.45 m ahead.
	FVibH2OTulleFrame Frame;
	AVibH2OTulleRig::ComputeFaceFrame(FVector::ZeroVector, FRotator::ZeroRotator, 90.0f, 400.0, 8.0f, 6.0f, Frame);
	FVibH2OTulle Jardin = GetDefault<AVibH2OTulleRig>()->Tulles[0];
	FVector BottomLeft;
	FVector BottomRight;
	FVector TopLeft;
	AVibH2OTulleRig::GetTulleCorners(Frame, Jardin, BottomLeft, BottomRight, TopLeft);
	FVibH2OOffAxisView View;
	if (!TestTrue(TEXT("L'oeil voit le tulle jardin"), AVibH2OTulleRig::ComputeOffAxisView(FVector::ZeroVector, BottomLeft, BottomRight, TopLeft, View)))
	{
		return false;
	}

	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (!TestNotNull(TEXT("Sphere du moteur"), Sphere))
	{
		return false;
	}
	auto SpawnBall = [World, Sphere](const FVector& Location)
	{
		AStaticMeshActor* Ball = World->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator);
		Ball->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
		Ball->GetStaticMeshComponent()->SetStaticMesh(Sphere);
		Ball->SetActorScale3D(FVector(0.4));
		return Ball;
	};
	// One ball in the middle of the tulle, one 40 cm inside its bottom-left corner.
	const FVector TopRight = BottomRight + (TopLeft - BottomLeft);
	SpawnBall((BottomLeft + TopRight) * 0.5);
	SpawnBall(BottomLeft + View.Right * 40.0 + View.Up * 40.0);

	const FIntPoint Size(64, 128);
	UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>();
	Target->RenderTargetFormat = RTF_RGBA16f;
	Target->ClearColor = FLinearColor::Black;
	Target->InitAutoFormat(Size.X, Size.Y);
	Target->UpdateResourceImmediate(true);

	// Base colour needs no light: the balls are grey, the void is black.
	AActor* Holder = World->SpawnActor<AActor>();
	USceneCaptureComponent2D* Capture = NewObject<USceneCaptureComponent2D>(Holder);
	Capture->TextureTarget = Target;
	Capture->CaptureSource = ESceneCaptureSource::SCS_BaseColor;
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->bUseCustomProjectionMatrix = true;
	Capture->CustomProjectionMatrix = View.MakeProjectionMatrix(GNearClippingPlane);
	Capture->FOVAngle = View.GetEnclosingFOV(static_cast<double>(Size.X) / Size.Y);
	Capture->RegisterComponentWithWorld(World);
	Capture->SetWorldLocationAndRotation(View.Eye, FRotationMatrix::MakeFromXZ(View.Forward, View.Up).Rotator());

	// The capture and the read-back queue on the render thread in order; reading waits for both.
	Capture->CaptureScene();

	TArray<FLinearColor> Pixels;
	FTextureRenderTargetResource* Resource = Target->GameThread_GetRenderTargetResource();
	if (!TestNotNull(TEXT("Cible de rendu"), Resource) || !TestTrue(TEXT("Pixels lus"), Resource->ReadLinearColorPixels(Pixels)) || Pixels.Num() != Size.X * Size.Y)
	{
		return false;
	}
	// Brightest value in a small square around a pixel, row 0 at the top.
	auto Around = [&Pixels, Size](int32 X, int32 Y)
	{
		float Brightest = 0.0f;
		for (int32 DY = -2; DY <= 2; ++DY)
		{
			for (int32 DX = -2; DX <= 2; ++DX)
			{
				const int32 PX = FMath::Clamp(X + DX, 0, Size.X - 1);
				const int32 PY = FMath::Clamp(Y + DY, 0, Size.Y - 1);
				const FLinearColor& C = Pixels[PY * Size.X + PX];
				Brightest = FMath::Max(Brightest, FMath::Max3(C.R, C.G, C.B));
			}
		}
		return Brightest;
	};

	// The corner ball is 40 cm in on a 270 x 540 cm tulle: 9.5 px from the left, 9.5 px from the bottom.
	const float Middle = Around(Size.X / 2, Size.Y / 2);
	const float Corner = Around(9, Size.Y - 1 - 9);
	const float Mirrored = Around(Size.X - 1 - 9, Size.Y - 1 - 9);
	const float Flipped = Around(9, 9);
	AddInfo(FString::Printf(TEXT("Rendu reel : milieu %.3f, coin bas gauche %.3f, bas droite %.3f, haut gauche %.3f"), Middle, Corner, Mirrored, Flipped));
	TestTrue(FString::Printf(TEXT("La bulle du milieu est au milieu de l'image (%.3f)"), Middle), Middle > 0.05f);
	TestTrue(FString::Printf(TEXT("La bulle du coin bas gauche est en bas a gauche (%.3f)"), Corner), Corner > 0.05f);
	TestTrue(FString::Printf(TEXT("Rien en bas a droite : l'image n'est pas en miroir (%.3f)"), Mirrored), Mirrored < 0.01f);
	TestTrue(FString::Printf(TEXT("Rien en haut a gauche : l'image n'est pas a l'envers (%.3f)"), Flipped), Flipped < 0.01f);
	return true;
}

// ---------------------------------------------------------------------------
// Switched on during play: one picture per tulle used, released when off.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2OTulleOutputsTest,
	"VibH2O.Tulles.SortiesEnJeu",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibH2OTulleOutputsTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("VibH2O_TestTullesJeu"));
	if (World == nullptr)
	{
		AddError(TEXT("Impossible de creer un monde de jeu de test."));
		return false;
	}
	ON_SCOPE_EXIT
	{
		World->DestroyWorld(false);
	};

	ACameraActor* Camera = World->SpawnActor<ACameraActor>(FVector(0.0, 0.0, 1000.0), FRotator(-90.0, 0.0, 0.0));
	AVibH2OTulleRig* Rig = World->SpawnActor<AVibH2OTulleRig>();
	if (!TestNotNull(TEXT("Camera"), Camera) || !TestNotNull(TEXT("Tulles"), Rig))
	{
		return false;
	}
	Rig->Camera = Camera;

	Rig->Tick(1.0f / 60.0f);
	TestNull(TEXT("Eteint : aucune image"), Rig->GetTulleTarget(0));

	Rig->bActive = true;
	Rig->Tick(1.0f / 60.0f);
	UTextureRenderTarget2D* JardinTarget = Rig->GetTulleTarget(0);
	if (TestNotNull(TEXT("Allume : le tulle jardin a son image"), JardinTarget))
	{
		TestTrue(TEXT("A la resolution de sa mire, 960 x 1920"), FIntPoint(JardinTarget->SizeX, JardinTarget->SizeY) == FIntPoint(960, 1920));
	}
	TestNotNull(TEXT("Le tulle cour a la sienne"), Rig->GetTulleTarget(1));
	TestNull(TEXT("Le bac inutilise n'en a pas"), Rig->GetTulleTarget(2));

	// Changing the list rebuilds the outputs.
	Rig->Tulles[2].bEnabled = true;
	Rig->Tick(1.0f / 60.0f);
	UTextureRenderTarget2D* BacTarget = Rig->GetTulleTarget(2);
	if (TestNotNull(TEXT("Le bac mis en service a son image"), BacTarget))
	{
		TestTrue(TEXT("A la resolution du bac, 1920 x 410"), FIntPoint(BacTarget->SizeX, BacTarget->SizeY) == FIntPoint(1920, 410));
	}

	Rig->bActive = false;
	Rig->Tick(1.0f / 60.0f);
	TestNull(TEXT("Eteint de nouveau : les images sont rendues"), Rig->GetTulleTarget(0));
	return true;
}

#endif // WITH_EDITOR

#endif // WITH_DEV_AUTOMATION_TESTS
