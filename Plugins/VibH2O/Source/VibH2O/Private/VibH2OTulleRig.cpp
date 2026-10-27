#include "VibH2OTulleRig.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "CameraRig_Rail.h"
#include "Components/SceneCaptureComponent2D.h"
#include "DrawDebugHelpers.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "VibH2OBubbleActor.h"
#include "VibH2OModule.h"
#include "VibH2OStageActor.h"

namespace VibH2OTulleRigInternal
{
	/** Distance used when nothing better is known: the stage is missing or behind the camera. */
	constexpr double FallbackDistance = 1000.0;

	/** Widening of the culling frustum, so nothing on the edge of a tulle is culled away. */
	constexpr double CullingMargin = 1.02;

	// SpoutUE is optional: its sender is found by name and set by reflection,
	// as the desk already does for the main output.
	static void SetString(UObject* Object, FName Name, const FString& Value)
	{
		if (FStrProperty* Property = FindFProperty<FStrProperty>(Object->GetClass(), Name))
		{
			Property->SetPropertyValue_InContainer(Object, Value);
		}
	}

	static void SetEnum(UObject* Object, FName Name, const TCHAR* ValueName)
	{
		if (FEnumProperty* Property = FindFProperty<FEnumProperty>(Object->GetClass(), Name))
		{
			const int64 Value = Property->GetEnum()->GetValueByNameString(ValueName);
			if (Value != INDEX_NONE)
			{
				Property->GetUnderlyingProperty()->SetIntPropertyValue(Property->ContainerPtrToValuePtr<void>(Object), Value);
			}
		}
	}

	static void SetObject(UObject* Object, FName Name, UObject* Value)
	{
		if (FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Object->GetClass(), Name))
		{
			Property->SetObjectPropertyValue_InContainer(Object, Value);
		}
	}
}

// ------------------------------------------------------------- Off-axis view

bool FVibH2OOffAxisView::Contains(const FVector& Point) const
{
	const FVector ToPoint = Point - Eye;
	const double Along = FVector::DotProduct(ToPoint, Forward);
	if (Along <= UE_KINDA_SMALL_NUMBER || Distance <= UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}
	// Brought back onto the tulle's plane along the ray from the eye.
	const double X = FVector::DotProduct(ToPoint, Right) * Distance / Along;
	const double Y = FVector::DotProduct(ToPoint, Up) * Distance / Along;
	return X >= Left && X <= RightEdge && Y >= Bottom && Y <= Top;
}

FMatrix FVibH2OOffAxisView::MakeProjectionMatrix(double NearPlane) const
{
	// The general perspective of a window seen off-centre: the frustum's edges
	// pass through the tulle's edges, wherever the eye stands.
	const double Width = RightEdge - Left;
	const double Height = Top - Bottom;
	const double ScaleX = 2.0 * Distance / Width;
	const double ScaleY = 2.0 * Distance / Height;
	const double ShiftX = -(RightEdge + Left) / Width;
	const double ShiftY = -(Top + Bottom) / Height;
	return FMatrix(
		FPlane(ScaleX, 0.0, 0.0, 0.0),
		FPlane(0.0, ScaleY, 0.0, 0.0),
		FPlane(ShiftX, ShiftY, 0.0, 1.0),
		FPlane(0.0, 0.0, NearPlane, 0.0));
}

float FVibH2OOffAxisView::GetEnclosingFOV(double AspectRatio) const
{
	if (Distance <= UE_KINDA_SMALL_NUMBER)
	{
		return 90.0f;
	}
	const double TanHorizontal = FMath::Max(FMath::Abs(Left), FMath::Abs(RightEdge)) / Distance;
	const double TanVertical = FMath::Max(FMath::Abs(Bottom), FMath::Abs(Top)) / Distance;
	const double Tan = FMath::Max(TanHorizontal, TanVertical * AspectRatio) * VibH2OTulleRigInternal::CullingMargin;
	return FMath::Min(static_cast<float>(FMath::RadiansToDegrees(2.0 * FMath::Atan(Tan))), 170.0f);
}

// --------------------------------------------------------------------- Actor

AVibH2OTulleRig::AVibH2OTulleRig()
{
	PrimaryActorTick.bCanEverTick = true;
	// After the camera and the bubbles have moved: the captures render this frame's picture.
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));

	Tulles = MakeMeasuredRoom();
}

