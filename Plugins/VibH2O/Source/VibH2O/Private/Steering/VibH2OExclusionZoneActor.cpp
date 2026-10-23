#include "Steering/VibH2OExclusionZoneActor.h"

#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Steering/VibH2OSteeringNaming.h"

namespace
{
	constexpr uint64 ZonePruneAfterFrames = 600;

	/** Closer than this to a perspective camera, a bubble is left alone. */
	constexpr float MinPerspectiveDepth = 1.0f;
}

AVibH2OExclusionZoneActor::AVibH2OExclusionZoneActor()
{
	PrimaryActorTick.bCanEverTick = false;

	ZoneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ZoneRoot"));
	SetRootComponent(ZoneRoot);

	Volume = CreateDefaultSubobject<UBoxComponent>(TEXT("Volume"));
	Volume->SetupAttachment(ZoneRoot);
	// A performer and her apparatus, roughly: 1.8 m wide, 2.6 m tall.
	Volume->InitBoxExtent(FVector(60.0f, 90.0f, 130.0f));
	Volume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Volume->SetGenerateOverlapEvents(false);
	Volume->SetCanEverAffectNavigation(false);
	Volume->bHiddenInGame = true;
	Volume->ShapeColor = FColor(220, 60, 240);

	Arrow = CreateDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	Arrow->SetupAttachment(Volume);
	Arrow->ArrowSize = 2.0f;
	Arrow->ArrowColor = FColor(220, 60, 240);
	Arrow->bHiddenInGame = true;
}

void AVibH2OExclusionZoneActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

#if WITH_EDITOR
	RefreshAutoControlName();
#endif

	const FColor Color = DisplayColor.ToFColor(true);
	if (Volume != nullptr)
	{
		Volume->ShapeColor = Color;
	}
	if (Arrow != nullptr)
	{
		Arrow->ArrowColor = Color;
	}

	// Outside play, the drag shows at once, unsmoothed: that is how the
	// range is checked while the zone is being placed.
	const UWorld* World = GetWorld();
	if (World == nullptr || !World->IsGameWorld())
	{
		const FVector2D Target = GetDragTargetCm();
		DragSpringX.Reset(Target.X);
		DragSpringY.Reset(Target.Y);
		ApplyDragOffset(Target);
	}
}

#if WITH_EDITOR
void AVibH2OExclusionZoneActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	// Renaming the actor in the outliner comes through here too.
	RefreshAutoControlName();
}

void AVibH2OExclusionZoneActor::RefreshAutoControlName()
{
	// Named after its label, so it shows up on the iPad with no field to fill
	// in - for as long as nobody typed a name of their own. A typed name is
	// never overwritten; clearing it hands back to the label. Stored, not
	// derived on the fly: labels do not exist in a packaged game.
	if (ControlName.IsNone() || ControlName == LastAutoControlName)
	{
		ControlName = VibH2OSteeringNaming::SanitizeControlName(GetActorLabel());
		LastAutoControlName = ControlName;
	}
}
#endif

FName AVibH2OExclusionZoneActor::GetEffectiveControlName() const
{
	return ControlName.IsNone() ? GetFName() : ControlName;
}

FString AVibH2OExclusionZoneActor::GetEffectiveLabel() const
{
	return DisplayLabel.IsEmpty() ? GetEffectiveControlName().ToString() : DisplayLabel;
}

void AVibH2OExclusionZoneActor::SetActive(bool bActive)
{
	Activation = bActive ? 1.0f : 0.0f;
}

void AVibH2OExclusionZoneActor::Toggle()
{
	SetActive(!IsActive());
}

void AVibH2OExclusionZoneActor::SetDragPosition(float X, float Y)
{
	DragX = FMath::Clamp(X, -1.0f, 1.0f);
	DragY = FMath::Clamp(Y, -1.0f, 1.0f);
}

FVector2D AVibH2OExclusionZoneActor::GetHalfSize() const
{
	if (Volume == nullptr)
	{
		return FVector2D(100.0, 100.0);
	}
	const FVector Extent = Volume->GetScaledBoxExtent();
	return FVector2D(Extent.Y, Extent.Z);
}

