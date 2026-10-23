#include "Steering/VibH2OAttractorActor.h"

#include "Components/SphereComponent.h"
#include "Steering/VibH2OSteeringNaming.h"

namespace
{
	/** Per-bubble state not touched for this many frames is dropped. */
	constexpr uint64 AttractorPruneAfterFrames = 600;
}

AVibH2OAttractorActor::AVibH2OAttractorActor()
{
	// The stage drives everything: an attractor alone has nothing to do.
	PrimaryActorTick.bCanEverTick = false;

	Volume = CreateDefaultSubobject<USphereComponent>(TEXT("Volume"));
	Volume->InitSphereRadius(150.0f);
	Volume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Volume->SetGenerateOverlapEvents(false);
	Volume->SetCanEverAffectNavigation(false);
	Volume->bHiddenInGame = true;
	Volume->ShapeColor = FColor(255, 140, 40);
	SetRootComponent(Volume);
}

void AVibH2OAttractorActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

#if WITH_EDITOR
	RefreshAutoControlName();
#endif

	if (Volume != nullptr)
	{
		Volume->ShapeColor = DisplayColor.ToFColor(true);
	}
}

FName AVibH2OAttractorActor::GetEffectiveControlName() const
{
	return ControlName.IsNone() ? GetFName() : ControlName;
}

FString AVibH2OAttractorActor::GetEffectiveLabel() const
{
	return DisplayLabel.IsEmpty() ? GetEffectiveControlName().ToString() : DisplayLabel;
}

#if WITH_EDITOR
void AVibH2OAttractorActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	// Renaming the actor in the outliner comes through here too.
	RefreshAutoControlName();
}

void AVibH2OAttractorActor::RefreshAutoControlName()
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

void AVibH2OAttractorActor::SetActive(bool bActive)
{
	Activation = bActive ? 1.0f : 0.0f;
}

void AVibH2OAttractorActor::Toggle()
{
	SetActive(!IsActive());
}

// --------------------------------------------------------------- Stage side

void AVibH2OAttractorActor::BeginSteeringFrame(const FTransform& StageRootTransform, float FadeScale, float DeltaSeconds, float TimeSeconds)
{
	// Relative to the stage root, every frame: the stage may have been moved,
	// and so may the attractor - the installation stays movable either way.
	VolumeToStage = Volume != nullptr
		? Volume->GetComponentTransform().GetRelativeTransform(StageRootTransform)
		: GetActorTransform().GetRelativeTransform(StageRootTransform);

	FrameFadeScale = FMath::Max(FadeScale, 0.0f);
	FrameDeltaSeconds = DeltaSeconds;
	FrameTimeSeconds = TimeSeconds;
	FrameNumber = GFrameCounter;
	FrameWeightSum = 0.0f;
	FrameMemberCount = 0;
}

float AVibH2OAttractorActor::StepBubbleWeight(int32 SeatIndex, int32 Seed)
{
	const bool bMember = FVibH2OSteeringMath::IsInShare(FVibH2OSteeringMath::SelectionHash(Seed), ShareStart, Share);
	const float Target = bMember ? FMath::Clamp(Activation, 0.0f, 1.0f) : 0.0f;

	FBubbleWeight* Entry = BubbleWeights.Find(SeatIndex);
	if (Entry == nullptr)
	{
		if (Target <= 0.0f)
		{
			// Nothing pulls this bubble and nothing has: no state to keep.
			return 0.0f;
		}
		Entry = &BubbleWeights.Add(SeatIndex);
	}

	// A second stage in the level would ask again in the same frame: answer
	// without stepping twice.
	if (Entry->LastFrame != FrameNumber)
	{
		Entry->LastFrame = FrameNumber;

		// Arriving and returning have their own durations; each bubble's is
		// spread round the nominal one, so the group flows instead of moving
		// as one rigid block.
		const bool bArriving = Target > Entry->Spring.Value;
		const float Nominal = (bArriving ? FadeInSeconds : FadeOutSeconds) * FrameFadeScale;
		Entry->Spring.Step(Target, FVibH2OSteeringMath::StaggeredDuration(Nominal, Seed, Stagger), FrameDeltaSeconds);
	}

	if (bMember)
	{
		FrameWeightSum += Entry->Spring.Value;
		++FrameMemberCount;
	}
	return Entry->Spring.Value;
}

FVector AVibH2OAttractorActor::GetCloudTarget(int32 Seed) const
{
	const float Radius = Volume != nullptr ? Volume->GetUnscaledSphereRadius() : 100.0f;
	const FVector UnitPoint = FVibH2OSteeringMath::CloudPoint(Seed, FrameTimeSeconds, CloudBreathAmount, CloudBreathSpeed);
	// The transform carries the actor's scale: a sphere scaled flat becomes a
	// flat cloud, which is the whole point of letting the artist scale it.
	return VolumeToStage.TransformPosition(UnitPoint * Radius);
}

void AVibH2OAttractorActor::EndSteeringFrame()
{
	CurrentWeight = FrameMemberCount > 0 ? FrameWeightSum / FrameMemberCount : 0.0f;

	// Bubbles that left the room, or that came home and are at rest, go.
	for (auto It = BubbleWeights.CreateIterator(); It; ++It)
	{
		const FBubbleWeight& Entry = It.Value();
		const bool bForgotten = FrameNumber > Entry.LastFrame + AttractorPruneAfterFrames;
		const bool bHome = Entry.Spring.Value == 0.0f && Entry.Spring.Velocity == 0.0f;
		if (bForgotten || bHome)
		{
			It.RemoveCurrent();
		}
	}
}