void AVibH2OTulleRig::ResetToMeasuredRoom()
{
	Modify();
	Tulles = MakeMeasuredRoom();
}

TArray<FVibH2OTulle> AVibH2OTulleRig::MakeMeasuredRoom()
{
	TArray<FVibH2OTulle> Room;

	// Scan of 4 September 2026 and the hall's test charts. The back tulles
	// hang 2.45 m behind the face tulle; their 1:2 ratio comes from the chart,
	// their width from the scan. The bac is the band on the floor in front.
	FVibH2OTulle Jardin;
	Jardin.Name = TEXT("Tulle jardin");
	Jardin.Lateral = -2.05f;
	Jardin.SpoutName = TEXT("VibH2O Tulle jardin");
	Room.Add(Jardin);

	FVibH2OTulle Cour;
	Cour.Name = TEXT("Tulle cour");
	Cour.Lateral = 2.65f;
	Cour.SpoutName = TEXT("VibH2O Tulle cour");
	Room.Add(Cour);

	FVibH2OTulle Bac;
	Bac.Name = TEXT("Bac");
	Bac.bEnabled = false;
	Bac.Facing = EVibH2OTulleFacing::Floor;
	Bac.Width = 8.0f;
	Bac.Height = 8.0f / 4.68f;
	Bac.Depth = -0.5f * Bac.Height;
	Bac.Resolution = FIntPoint(1920, 410);
	Bac.DepthFrom = -100.0f;
	Bac.DepthTo = -0.2f;
	Bac.SpoutName = TEXT("VibH2O Bac");
	Room.Add(Bac);
	return Room;
}

void AVibH2OTulleRig::BeginPlay()
{
	Super::BeginPlay();
	Assignment.Reset();
}

void AVibH2OTulleRig::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ApplyHiddenActors(true);
	ReleaseOutputs();
	Super::EndPlay(EndPlayReason);
}

void AVibH2OTulleRig::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateLayout(DeltaSeconds);

	if (ShouldRender())
	{
		const uint32 Signature = ComputeOutputsSignature();
		if (!bOutputsBuilt || Signature != OutputsSignature)
		{
			ReleaseOutputs();
			BuildOutputs();
		}
		ApplyOutputs();
		ApplyHiddenActors(false);
	}
	else if (bOutputsBuilt || HiddenFromMain.Num() > 0)
	{
		ApplyHiddenActors(true);
		ReleaseOutputs();
	}

#if ENABLE_DRAW_DEBUG
	// Editor only: in play the lines would print onto the projected picture.
	if (bShowGuides && GetWorld() != nullptr && !GetWorld()->IsGameWorld())
	{
		DrawGuides();
	}
#endif
}

void AVibH2OTulleRig::ResolveConnections()
{
	if (Stage == nullptr && GetWorld() != nullptr)
	{
		for (TActorIterator<AVibH2OStageActor> It(GetWorld()); It; ++It)
		{
			Stage = *It;
			break;
		}
	}
}

bool AVibH2OTulleRig::GetEyeView(float DeltaSeconds, FMinimalViewInfo& OutView) const
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}
	auto FromActor = [DeltaSeconds, &OutView](const AActor* Actor)
	{
		if (Actor == nullptr)
		{
			return false;
		}
		UCameraComponent* Component = Actor->FindComponentByClass<UCameraComponent>();
		if (Component == nullptr)
		{
			return false;
		}
		Component->GetCameraView(DeltaSeconds, OutView);
		return true;
	};

	if (FromActor(Camera))
	{
		return true;
	}
	if (World->IsGameWorld())
	{
		const APlayerController* Player = World->GetFirstPlayerController();
		if (Player != nullptr && Player->PlayerCameraManager != nullptr)
		{
			// The view target's own camera is this frame's; the manager's cache may be the last one's.
			if (FromActor(Player->PlayerCameraManager->GetViewTarget()))
			{
				return true;
			}
			OutView = Player->PlayerCameraManager->GetCameraCacheView();
			return true;
		}
	}
	for (TActorIterator<ACameraRig_Rail> It(World); It; ++It)
	{
		TArray<AActor*> Attached;
		It->GetAttachedActors(Attached, true, true);
		for (const AActor* Actor : Attached)
		{
			if (FromActor(Actor))
			{
				return true;
			}
		}
	}
	return false;
}

