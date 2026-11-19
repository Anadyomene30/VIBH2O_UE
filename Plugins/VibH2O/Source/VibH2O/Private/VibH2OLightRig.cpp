#include "VibH2OLightRig.h"

#include "Components/SpotLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "VibH2OStageActor.h"

namespace VibH2OLightRigInternal
{
	static USpotLightComponent* MakeSpot(AActor* Owner, USceneComponent* Root, const TCHAR* Name)
	{
		USpotLightComponent* Spot = Owner->CreateDefaultSubobject<USpotLightComponent>(Name);
		Spot->SetupAttachment(Root);
		// The rig moves its lights every time the room changes: anything but
		// movable would be baked into the wrong place.
		Spot->SetMobility(EComponentMobility::Movable);
		// Candelas, so the inverse-square compensation below is plain physics.
		Spot->SetIntensityUnits(ELightUnits::Candelas);
		// A soft source: a hard point light leaves a pinprick highlight that
		// reads as plastic on a wet surface.
		Spot->SetSourceRadius(40.0f);
		Spot->SetSoftSourceRadius(120.0f);
		return Spot;
	}
}

AVibH2OLightRig::AVibH2OLightRig()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	// After the stage has placed its bubbles, so the lights aim at this frame's
	// room and not the last one.
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("RigRoot")));

	KeyLight = VibH2OLightRigInternal::MakeSpot(this, GetRootComponent(), TEXT("KeyLight"));
	BackLight = VibH2OLightRigInternal::MakeSpot(this, GetRootComponent(), TEXT("BackLight"));
	FillLight = VibH2OLightRigInternal::MakeSpot(this, GetRootComponent(), TEXT("FillLight"));

	// Defaults written for deep water: a slightly warm key from the audience's
	// side, a cold strong back light opposite, a dim blue fill low on the
	// other flank. Warm against cold is what separates the bubbles from the
	// water; the back light is what makes them read as soft bodies.
	Key.Azimuth = -30.0f;
	Key.Elevation = 50.0f;
	Key.Illuminance = 2.2f;
	Key.Color = FLinearColor(1.0f, 0.93f, 0.82f);
	Key.ConeAngle = 30.0f;
	Key.VolumetricScattering = 0.6f;
	Key.bCastShadows = true;

	Back.Azimuth = 165.0f;
	Back.Elevation = 35.0f;
	Back.Illuminance = 4.0f;
	Back.Color = FLinearColor(0.45f, 0.85f, 1.0f);
	Back.ConeAngle = 34.0f;
	Back.VolumetricScattering = 2.5f;
	Back.bCastShadows = false;

	Fill.Azimuth = 70.0f;
	Fill.Elevation = 12.0f;
	Fill.Illuminance = 0.6f;
	Fill.Color = FLinearColor(0.25f, 0.45f, 1.0f);
	Fill.ConeAngle = 45.0f;
	Fill.VolumetricScattering = 0.2f;
	Fill.bCastShadows = false;
}

bool AVibH2OLightRig::ShouldTickIfViewportsOnly() const
{
	// Lighting is dialled in the editor: it must follow the room there too.
	return true;
}

void AVibH2OLightRig::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// A settings change in the Details panel shows at once, not at the next
	// tick - and the first placement must not glide in from the origin.
	UpdateRig(0.0f, true);
}

void AVibH2OLightRig::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateRig(DeltaSeconds, false);
}

void AVibH2OLightRig::SnapToRoom()
{
	UpdateRig(0.0f, true);
}