FVector2D AVibH2OExclusionZoneActor::GetDragTargetCm() const
{
	const double Mirror = bMirrorDrag ? -1.0 : 1.0;
	return FVector2D(
		FMath::Clamp(DragX, -1.0f, 1.0f) * DragRange.X * Mirror,
		FMath::Clamp(DragY, -1.0f, 1.0f) * DragRange.Y);
}

void AVibH2OExclusionZoneActor::ApplyDragOffset(const FVector2D& OffsetCm)
{
	if (Volume == nullptr)
	{
		return;
	}
	// The range is meant in centimetres of the scene, whatever the actor's
	// scale: undo that scale on the way down.
	const FVector RootScale = GetActorScale3D();
	const FVector Wanted(
		0.0,
		OffsetCm.X / FMath::Max(FMath::Abs(RootScale.Y), KINDA_SMALL_NUMBER),
		OffsetCm.Y / FMath::Max(FMath::Abs(RootScale.Z), KINDA_SMALL_NUMBER));

	if (!Volume->GetRelativeLocation().Equals(Wanted, 0.01))
	{
		Volume->SetRelativeLocation(Wanted);
	}
}

// --------------------------------------------------------------- Stage side

void AVibH2OExclusionZoneActor::BeginSteeringFrame(const FTransform& StageRootTransform, float DeltaSeconds)
{
	FrameNumber = GFrameCounter;
	FrameDeltaSeconds = DeltaSeconds;

	if (LastSteppedFrame != FrameNumber)
	{
		LastSteppedFrame = FrameNumber;

		Weight.Step(FMath::Clamp(Activation, 0.0f, 1.0f), FadeSeconds, DeltaSeconds);

		// The zone trails the finger slightly: a drag sent at 25 Hz over Wi-Fi
		// arrives in steps, and a zone stepping round a performer would drag
		// its bubbles along in steps too.
		const FVector2D Target = GetDragTargetCm();
		if (!bDragInitialised)
		{
			DragSpringX.Reset(Target.X);
			DragSpringY.Reset(Target.Y);
			bDragInitialised = true;
		}
		else
		{
			DragSpringX.Step(Target.X, DragSmoothingSeconds, DeltaSeconds);
			DragSpringY.Step(Target.Y, DragSmoothingSeconds, DeltaSeconds);
		}
		ApplyDragOffset(FVector2D(DragSpringX.Value, DragSpringY.Value));

		if (Weight.Value <= 0.0f && Activation <= 0.0f)
		{
			// Fully off: the next activation starts from where the bubbles
			// really are, not from sides remembered from the last one.
			BubbleStates.Reset();
		}
	}

	FrameParams.Shape = (Shape == EVibH2OExclusionShape::Rectangle) ? EVibH2OZoneShapeKind::Rectangle : EVibH2OZoneShapeKind::Ellipse;
	FrameParams.Margin = Margin;
	FrameParams.GlideRadiansPerSecond = FMath::DegreesToRadians(GlideDegreesPerSecond);
	FrameParams.Weight = Weight.Value;

	if (Volume == nullptr)
	{
		FrameParams.Weight = 0.0f;
		return;
	}

	// Everything in the stage's local space, like the bubbles: the stage can
	// be moved, and so can the zone, without either noticing.
	const FTransform VolumeToStage = Volume->GetComponentTransform().GetRelativeTransform(StageRootTransform);
	const FVector Center = VolumeToStage.GetLocation();
	const FQuat Rotation = VolumeToStage.GetRotation();
	const FVector AxisX = Rotation.GetAxisX();
	const FVector AxisY = Rotation.GetAxisY();
	const FVector AxisZ = Rotation.GetAxisZ();

	const FVector Extent = Volume->GetUnscaledBoxExtent() * VolumeToStage.GetScale3D().GetAbs();
	FrameParams.HalfSize = FVector2D(Extent.Y, Extent.Z);
	FrameHalfDepth = static_cast<float>(Extent.X);

	bPerspective = false;
	if (IsValid(ViewCamera))
	{
		const FVector Camera = StageRootTransform.InverseTransformPosition(ViewCamera->GetActorLocation());
		const FVector ToZone = Center - Camera;
		const double Distance = ToZone.Size();
		if (Distance > MinPerspectiveDepth)
		{
			// A cone from the camera: the zone's own up and right, straightened
			// against the line of sight, so it reads the right way up on screen.
			const FVector Forward = ToZone / Distance;
			FVector Up = (AxisZ - Forward * (AxisZ | Forward)).GetSafeNormal();
			if (Up.IsNearlyZero())
			{
				Up = (AxisX - Forward * (AxisX | Forward)).GetSafeNormal();
			}
			FVector Right = (AxisY - Forward * (AxisY | Forward) - Up * (AxisY | Up)).GetSafeNormal();
			if (Right.IsNearlyZero())
			{
				Right = (Up ^ Forward).GetSafeNormal();
			}

			bPerspective = true;
			FrameOrigin = Camera;
			FrameForward = Forward;
			FrameRight = Right;
			FrameUp = Up;
			FrameReferenceDepth = static_cast<float>(Distance);
		}
	}

	if (!bPerspective)
	{
		FrameOrigin = Center;
		FrameForward = AxisX;
		FrameRight = AxisY;
		FrameUp = AxisZ;
		FrameReferenceDepth = 1.0f;
	}
}