void AVibH2OTulleRig::GatherBubbles(TArray<AActor*>& OutActors) const
{
	if (Stage == nullptr)
	{
		return;
	}
	for (AVibH2OBubbleActor* Bubble : Stage->GetBubbles())
	{
		OutActors.Add(Bubble);
	}
	for (AVibH2OBubbleActor* Daughter : Stage->GetDaughters())
	{
		OutActors.Add(Daughter);
	}
}

void AVibH2OTulleRig::UpdateLayout(float DeltaSeconds)
{
	ResolveConnections();

	const int32 Count = Tulles.Num();
	Views.SetNum(Count);
	ViewValid.Init(false, Count);
	BubblesPerTulle.Init(0, Count);
	BubblesOnFace = 0;
	bFrameValid = false;

	if (!GetEyeView(DeltaSeconds, EyeView))
	{
		VirtualCmPerMetre = 0.0f;
		ReferenceSeatDistance = 0.0f;
		Assignment.Reset();
		return;
	}

	const FVector Forward = EyeView.Rotation.Vector();
	double Distance = FacePlaneDistance;
	if (Distance <= 0.0 && Stage != nullptr)
	{
		Distance = FVector::DotProduct(Stage->GetActorLocation() - EyeView.Location, Forward);
	}
	if (Distance <= 1.0)
	{
		Distance = VibH2OTulleRigInternal::FallbackDistance;
	}

	bFrameValid = ComputeFaceFrame(EyeView.Location, EyeView.Rotation, EyeView.FOV, Distance, FaceWidth, FaceHeight, Frame);
	if (!bFrameValid)
	{
		VirtualCmPerMetre = 0.0f;
		ReferenceSeatDistance = 0.0f;
		Assignment.Reset();
		return;
	}
	VirtualCmPerMetre = static_cast<float>(Frame.Scale);
	ReferenceSeatDistance = static_cast<float>(0.5 * FaceWidth / FMath::Tan(FMath::DegreesToRadians(0.5 * EyeView.FOV)));

	for (int32 Index = 0; Index < Count; ++Index)
	{
		FVector BottomLeft;
		FVector BottomRight;
		FVector TopLeft;
		GetTulleCorners(Frame, Tulles[Index], BottomLeft, BottomRight, TopLeft);
		ViewValid[Index] = ComputeOffAxisView(EyeView.Location, BottomLeft, BottomRight, TopLeft, Views[Index]);
	}

	TArray<AActor*> Actors;
	GatherBubbles(Actors);
	TMap<TWeakObjectPtr<AActor>, int32> Next;
	for (AActor* Actor : Actors)
	{
		if (Actor == nullptr)
		{
			continue;
		}
		const int32* Current = Assignment.Find(Actor);
		const int32 Tulle = PickTulle(Tulles, Views, ViewValid, Frame, Actor->GetActorLocation(), Current != nullptr ? *Current : INDEX_NONE, Hysteresis);
		if (Tulle == INDEX_NONE)
		{
			++BubblesOnFace;
		}
		else
		{
			Next.Add(Actor, Tulle);
			++BubblesPerTulle[Tulle];
		}
	}
	Assignment = MoveTemp(Next);
}

int32 AVibH2OTulleRig::GetTulleOfActor(const AActor* Actor) const
{
	const int32* Found = Assignment.Find(Actor);
	return Found != nullptr ? *Found : INDEX_NONE;
}

UTextureRenderTarget2D* AVibH2OTulleRig::GetTulleTarget(int32 TulleIndex) const
{
	return Targets.IsValidIndex(TulleIndex) ? Targets[TulleIndex].Get() : nullptr;
}

// ------------------------------------------------------------------- Outputs

bool AVibH2OTulleRig::ShouldRender() const
{
	const UWorld* World = GetWorld();
	return bActive && bFrameValid && World != nullptr && World->IsGameWorld();
}

uint32 AVibH2OTulleRig::ComputeOutputsSignature() const
{
	uint32 Hash = GetTypeHash(Tulles.Num());
	for (const FVibH2OTulle& Tulle : Tulles)
	{
		Hash = HashCombine(Hash, GetTypeHash(Tulle.bEnabled));
		Hash = HashCombine(Hash, GetTypeHash(Tulle.Resolution));
		Hash = HashCombine(Hash, GetTypeHash(Tulle.SpoutName));
	}
	return HashCombine(Hash, GetTypeHash(bBlackBackground));
}