AVibH2OStageActor* AVibH2OLightRig::ResolveStage()
{
	if (IsValid(Stage))
	{
		return Stage;
	}
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}
	// Not stored back into Stage: that would dirty the level from a tick, and
	// leaving it empty keeps "the stage of this level" meaning just that.
	for (TActorIterator<AVibH2OStageActor> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void AVibH2OLightRig::UpdateRig(float DeltaSeconds, bool bSnap)
{
	AVibH2OStageActor* TargetStage = ResolveStage();
	if (TargetStage == nullptr)
	{
		return;
	}

	FBox Bounds = (bFollowFocus && !TargetStage->ActiveFocusGroup.IsNone())
		? TargetStage->GetFocusBounds()
		: TargetStage->GetRoomBounds();

	// No bubble yet: light the area the room is about to occupy rather than a
	// single point, so the lights are already sensible when the plan arrives.
	if (Bounds.GetExtent().IsNearlyZero())
	{
		const FVector Origin = TargetStage->GetActorLocation();
		Bounds = FBox(Origin - FVector(400.0f, 400.0f, 100.0f), Origin + FVector(400.0f, 400.0f, 100.0f));
	}

	const FVector TargetCenter = Bounds.GetCenter();
	const float TargetRadius = FMath::Max(static_cast<float>(Bounds.GetExtent().Size()), 50.0f);

	if (bSnap || !bHasTarget || FollowSpeed <= 0.0f)
	{
		SmoothedCenter = TargetCenter;
		SmoothedRadius = TargetRadius;
		bHasTarget = true;
	}
	else
	{
		SmoothedCenter = FMath::VInterpTo(SmoothedCenter, TargetCenter, DeltaSeconds, FollowSpeed);
		SmoothedRadius = FMath::FInterpTo(SmoothedRadius, TargetRadius, DeltaSeconds, FollowSpeed);
	}

	// Angles are taken in the stage's frame: turn the installation, and the
	// lighting turns with it - the same "local, never world" rule as the room.
	const FQuat StageRotation = TargetStage->GetActorQuat();

	ApplyLight(KeyLight, Key, StageRotation);
	ApplyLight(BackLight, Back, StageRotation);
	ApplyLight(FillLight, Fill, StageRotation);
}

void AVibH2OLightRig::ApplyLight(USpotLightComponent* Component, const FVibH2ORigLight& Settings, const FQuat& StageRotation) const
{
	if (Component == nullptr)
	{
		return;
	}

	const bool bOn = Settings.bEnabled && MasterIntensity > 0.0f && Settings.Illuminance > 0.0f;
	if (Component->IsVisible() != bOn)
	{
		Component->SetVisibility(bOn);
	}
	if (!bOn)
	{
		return;
	}

	const float HalfAngle = FMath::Clamp(Settings.ConeAngle, 5.0f, 80.0f);

	// Back off until the cone holds the whole room: a sphere of radius R seen
	// under a half-angle A needs a distance of R / sin(A).
	const float CoverRadius = SmoothedRadius * FMath::Max(CoverageMargin, 1.0f);
	const float Distance = FMath::Max(CoverRadius / FMath::Sin(FMath::DegreesToRadians(HalfAngle)), MinDistance);

	const FVector LocalFrom = FRotator(Settings.Elevation, Settings.Azimuth, 0.0f).Vector();
	const FVector From = StageRotation.RotateVector(LocalFrom);
	const FVector Location = SmoothedCenter + From * Distance;
	const FRotator Aim = (-From).Rotation();

	Component->SetWorldLocationAndRotation(Location, Aim);

	// Inverse square: the lamp gets stronger as it backs off, so the bubbles
	// receive the requested lux at any room size. Distance is in cm, lux are
	// per square metre.
	const float DistanceMeters = Distance / 100.0f;
	const float Candelas = Settings.Illuminance * MasterIntensity * DistanceMeters * DistanceMeters;

	// Render-state changes are not free: only push what actually moved.
	if (!FMath::IsNearlyEqual(Component->Intensity, Candelas, Candelas * 0.01f + KINDA_SMALL_NUMBER))
	{
		Component->SetIntensity(Candelas);
	}
	if (Component->GetLightColor() != Settings.Color)
	{
		Component->SetLightColor(Settings.Color);
	}

	// The beam softens over its last third rather than ending in a hard ring.
	const float Inner = HalfAngle * 0.66f;
	if (!FMath::IsNearlyEqual(Component->OuterConeAngle, HalfAngle, 0.05f))
	{
		Component->SetOuterConeAngle(HalfAngle);
	}
	if (!FMath::IsNearlyEqual(Component->InnerConeAngle, Inner, 0.05f))
	{
		Component->SetInnerConeAngle(Inner);
	}

	// Reach just past the far side of the room, no further: an attenuation
	// radius much larger than needed costs shading over empty water.
	const float Reach = Distance + CoverRadius * 2.0f;
	if (!FMath::IsNearlyEqual(Component->AttenuationRadius, Reach, Reach * 0.02f))
	{
		Component->SetAttenuationRadius(Reach);
	}

	if (!FMath::IsNearlyEqual(Component->VolumetricScatteringIntensity, Settings.VolumetricScattering, 0.01f))
	{
		Component->SetVolumetricScatteringIntensity(Settings.VolumetricScattering);
	}
	if ((Component->CastShadows != 0) != Settings.bCastShadows)
	{
		Component->SetCastShadows(Settings.bCastShadows);
	}
}