FVector AVibH2OExclusionZoneActor::ApplyToBubble(int32 SeatIndex, int32 Seed, const FVector& LocalLocation)
{
	if (Weight.Value <= 0.0f && Activation <= 0.0f)
	{
		return LocalLocation;
	}

	FBubbleEntry& Entry = BubbleStates.FindOrAdd(SeatIndex);
	// Asked twice in a frame - a second stage - the side does not turn twice.
	const float DeltaSeconds = (Entry.LastFrame == FrameNumber) ? 0.0f : FrameDeltaSeconds;
	Entry.LastFrame = FrameNumber;

	const FVector Offset = LocalLocation - FrameOrigin;
	const float Depth = static_cast<float>(Offset | FrameForward);

	float Scale = 1.0f;
	float DepthFromZone = Depth;
	if (bPerspective)
	{
		if (Depth < MinPerspectiveDepth)
		{
			// Behind the camera: it cannot cover anyone in the image.
			return LocalLocation;
		}
		// Projected onto the zone's own plane: what counts is where the bubble
		// lands in the image, not how far it is.
		Scale = FrameReferenceDepth / Depth;
		DepthFromZone = Depth - FrameReferenceDepth;
	}

	FVibH2OZonePushParams Params = FrameParams;
	if (bLimitDepth)
	{
		Params.Weight *= 1.0f - FMath::SmoothStep(FrameHalfDepth, FrameHalfDepth + FMath::Max(Margin, 1.0f), FMath::Abs(DepthFromZone));
	}

	const FVector2D Projected((Offset | FrameRight) * Scale, (Offset | FrameUp) * Scale);
	const FVector2D Pushed = FVibH2OSteeringMath::PushOutOfZone(Projected, Params, Entry.State, Seed, DeltaSeconds);
	const FVector2D Delta = (Pushed - Projected) / Scale;

	if (Delta.IsNearlyZero(1.0e-4))
	{
		return LocalLocation;
	}
	// Sideways only: depth is left alone, so a bubble keeps its place in the
	// school and only steps out of the performer's way.
	return LocalLocation + FrameRight * Delta.X + FrameUp * Delta.Y;
}

void AVibH2OExclusionZoneActor::EndSteeringFrame()
{
	for (auto It = BubbleStates.CreateIterator(); It; ++It)
	{
		if (FrameNumber > It.Value().LastFrame + ZonePruneAfterFrames)
		{
			It.RemoveCurrent();
		}
	}
}