void AVibH2OTulleRig::BuildOutputs()
{
	using namespace VibH2OTulleRigInternal;

	UClass* SenderClass = FindObject<UClass>(nullptr, TEXT("/Script/SpoutUE.SpoutSenderComponent"));
	const int32 Count = Tulles.Num();
	Captures.Init(nullptr, Count);
	Targets.Init(nullptr, Count);
	Senders.Init(nullptr, Count);

	int32 Built = 0;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FVibH2OTulle& Tulle = Tulles[Index];
		if (!Tulle.bEnabled || Tulle.Resolution.X <= 0 || Tulle.Resolution.Y <= 0)
		{
			continue;
		}

		// Linear half floats, as the Spout sender expects from a capture: it
		// encodes them for the receiver itself.
		UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(this);
		Target->RenderTargetFormat = RTF_RGBA16f;
		Target->ClearColor = FLinearColor::Black;
		Target->TargetGamma = 1.0f;
		Target->InitAutoFormat(Tulle.Resolution.X, Tulle.Resolution.Y);
		Target->UpdateResourceImmediate(true);
		Targets[Index] = Target;

		USceneCaptureComponent2D* Capture = NewObject<USceneCaptureComponent2D>(this);
		Capture->SetUsingAbsoluteLocation(true);
		Capture->SetUsingAbsoluteRotation(true);
		Capture->SetUsingAbsoluteScale(true);
		Capture->TextureTarget = Target;
		Capture->CaptureSource = ESceneCaptureSource::SCS_FinalToneCurveHDR;
		Capture->bCaptureEveryFrame = true;
		Capture->bCaptureOnMovement = false;
		Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		Capture->bUseCustomProjectionMatrix = true;
		if (bBlackBackground)
		{
			Capture->ShowFlags.SetFog(false);
			Capture->ShowFlags.SetVolumetricFog(false);
			Capture->ShowFlags.SetAtmosphere(false);
		}
		Capture->SetupAttachment(GetRootComponent());
		Capture->RegisterComponent();
		Captures[Index] = Capture;

		if (SenderClass != nullptr)
		{
			UActorComponent* Sender = NewObject<UActorComponent>(this, SenderClass);
			SetString(Sender, TEXT("SenderName"), Tulle.SpoutName);
			SetEnum(Sender, TEXT("SourceMode"), TEXT("RenderTarget"));
			SetObject(Sender, TEXT("SourceRenderTarget"), Target);
			Sender->RegisterComponent();
			Senders[Index] = Sender;
		}
		++Built;
	}

	bOutputsBuilt = true;
	OutputsSignature = ComputeOutputsSignature();
	UE_LOG(LogVibH2O, Log, TEXT("VibH2O Tulles : %d sortie(s) en profondeur%s."), Built,
		SenderClass != nullptr ? TEXT(", chacune avec son emetteur Spout") : TEXT(", sans Spout (plugin SpoutUE absent)"));
}

void AVibH2OTulleRig::ReleaseOutputs()
{
	for (UActorComponent* Sender : Senders)
	{
		if (Sender != nullptr)
		{
			Sender->DestroyComponent();
		}
	}
	for (USceneCaptureComponent2D* Capture : Captures)
	{
		if (Capture != nullptr)
		{
			Capture->DestroyComponent();
		}
	}
	Senders.Reset();
	Captures.Reset();
	Targets.Reset();
	bOutputsBuilt = false;
}

void AVibH2OTulleRig::ApplyOutputs()
{
	for (int32 Index = 0; Index < Captures.Num(); ++Index)
	{
		USceneCaptureComponent2D* Capture = Captures[Index];
		if (Capture == nullptr)
		{
			continue;
		}
		Capture->ShowOnlyActors.Reset();
		if (!ViewValid.IsValidIndex(Index) || !ViewValid[Index])
		{
			// An empty list renders nothing: the tulle stays black.
			continue;
		}

		const FVibH2OOffAxisView& View = Views[Index];
		const FIntPoint Resolution = Tulles[Index].Resolution;
		Capture->SetWorldLocationAndRotation(View.Eye, FRotationMatrix::MakeFromXZ(View.Forward, View.Up).Rotator());
		Capture->CustomProjectionMatrix = View.MakeProjectionMatrix(GNearClippingPlane);
		Capture->FOVAngle = View.GetEnclosingFOV(static_cast<double>(Resolution.X) / Resolution.Y);
		// Same lens and grade as the main camera, so a bubble keeps its look when it changes tulle.
		Capture->PostProcessSettings = EyeView.PostProcessSettings;
		Capture->PostProcessBlendWeight = EyeView.PostProcessBlendWeight;

		for (const TPair<TWeakObjectPtr<AActor>, int32>& Pair : Assignment)
		{
			if (Pair.Value == Index)
			{
				if (AActor* Actor = Pair.Key.Get())
				{
					Capture->ShowOnlyActors.Add(Actor);
				}
			}
		}
	}
}

void AVibH2OTulleRig::ApplyHiddenActors(bool bRelease)
{
	UWorld* World = GetWorld();
	APlayerController* Player = World != nullptr ? World->GetFirstPlayerController() : nullptr;
	if (Player != nullptr)
	{
		for (const TWeakObjectPtr<AActor>& Hidden : HiddenFromMain)
		{
			if (AActor* Actor = Hidden.Get())
			{
				Player->HiddenActors.Remove(Actor);
			}
		}
	}
	HiddenFromMain.Reset();
	if (bRelease || Player == nullptr)
	{
		return;
	}
	for (const TPair<TWeakObjectPtr<AActor>, int32>& Pair : Assignment)
	{
		if (AActor* Actor = Pair.Key.Get())
		{
			Player->HiddenActors.AddUnique(Actor);
			HiddenFromMain.Add(Actor);
		}
	}
}

void AVibH2OTulleRig::DrawGuides() const
{
#if ENABLE_DRAW_DEBUG
	UWorld* World = GetWorld();
	if (World == nullptr || !bFrameValid)
	{
		return;
	}
	// A cross through each tulle tells it from a bare rectangle at any distance.
	auto DrawTulle = [World](const FVector& BottomLeft, const FVector& BottomRight, const FVector& TopLeft, const FColor& Color)
	{
		const FVector TopRight = BottomRight + (TopLeft - BottomLeft);
		DrawDebugLine(World, BottomLeft, BottomRight, Color);
		DrawDebugLine(World, BottomRight, TopRight, Color);
		DrawDebugLine(World, TopRight, TopLeft, Color);
		DrawDebugLine(World, TopLeft, BottomLeft, Color);
		DrawDebugLine(World, BottomLeft, TopRight, Color);
		DrawDebugLine(World, BottomRight, TopLeft, Color);
	};

	FVibH2OTulle Face;
	Face.Width = FaceWidth;
	Face.Height = FaceHeight;
	Face.Depth = 0.0f;
	FVector BottomLeft;
	FVector BottomRight;
	FVector TopLeft;
	GetTulleCorners(Frame, Face, BottomLeft, BottomRight, TopLeft);
	DrawTulle(BottomLeft, BottomRight, TopLeft, FColor(255, 190, 0));

	static const FColor Colors[] = { FColor(5, 150, 105), FColor(219, 39, 119), FColor(59, 130, 246), FColor(160, 90, 255) };
	for (int32 Index = 0; Index < Tulles.Num(); ++Index)
	{
		const FVibH2OTulle& Tulle = Tulles[Index];
		GetTulleCorners(Frame, Tulle, BottomLeft, BottomRight, TopLeft);
		const FColor Color = Tulle.bEnabled ? Colors[Index % UE_ARRAY_COUNT(Colors)] : FColor(110, 110, 110);
		DrawTulle(BottomLeft, BottomRight, TopLeft, Color);
	}
#endif
}

// ------------------------------------------------------------------ Geometry

bool AVibH2OTulleRig::ComputeFaceFrame(const FVector& Eye, const FRotator& ViewRotation, float HorizontalFOV, double Distance,
	float InFaceWidth, float InFaceHeight, FVibH2OTulleFrame& OutFrame)
{
	if (HorizontalFOV <= 0.0f || HorizontalFOV >= 179.0f || Distance <= 0.0 || InFaceWidth <= 0.0f || InFaceHeight <= 0.0f)
	{
		return false;
	}
	const FRotationMatrix Axes(ViewRotation);
	const FVector Forward = Axes.GetScaledAxis(EAxis::X);
	const FVector Right = Axes.GetScaledAxis(EAxis::Y);
	const FVector Up = Axes.GetScaledAxis(EAxis::Z);

	const double HalfWidth = Distance * FMath::Tan(FMath::DegreesToRadians(0.5 * HorizontalFOV));
	OutFrame.Scale = 2.0 * HalfWidth / InFaceWidth;
	OutFrame.Origin = Eye + Forward * Distance - Up * (0.5 * InFaceHeight * OutFrame.Scale);
	OutFrame.Right = Right;
	OutFrame.Up = Up;
	OutFrame.Back = Forward;
	return true;
}

void AVibH2OTulleRig::GetTulleCorners(const FVibH2OTulleFrame& InFrame, const FVibH2OTulle& Tulle,
	FVector& OutBottomLeft, FVector& OutBottomRight, FVector& OutTopLeft)
{
	const double Scale = InFrame.Scale;
	const FVector Anchor = InFrame.Origin
		+ InFrame.Right * (Tulle.Lateral * Scale)
		+ InFrame.Back * (Tulle.Depth * Scale)
		+ InFrame.Up * (Tulle.Elevation * Scale);
	const FVector HalfWidth = InFrame.Right * (0.5 * Tulle.Width * Scale);

	if (Tulle.Facing == EVibH2OTulleFacing::Floor)
	{
		// Seen from the audience the near edge is the bottom one, the far edge the top.
		const FVector HalfDepth = InFrame.Back * (0.5 * Tulle.Height * Scale);
		OutBottomLeft = Anchor - HalfWidth - HalfDepth;
		OutBottomRight = Anchor + HalfWidth - HalfDepth;
		OutTopLeft = Anchor - HalfWidth + HalfDepth;
	}
	else
	{
		OutBottomLeft = Anchor - HalfWidth;
		OutBottomRight = Anchor + HalfWidth;
		OutTopLeft = OutBottomLeft + InFrame.Up * (Tulle.Height * Scale);
	}
}

bool AVibH2OTulleRig::ComputeOffAxisView(const FVector& Eye, const FVector& BottomLeft, const FVector& BottomRight, const FVector& TopLeft,
	FVibH2OOffAxisView& OutView)
{
	FVector Right = BottomRight - BottomLeft;
	FVector Up = TopLeft - BottomLeft;
	if (!Right.Normalize() || !Up.Normalize())
	{
		return false;
	}
	Up = (Up - Right * FVector::DotProduct(Up, Right)).GetSafeNormal();
	if (Up.IsNearlyZero())
	{
		return false;
	}
	// Right, up, forward: the eye looks into the tulle from its audience side.
	const FVector Forward = FVector::CrossProduct(Right, Up);
	const double Distance = FVector::DotProduct(BottomLeft - Eye, Forward);
	if (Distance <= UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}

	OutView.Eye = Eye;
	OutView.Forward = Forward;
	OutView.Right = Right;
	OutView.Up = Up;
	OutView.Distance = Distance;
	OutView.Left = FVector::DotProduct(BottomLeft - Eye, Right);
	OutView.RightEdge = FVector::DotProduct(BottomRight - Eye, Right);
	OutView.Bottom = FVector::DotProduct(BottomLeft - Eye, Up);
	OutView.Top = FVector::DotProduct(TopLeft - Eye, Up);
	return OutView.RightEdge > OutView.Left && OutView.Top > OutView.Bottom;
}

double AVibH2OTulleRig::GetRealDepth(const FVibH2OTulleFrame& InFrame, const FVector& Point)
{
	return InFrame.Scale > 0.0 ? FVector::DotProduct(Point - InFrame.Origin, InFrame.Back) / InFrame.Scale : 0.0;
}

int32 AVibH2OTulleRig::PickTulle(TConstArrayView<FVibH2OTulle> InTulles, TConstArrayView<FVibH2OOffAxisView> InViews, TConstArrayView<bool> bViewValid,
	const FVibH2OTulleFrame& InFrame, const FVector& Point, int32 CurrentTulle, float InHysteresis)
{
	const int32 Count = FMath::Min3(InTulles.Num(), InViews.Num(), bViewValid.Num());
	const double Depth = GetRealDepth(InFrame, Point);

	auto Holds = [&](int32 Index, double Margin)
	{
		const FVibH2OTulle& Tulle = InTulles[Index];
		return Tulle.bEnabled && bViewValid[Index]
			&& Depth >= Tulle.DepthFrom - Margin && Depth <= Tulle.DepthTo + Margin
			&& InViews[Index].Contains(Point);
	};

	if (CurrentTulle >= 0 && CurrentTulle < Count && Holds(CurrentTulle, InHysteresis))
	{
		return CurrentTulle;
	}
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (Holds(Index, 0.0))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}
