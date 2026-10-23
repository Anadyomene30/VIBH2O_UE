#include "VibH2OStageActor.h"

#include "Curves/CurveFloat.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Layout/VibH2OLayoutMath.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Steering/VibH2OAttractorActor.h"
#include "Steering/VibH2OExclusionZoneActor.h"
#include "Steering/VibH2OSteeringMath.h"
#include "VibH2OBubbleActor.h"
#include "VibH2OMood.h"
#include "VibH2OModule.h"
#include "VibH2OSubsystem.h"

AVibH2OStageActor::AVibH2OStageActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	// The subsystem ticks as an FTickableGameObject, on its own schedule
	// relative to actor ticks. Whichever side happens to run first, the worst
	// case is one frame of latency on freshly drained data - invisible at stage
	// timescales - and beat-phase continuity is unaffected either way, since
	// the phase advances in the subsystem, not here.
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	StageRoot = CreateDefaultSubobject<USceneComponent>(TEXT("StageRoot"));
	SetRootComponent(StageRoot);

	BubbleClass = AVibH2OBubbleActor::StaticClass();

	GridDriver = CreateDefaultSubobject<UVibH2OGridDriver>(TEXT("GridDriver"));
	// A real school by default: the cone was the placeholder that proved the
	// morph, the boids are what the piece asks for.
	FlockDriver = CreateDefaultSubobject<UVibH2OFlockDriver>(TEXT("FlockDriver"));
}

UVibH2OSubsystem* AVibH2OStageActor::GetSubsystem() const
{
	return UVibH2OSubsystem::Get(this);
}

void AVibH2OStageActor::BeginPlay()
{
	Super::BeginPlay();

	if (UVibH2OSubsystem* Subsystem = GetSubsystem())
	{
		BindToSubsystem(Subsystem);
	}
	else
	{
		UE_LOG(LogVibH2O, Warning, TEXT("VibH2O: acteur de scene '%s' sans subsystem — aucune bulle ne sera creee."), *GetName());
	}
}

void AVibH2OStageActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromSubsystem();
	DestroyBubbles();
	Super::EndPlay(EndPlayReason);
}

void AVibH2OStageActor::Destroyed()
{
	// Deleting the stage in the editor: its live bubbles go with it, rather
	// than being left floating, ownerless, until the map is reloaded.
	UnbindFromSubsystem();
	DestroyBubbles();
	Super::Destroyed();
}

void AVibH2OStageActor::BindToSubsystem(UVibH2OSubsystem* Subsystem)
{
	if (Subsystem == nullptr || BoundSubsystem.Get() == Subsystem)
	{
		return;
	}
	UnbindFromSubsystem();

	Subsystem->OnRoomChanged.AddDynamic(this, &AVibH2OStageActor::HandleRoomChanged);
	Subsystem->OnOscBlendAlphaChanged.AddDynamic(this, &AVibH2OStageActor::HandleOscBlendAlpha);
	Subsystem->OnOscFocusAmountChanged.AddDynamic(this, &AVibH2OStageActor::HandleOscFocusAmount);
	Subsystem->OnOscFocusGroupChanged.AddDynamic(this, &AVibH2OStageActor::HandleOscFocusGroup);
	Subsystem->OnOscAttractorChanged.AddDynamic(this, &AVibH2OStageActor::HandleOscAttractor);
	Subsystem->OnOscAttractorShareChanged.AddDynamic(this, &AVibH2OStageActor::HandleOscAttractorShare);
	Subsystem->OnOscAttractorsReleased.AddDynamic(this, &AVibH2OStageActor::HandleOscAttractorsReleased);
	Subsystem->OnOscAttractorFadeScaleChanged.AddDynamic(this, &AVibH2OStageActor::HandleOscAttractorFadeScale);
	Subsystem->OnOscExclusionChanged.AddDynamic(this, &AVibH2OStageActor::HandleOscExclusion);
	Subsystem->OnOscExclusionMoved.AddDynamic(this, &AVibH2OStageActor::HandleOscExclusionMoved);
	BoundSubsystem = Subsystem;

	// A room may have arrived before this actor started up.
	if (Subsystem->HasRoom())
	{
		HandleRoomChanged(Subsystem->GetRoomRef());
	}
}

void AVibH2OStageActor::UnbindFromSubsystem()
{
	if (UVibH2OSubsystem* Subsystem = BoundSubsystem.Get())
	{
		Subsystem->OnRoomChanged.RemoveDynamic(this, &AVibH2OStageActor::HandleRoomChanged);
		Subsystem->OnOscBlendAlphaChanged.RemoveDynamic(this, &AVibH2OStageActor::HandleOscBlendAlpha);
		Subsystem->OnOscFocusAmountChanged.RemoveDynamic(this, &AVibH2OStageActor::HandleOscFocusAmount);
		Subsystem->OnOscFocusGroupChanged.RemoveDynamic(this, &AVibH2OStageActor::HandleOscFocusGroup);
		Subsystem->OnOscAttractorChanged.RemoveDynamic(this, &AVibH2OStageActor::HandleOscAttractor);
		Subsystem->OnOscAttractorShareChanged.RemoveDynamic(this, &AVibH2OStageActor::HandleOscAttractorShare);
		Subsystem->OnOscAttractorsReleased.RemoveDynamic(this, &AVibH2OStageActor::HandleOscAttractorsReleased);
		Subsystem->OnOscAttractorFadeScaleChanged.RemoveDynamic(this, &AVibH2OStageActor::HandleOscAttractorFadeScale);
		Subsystem->OnOscExclusionChanged.RemoveDynamic(this, &AVibH2OStageActor::HandleOscExclusion);
		Subsystem->OnOscExclusionMoved.RemoveDynamic(this, &AVibH2OStageActor::HandleOscExclusionMoved);
	}
	BoundSubsystem.Reset();
}

void AVibH2OStageActor::DestroyBubbles()
{
	for (const TPair<int32, TObjectPtr<AVibH2OBubbleActor>>& Entry : Bubbles)
	{
		if (IsValid(Entry.Value))
		{
			Entry.Value->Destroy();
		}
	}
	Bubbles.Reset();
}

bool AVibH2OStageActor::ShouldTickIfViewportsOnly() const
{
	// Ticking in the editor drives the live bubbles, or failing that draws the
	// debug preview.
	return bPreviewInEditor || bLiveInEditor;
}

#if WITH_EDITOR
void AVibH2OStageActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AVibH2OStageActor, ActiveFocusGroup))
	{
		OnFocusGroupChanged.Broadcast(ActiveFocusGroup);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(AVibH2OStageActor, MoodPreset))
	{
		// Dropping a preset loads it straight away: that one gesture is the
		// whole point of having presets on the get-in.
		ApplyMoodPreset(MoodPreset);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(AVibH2OStageActor, bLiveInEditor) && !bLiveInEditor)
	{
		const UWorld* World = GetWorld();
		if (World != nullptr && !World->IsGameWorld())
		{
			UnbindFromSubsystem();
			DestroyBubbles();
		}
	}
}
#endif

// ------------------------------------------------------------------ Events

void AVibH2OStageActor::HandleRoomChanged(const FVibH2ORoomSnapshot& NewRoom)
{
	ReconcileBubbles(NewRoom);
}

void AVibH2OStageActor::HandleOscBlendAlpha(float Value)
{
	// We only write on reception. During Sequencer playback the track rewrites
	// the property every frame and therefore wins on its own.
	if (bFollowOscBlend)
	{
		BlendAlpha = Value;
	}
}

void AVibH2OStageActor::HandleOscFocusAmount(float Value)
{
	if (bFollowOscFocus)
	{
		FocusAmount = Value;
	}
}

void AVibH2OStageActor::HandleOscFocusGroup(FName GroupName)
{
	if (bFollowOscFocus)
	{
		SetActiveFocusGroup(GroupName);
	}
}

// ---------------------------------------------------------- Bubble lifetime

void AVibH2OStageActor::RebuildNow()
{
	if (UVibH2OSubsystem* Subsystem = GetSubsystem())
	{
		// A forced rebuild destroys everything, then reconciles against an
		// empty map: that is exactly the creation path.
		DestroyBubbles();
		ReconcileBubbles(Subsystem->GetRoomRef());
	}
}

void AVibH2OStageActor::ReconcileBubbles(const FVibH2ORoomSnapshot& NewRoom)
{
	UWorld* World = GetWorld();
	if (World == nullptr || StageRoot == nullptr)
	{
		return;
	}

	Room = NewRoom;

	const UClass* SpawnClass = BubbleClass ? BubbleClass.Get() : AVibH2OBubbleActor::StaticClass();
	if (SpawnClass == nullptr)
	{
		return;
	}

	int32 Created = 0;
	int32 Reused = 0;
	int32 Destroyed = 0;

	TSet<int32> WantedSeats;

	for (const FVibH2OSeat& Seat : Room.Seats)
	{
		if (!Seat.bOccupied)
		{
			// Pitfall 3: an empty seat has no bubble. The grid has holes, and
			// that is normal.
			continue;
		}
		WantedSeats.Add(Seat.SeatIndex);

		TObjectPtr<AVibH2OBubbleActor>* Existing = Bubbles.Find(Seat.SeatIndex);
		if (Existing != nullptr && IsValid(*Existing))
		{
			// Rebuild by difference: the bubble stays put. Without this the
			// whole room would flicker on every send from Max.
			if ((*Existing)->IndividualId != Seat.IndividualId)
			{
				(*Existing)->AssignSeat(Seat);
			}
			++Reused;
			continue;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.ObjectFlags |= RF_Transient;
#if WITH_EDITOR
		if (!World->IsGameWorld())
		{
			// Editor bubbles are scaffolding: no transaction, no dirty level,
			// no outliner entry, and never written into an actor package.
			SpawnParams.bTemporaryEditorActor = true;
			SpawnParams.bHideFromSceneOutliner = true;
			SpawnParams.bCreateActorPackage = false;
		}
#endif

		AVibH2OBubbleActor* Bubble = World->SpawnActor<AVibH2OBubbleActor>(
			const_cast<UClass*>(SpawnClass), GetActorTransform(), SpawnParams);

		if (Bubble == nullptr)
		{
			continue;
		}

		// Attaching to the stage root is what makes moving, rotating or scaling
		// the actor carry the whole room along with it.
		//
		// SnapToTarget, not KeepRelative: an unparented actor's relative
		// transform equals its world transform, so keeping it would place the
		// bubble at twice the stage offset for one frame before Tick corrects
		// it. Snapping starts it at the root, where the drivers expect it.
		Bubble->AttachToComponent(StageRoot, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Bubble->AssignSeat(Seat);
		Bubbles.Add(Seat.SeatIndex, Bubble);
		++Created;
	}

	for (auto It = Bubbles.CreateIterator(); It; ++It)
	{
		if (!WantedSeats.Contains(It.Key()))
		{
			if (IsValid(It.Value()))
			{
				It.Value()->Destroy();
			}
			It.RemoveCurrent();
			++Destroyed;
		}
	}

	UE_LOG(LogVibH2O, Log,
		TEXT("VibH2O: salle reconstruite — %d bulles creees, %d conservees, %d detruites (total %d)."),
		Created, Reused, Destroyed, Bubbles.Num());

	OnRoomBuilt.Broadcast(Bubbles.Num());
}

// --------------------------------------------------------------------- Tick

void AVibH2OStageActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	StageTime += DeltaSeconds;

	const UWorld* World = GetWorld();
	if (World != nullptr && World->IsGameWorld())
	{
		UpdateBubbles(DeltaSeconds);
		PublishRemoteState(DeltaSeconds);
		return;
	}

#if WITH_EDITOR
	if (bLiveInEditor && TickEditorLive(DeltaSeconds))
	{
		return;
	}
	if (bPreviewInEditor)
	{
		DrawEditorPreview();
	}
#endif
}

bool AVibH2OStageActor::TickEditorLive(float DeltaSeconds)
{
#if WITH_EDITOR
	// GetSubsystem() hands back the editor stand-in here, and nullptr for
	// worlds that should not simulate (Blueprint editor previews).
	UVibH2OSubsystem* Subsystem = GetSubsystem();
	if (Subsystem == nullptr || !Subsystem->IsEditorPreview())
	{
		return false;
	}

	// Ticking it settles a pending room plan, which raises OnRoomChanged and
	// creates the bubbles through the very path Play uses.
	BindToSubsystem(Subsystem);
	Subsystem->TickEditorPreview(DeltaSeconds);

	// Catch-up on the generation, not only the event: in a heavy map opened
	// under 5.8 the room settled without the stage ever receiving
	// OnRoomChanged, and the editor stayed empty. Comparing generations costs
	// one integer per frame and makes a missed broadcast harmless.
	if (Subsystem->HasRoom() && Subsystem->GetRoomRef().Generation != Room.Generation)
	{
		UE_LOG(LogVibH2O, Log, TEXT("VibH2O: plan de salle rattrape dans l'editeur (generation %d)."),
			Subsystem->GetRoomRef().Generation);
		ReconcileBubbles(Subsystem->GetRoomRef());
	}

	if (Bubbles.Num() == 0)
	{
		return false;
	}
	UpdateBubbles(DeltaSeconds);
	return true;
#else
	return false;
#endif
}

FVibH2OGridParams AVibH2OStageActor::MakeGridParams(int32 Columns, int32 Rows) const
{
	FVibH2OGridParams Params;
	Params.Columns = Columns;
	Params.Rows = Rows;
	Params.ColumnSpacing = ColumnSpacing;
	Params.RowSpacing = RowSpacing;
	Params.CurvatureAngleDeg = CurvatureAngle;
	Params.bFanOut = bFanOut;
	Params.bOrientToCenter = bOrientToCenter;
	Params.OrientationOffset = OrientationOffset;
	Params.ElevationPerRow = ElevationPerRow;
	Params.ElevationCurve = ElevationCurve ? &ElevationCurve->FloatCurve : nullptr;
	Params.DepthAmplitude = DepthAmplitude;
	Params.DepthNoiseScale = DepthNoiseScale;
	return Params;
}

void AVibH2OStageActor::UpdateBubbles(float DeltaSeconds)
{
	UVibH2OSubsystem* Subsystem = GetSubsystem();
	if (Subsystem == nullptr)
	{
		return;
	}

	UpdateMood(Subsystem, DeltaSeconds);

	if (Bubbles.Num() == 0)
	{
		return;
	}

	const float CollectiveSynchrony = Subsystem->GetCollectiveSynchrony();
	const int32 OccupiedCount = Subsystem->GetOccupiedCount();

	const bool bFloatEnabled = Subsystem->IsEffectEnabled(VibH2OEffects::Float);
	const bool bDriftEnabled = Subsystem->IsEffectEnabled(VibH2OEffects::Drift);
	const bool bTintEnabled = Subsystem->IsEffectEnabled(VibH2OEffects::Tint);
	const bool bFocusEnabled = Subsystem->IsEffectEnabled(VibH2OEffects::Focus);
	const bool bScaleEnabled = Subsystem->IsEffectEnabled(VibH2OEffects::Scale);

	// Easing curve for the morph, if the artist supplied one.
	const float RawBlend = FMath::Clamp(BlendAlpha, 0.0f, 1.0f);
	const float Blend = BlendCurve ? FMath::Clamp(BlendCurve->GetFloatValue(RawBlend), 0.0f, 1.0f) : RawBlend;

	// Drivers that carry state step forward once, as a whole, before anyone
	// asks where a single bubble is. A school has to move as a school.
	FVibH2OFrameContext Frame;
	Frame.Stage = this;
	Frame.DeltaSeconds = DeltaSeconds;
	Frame.TimeSeconds = StageTime;
	Frame.OccupiedCount = OccupiedCount;
	Frame.CollectiveSynchrony = CollectiveSynchrony;
	Frame.AverageExcitation = Subsystem->GetAverageExcitation();
	Frame.BlendAlpha = Blend;

	if (GridDriver != nullptr)
	{
		GridDriver->PrepareFrame(Frame);
	}
	if (FlockDriver != nullptr)
	{
		FlockDriver->PrepareFrame(Frame);
	}

	// The hand-steered layers step their fades once, before any bubble.
	BeginSteering(DeltaSeconds);

	// One view position for the whole room: every seat number turns toward it,
	// and asking the player controller once beats asking it two hundred times.
	FVector ViewLocation = GetActorLocation();
	if (bShowSeatNumbers)
	{
		if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			FRotator ViewRotation = FRotator::ZeroRotator;
			PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
		}
		else if (GetWorld()->ViewLocationsRenderedLastFrame.Num() > 0)
		{
			// No player in the editor: the viewport camera stands in for one.
			ViewLocation = GetWorld()->ViewLocationsRenderedLastFrame[0];
		}
	}

	// The mood scales the motion. Amplitudes only: speeds go through the
	// float clock, which the mood advances faster or slower, so that nothing
	// ever jumps when the mood changes.
	FVibH2OFloatParams FloatParams;
	FloatParams.Amplitude = FloatAmplitude * BlendedLook.FloatScale;
	FloatParams.Speed = FloatSpeed;
	FloatParams.RotationAmount = FloatRotationAmount;

	FVibH2ODriftParams DriftParams;
	DriftParams.Threshold = DriftThreshold;
	DriftParams.Intensity = DriftIntensity * BlendedLook.DriftScale;
	DriftParams.DirectionSpeed = DriftDirectionSpeed;
	DriftParams.MaxRatio = DriftMaxRatio;

	const float MinSpacing = FMath::Min(ColumnSpacing, RowSpacing);

	static const FVibH2OIndividualState EmptyState;

	for (const FVibH2OSeat& Seat : Room.Seats)
	{
		if (!Seat.bOccupied)
		{
			continue;
		}

		TObjectPtr<AVibH2OBubbleActor>* Found = Bubbles.Find(Seat.SeatIndex);
		if (Found == nullptr || !IsValid(*Found))
		{
			continue;
		}
		AVibH2OBubbleActor* Bubble = *Found;

		const FVibH2OIndividualState* StatePtr = Subsystem->FindIndividual(Seat.IndividualId);
		const FVibH2OIndividualState& State = StatePtr ? *StatePtr : EmptyState;

		FVibH2OLayoutContext Context;
		Context.Stage = this;
		Context.Seat = Seat;
		Context.Columns = Room.Columns;
		Context.Rows = Room.Rows;
		Context.OccupiedOrdinal = Subsystem->GetOccupiedOrdinal(Seat.SeatIndex);
		Context.OccupiedCount = OccupiedCount;
		Context.Individual = State;
		Context.CollectiveSynchrony = CollectiveSynchrony;
		Context.TimeSeconds = StageTime;
		// The seed derives from the seat index, so relief and phases stay
		// stable from one launch to the next.
		Context.Seed = Seat.SeatIndex;

		// The position stack, as described in docs/SPEC.md section E.
		FTransform GridTransform = FTransform::Identity;
		if (GridDriver != nullptr)
		{
			GridTransform = GridDriver->EvaluateSeat(Context);
		}

		FVector Location = GridTransform.GetLocation();
		FQuat Rotation = GridTransform.GetRotation();

		if (Blend > 0.0f && FlockDriver != nullptr)
		{
			const FTransform FlockTransform = FlockDriver->EvaluateSeat(Context);
			Location = FMath::Lerp(Location, FlockTransform.GetLocation(), Blend);
			Rotation = FQuat::Slerp(Rotation, FlockTransform.GetRotation(), Blend);
			Rotation.Normalize();
		}

		// Attractors pull AFTER the blend, never inside a driver: the drivers
		// keep running underneath, so a released bubble falls back onto a place
		// that was kept for it all along.
		Location = ApplyAttractors(Location, Seat.SeatIndex, Context.Seed);

		// The float is added AFTER the blend, and not inside the grid driver:
		// it is a property of the bubble - it is underwater, so it floats - and
		// not of the tableau. That is how it survives the move to the vortex.
		if (bFloatEnabled && FloatParams.Amplitude > 0.0f)
		{
			Location += FVibH2OLayoutMath::EvaluateFloatOffset(FloatParams, Context.Seed, FloatClock);
		}
		if (bFloatEnabled && FloatRotationAmount > 0.0f)
		{
			Rotation = Rotation * FVibH2OLayoutMath::EvaluateFloatRotation(FloatParams, Context.Seed, FloatClock).Quaternion();
		}

		// The drift belongs to the person too, not to the tableau: it follows
		// arousal, and stays bounded by construction.
		if (bDriftEnabled && DriftParams.Intensity > 0.0f)
		{
			Location += FVibH2OLayoutMath::EvaluateDriftOffset(DriftParams, Context.Seed, StageTime, State.Excitation, MinSpacing);
		}

		// Exclusion comes last of all, so nothing added after it - float,
		// drift, attractor - can carry a bubble back in front of a performer.
		Location = ApplyExclusionZones(Location, Seat.SeatIndex, Context.Seed);

		float Scale = BaseScale;
		if (bScaleEnabled)
		{
			// The mood sets the size of the whole room and how hard it beats;
			// the person's own arousal still swells their bubble within that.
			Scale *= BlendedLook.SizeScale
				* (1.0f + State.Excitation * ScalePerExcitation + State.BeatPulse * BeatPopAmount * BlendedLook.PulseScale);
		}

		// Three axes, three slightly different rates, a phase of its own: the
		// bubble is never a sphere, and never the same shape twice. Arousal
		// deepens it, so an excited bubble is visibly more restless in
		// silhouette before any colour has changed.
		FVector ScaleVector(Scale);
		if (bScaleEnabled && OrganicDeform > 0.0f)
		{
			// Arousal adds to the restlessness without doubling it: excited is
			// livelier, not a different material.
			const float Amount = OrganicDeform * (0.7f + State.Excitation * 0.6f) * BlendedLook.Turbulence;
			const float W = StageTime * OrganicSpeed * 2.0f * PI;
			const float P0 = FVibH2ONoise::UnitFloat(Context.Seed * 23 + 1) * 2.0f * PI;
			const float P1 = FVibH2ONoise::UnitFloat(Context.Seed * 23 + 2) * 2.0f * PI;
			const float P2 = FVibH2ONoise::UnitFloat(Context.Seed * 23 + 3) * 2.0f * PI;

			ScaleVector = FVector(
				Scale * (1.0f + Amount * FMath::Sin(W * 1.00f + P0)),
				Scale * (1.0f + Amount * FMath::Sin(W * 0.79f + P1)),
				Scale * (1.0f + Amount * FMath::Sin(W * 1.23f + P2)));
		}

		Bubble->SetActorRelativeLocation(Location);
		Bubble->SetActorRelativeRotation(Rotation);
		Bubble->SetActorRelativeScale3D(ScaleVector);

		const float Mask = bFocusEnabled ? ComputeFocusMask(Seat) : 1.0f;

		// Synchrony is its own axis: it decides how far this bubble falls in
		// step with the room - shared striations, shared hue, a shared halo -
		// and never which mood's palette it wears.
		const float Coherence = FMath::Clamp(
			FMath::Lerp(CollectiveSynchrony, State.Synchrony, IndividualCoherenceShare), 0.0f, 1.0f);

		// Where the person stands against the ROOM, not against an absolute
		// scale: the room's mean sits mid-palette, so each mood's palette is
		// used in full whatever its level. In step, the gap closes.
		const float Gap = (State.Excitation - RoomExcitation) * (1.0f - Coherence * MoodPalette.SyncHueTightening);
		// Tint switched off means no colour response at all - neither the
		// room's mood nor the person's nuance: one fixed colour, the middle of
		// the normal palette. Keeping the mood's colour would leave the toggle
		// doing nothing whenever the room is uniform, which the isolation
		// harness caught.
		const FLinearColor Tint = bTintEnabled
			? BlendedLook.ColorAt(0.5f + Gap * MoodPalette.IndividualContrast)
			: MoodPalette.Normal.ColorAt(0.5f);

		FVibH2OBubbleMood BubbleMood;
		BubbleMood.Weights = MoodTracker.GetWeights();
		BubbleMood.Mood = CurrentMood;
		BubbleMood.Glow = BlendedLook.Glow;
		BubbleMood.Turbulence = BlendedLook.Turbulence;
		// An aroused person's striations run a little faster than the room's.
		BubbleMood.StripeSpeed = BlendedLook.StripeSpeed * (0.75f + 0.5f * FMath::Clamp(State.Excitation, 0.0f, 1.0f));
		BubbleMood.RoomStripePhase = RoomStripePhase;
		BubbleMood.Coherence = Coherence;
		BubbleMood.SyncHalo = MoodPalette.SyncHalo;
		BubbleMood.RoomExcitation = RoomExcitation;
		BubbleMood.Brightness = BubbleBrightness;
		Bubble->SetMood(BubbleMood, DeltaSeconds);

		Bubble->SetSeatLabelVisible(bShowSeatNumbers);
		if (bShowSeatNumbers)
		{
			Bubble->OrientSeatLabel(ViewLocation);
		}

		Bubble->UpdateFromState(State, Mask, Tint, CollectiveSynchrony, Blend, StriationSpeedScale, DeltaSeconds);
	}

	EndSteering();
}

// ------------------------------------------------------ Attractors and zones

namespace
{
	template <typename ActorType>
	void SortForDisplay(TArray<ActorType*>& Actors)
	{
		Actors.Sort([](const ActorType& A, const ActorType& B)
		{
			if (A.DisplayOrder != B.DisplayOrder)
			{
				return A.DisplayOrder < B.DisplayOrder;
			}
			return A.GetEffectiveLabel() < B.GetEffectiveLabel();
		});
	}

	FString ToHexColor(const FLinearColor& Color)
	{
		const FColor Srgb = Color.ToFColor(true);
		return FString::Printf(TEXT("#%02x%02x%02x"), Srgb.R, Srgb.G, Srgb.B);
	}
}

void AVibH2OStageActor::BeginSteering(float DeltaSeconds)
{
	FrameAttractors.Reset();
	FrameZones.Reset();

	UWorld* World = GetWorld();
	if (World == nullptr || StageRoot == nullptr)
	{
		return;
	}

	// Found afresh every frame rather than registered: a zone dropped into the
	// level, deleted, or streamed in takes effect at once, in the editor as in
	// Play. A few dozen actors of one class is a cheap walk.
	const FTransform RootTransform = StageRoot->GetComponentTransform();

	if (bApplyAttractors)
	{
		for (TActorIterator<AVibH2OAttractorActor> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				It->BeginSteeringFrame(RootTransform, AttractorFadeScale, DeltaSeconds, StageTime);
				FrameAttractors.Add(*It);
			}
		}
	}

	if (bApplyExclusionZones)
	{
		for (TActorIterator<AVibH2OExclusionZoneActor> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				It->BeginSteeringFrame(RootTransform, DeltaSeconds);
				FrameZones.Add(*It);
			}
		}
	}
}

void AVibH2OStageActor::EndSteering()
{
	for (AVibH2OAttractorActor* Attractor : FrameAttractors)
	{
		if (IsValid(Attractor))
		{
			Attractor->EndSteeringFrame();
		}
	}
	for (AVibH2OExclusionZoneActor* Zone : FrameZones)
	{
		if (IsValid(Zone))
		{
			Zone->EndSteeringFrame();
		}
	}
}

FVector AVibH2OStageActor::ApplyAttractors(const FVector& Base, int32 SeatIndex, int32 Seed)
{
	if (FrameAttractors.Num() == 0)
	{
		return Base;
	}

	TArray<FVector, TInlineAllocator<8>> Targets;
	TArray<float, TInlineAllocator<8>> Weights;
	for (AVibH2OAttractorActor* Attractor : FrameAttractors)
	{
		const float Weight = Attractor->StepBubbleWeight(SeatIndex, Seed);
		if (Weight > 0.0f)
		{
			Targets.Add(Attractor->GetCloudTarget(Seed));
			Weights.Add(Weight);
		}
	}
	return FVibH2OSteeringMath::BlendTowardTargets(Base, MakeArrayView(Targets), MakeArrayView(Weights));
}

FVector AVibH2OStageActor::ApplyExclusionZones(const FVector& Location, int32 SeatIndex, int32 Seed)
{
	FVector Result = Location;
	for (AVibH2OExclusionZoneActor* Zone : FrameZones)
	{
		Result = Zone->ApplyToBubble(SeatIndex, Seed, Result);
	}
	return Result;
}

TArray<AVibH2OAttractorActor*> AVibH2OStageActor::GetAttractors() const
{
	TArray<AVibH2OAttractorActor*> Result;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AVibH2OAttractorActor> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				Result.Add(*It);
			}
		}
	}
	SortForDisplay(Result);
	return Result;
}

TArray<AVibH2OExclusionZoneActor*> AVibH2OStageActor::GetExclusionZones() const
{
	TArray<AVibH2OExclusionZoneActor*> Result;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AVibH2OExclusionZoneActor> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				Result.Add(*It);
			}
		}
	}
	SortForDisplay(Result);
	return Result;
}

AVibH2OAttractorActor* AVibH2OStageActor::FindAttractor(FName Name) const
{
	// FName comparison ignores case: "/Attractor/gauche/" finds "Gauche".
	for (AVibH2OAttractorActor* Attractor : GetAttractors())
	{
		if (Attractor->GetEffectiveControlName() == Name)
		{
			return Attractor;
		}
	}
	UE_LOG(LogVibH2O, Warning, TEXT("VibH2O: attracteur '%s' inconnu dans ce niveau."), *Name.ToString());
	return nullptr;
}

AVibH2OExclusionZoneActor* AVibH2OStageActor::FindExclusionZone(FName Name) const
{
	for (AVibH2OExclusionZoneActor* Zone : GetExclusionZones())
	{
		if (Zone->GetEffectiveControlName() == Name)
		{
			return Zone;
		}
	}
	UE_LOG(LogVibH2O, Warning, TEXT("VibH2O: zone d'exclusion '%s' inconnue dans ce niveau."), *Name.ToString());
	return nullptr;
}

void AVibH2OStageActor::ReleaseAllAttractors()
{
	int32 Released = 0;
	for (AVibH2OAttractorActor* Attractor : GetAttractors())
	{
		// One driven by Sequencer is not ours to release.
		if (Attractor->bFollowOsc && Attractor->Activation > 0.0f)
		{
			Attractor->Activation = 0.0f;
			++Released;
		}
	}
	UE_LOG(LogVibH2O, Log, TEXT("VibH2O: tous les attracteurs relaches (%d)."), Released);
}

void AVibH2OStageActor::HandleOscAttractor(FName Name, float Value)
{
	AVibH2OAttractorActor* Attractor = FindAttractor(Name);
	if (Attractor != nullptr && Attractor->bFollowOsc)
	{
		Attractor->Activation = FMath::Clamp(Value, 0.0f, 1.0f);
	}
}

void AVibH2OStageActor::HandleOscAttractorShare(FName Name, float Value)
{
	AVibH2OAttractorActor* Attractor = FindAttractor(Name);
	if (Attractor != nullptr && Attractor->bFollowOsc)
	{
		Attractor->Share = FMath::Clamp(Value, 0.0f, 1.0f);
	}
}

void AVibH2OStageActor::HandleOscAttractorsReleased()
{
	if (bFollowOscSteering)
	{
		ReleaseAllAttractors();
	}
}

void AVibH2OStageActor::HandleOscAttractorFadeScale(float Value)
{
	if (bFollowOscSteering)
	{
		AttractorFadeScale = FMath::Clamp(Value, 0.1f, 10.0f);
	}
}

void AVibH2OStageActor::HandleOscExclusion(FName Name, float Value)
{
	AVibH2OExclusionZoneActor* Zone = FindExclusionZone(Name);
	if (Zone != nullptr && Zone->bFollowOsc)
	{
		Zone->Activation = FMath::Clamp(Value, 0.0f, 1.0f);
	}
}

void AVibH2OStageActor::HandleOscExclusionMoved(FName Name, FVector2D Position)
{
	AVibH2OExclusionZoneActor* Zone = FindExclusionZone(Name);
	if (Zone != nullptr && Zone->bFollowOsc)
	{
		Zone->SetDragPosition(static_cast<float>(Position.X), static_cast<float>(Position.Y));
	}
}

// ------------------------------------------------------------------ Remote

void AVibH2OStageActor::PublishRemoteState(float DeltaSeconds)
{
	if (!bPublishRemoteState)
	{
		return;
	}
	UVibH2OSubsystem* Subsystem = GetSubsystem();
	if (Subsystem == nullptr || !Subsystem->IsRemoteRunning())
	{
		return;
	}

	// A few times a second is plenty for a button ring, and keeps the JSON
	// off the frame budget.
	RemotePublishTimer -= DeltaSeconds;
	if (RemotePublishTimer > 0.0f)
	{
		return;
	}
	RemotePublishTimer = 0.15f;
	Subsystem->SetRemoteState(BuildRemoteStateJson());
}

FString AVibH2OStageActor::BuildRemoteStateJson() const
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("v"), 1);
	Root->SetNumberField(TEXT("bubbles"), Bubbles.Num());
	Root->SetNumberField(TEXT("blend"), BlendAlpha);
	Root->SetNumberField(TEXT("fadeScale"), AttractorFadeScale);

	TArray<TSharedPtr<FJsonValue>> Attractors;
	for (const AVibH2OAttractorActor* Attractor : GetAttractors())
	{
		const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("id"), Attractor->GetEffectiveControlName().ToString());
		Entry->SetStringField(TEXT("label"), Attractor->GetEffectiveLabel());
		Entry->SetStringField(TEXT("color"), ToHexColor(Attractor->DisplayColor));
		Entry->SetNumberField(TEXT("on"), Attractor->Activation);
		Entry->SetNumberField(TEXT("w"), Attractor->GetCurrentWeight());
		Entry->SetNumberField(TEXT("share"), Attractor->Share);
		Entry->SetBoolField(TEXT("osc"), Attractor->bFollowOsc);
		Attractors.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Root->SetArrayField(TEXT("attractors"), Attractors);

	TArray<TSharedPtr<FJsonValue>> Zones;
	for (const AVibH2OExclusionZoneActor* Zone : GetExclusionZones())
	{
		const FVector2D HalfSize = Zone->GetHalfSize();
		const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("id"), Zone->GetEffectiveControlName().ToString());
		Entry->SetStringField(TEXT("label"), Zone->GetEffectiveLabel());
		Entry->SetStringField(TEXT("color"), ToHexColor(Zone->DisplayColor));
		Entry->SetNumberField(TEXT("on"), Zone->Activation);
		Entry->SetNumberField(TEXT("w"), Zone->GetCurrentWeight());
		Entry->SetNumberField(TEXT("x"), Zone->DragX);
		Entry->SetNumberField(TEXT("y"), Zone->DragY);
		// The zone's size on the pad, as a fraction of the drag range.
		Entry->SetNumberField(TEXT("sx"), HalfSize.X / FMath::Max(Zone->DragRange.X, 1.0));
		Entry->SetNumberField(TEXT("sy"), HalfSize.Y / FMath::Max(Zone->DragRange.Y, 1.0));
		Entry->SetNumberField(TEXT("aspect"), Zone->DragRange.X / FMath::Max(Zone->DragRange.Y, 1.0));
		Entry->SetBoolField(TEXT("mirror"), Zone->bMirrorDrag);
		Entry->SetStringField(TEXT("shape"), Zone->Shape == EVibH2OExclusionShape::Rectangle ? TEXT("rect") : TEXT("ellipse"));
		Entry->SetBoolField(TEXT("osc"), Zone->bFollowOsc);
		Zones.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Root->SetArrayField(TEXT("zones"), Zones);

	const TSharedRef<FJsonObject> Focus = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Groups;
	for (const FVibH2OFocusGroup& Group : FocusGroups)
	{
		Groups.Add(MakeShared<FJsonValueString>(Group.GroupName.ToString()));
	}
	Focus->SetArrayField(TEXT("groups"), Groups);
	Focus->SetStringField(TEXT("active"), ActiveFocusGroup.IsNone() ? FString() : ActiveFocusGroup.ToString());
	Focus->SetNumberField(TEXT("amount"), FocusAmount);
	Root->SetObjectField(TEXT("focus"), Focus);

	TArray<TSharedPtr<FJsonValue>> Effects;
	if (const UVibH2OSubsystem* Subsystem = GetSubsystem())
	{
		for (const FName& Effect : Subsystem->GetKnownEffects())
		{
			const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetStringField(TEXT("id"), Effect.ToString());
			Entry->SetBoolField(TEXT("on"), Subsystem->IsEffectEnabled(Effect));
			Effects.Add(MakeShared<FJsonValueObject>(Entry));
		}
		Root->SetBoolField(TEXT("oscIgnored"), Subsystem->bIgnoreOscControl);
	}
	Root->SetArrayField(TEXT("effects"), Effects);

	FString Out;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
	FJsonSerializer::Serialize(Root, Writer);
	return Out;
}

// ------------------------------------------------------------------- Focus

void AVibH2OStageActor::SetActiveFocusGroup(FName GroupName)
{
	if (ActiveFocusGroup == GroupName)
	{
		return;
	}
	ActiveFocusGroup = GroupName;
	UE_LOG(LogVibH2O, Log, TEXT("VibH2O: groupe de focus '%s'."), *GroupName.ToString());
	OnFocusGroupChanged.Broadcast(GroupName);
}

void AVibH2OStageActor::SelectFocusGroupByIndex(int32 Index)
{
	if (FocusGroups.IsValidIndex(Index))
	{
		SetActiveFocusGroup(FocusGroups[Index].GroupName);
	}
	else
	{
		SetActiveFocusGroup(NAME_None);
	}
}

void AVibH2OStageActor::CycleFocusGroup(int32 Delta)
{
	if (FocusGroups.Num() == 0)
	{
		return;
	}

	int32 Current = INDEX_NONE;
	for (int32 Index = 0; Index < FocusGroups.Num(); ++Index)
	{
		if (FocusGroups[Index].GroupName == ActiveFocusGroup)
		{
			Current = Index;
			break;
		}
	}

	// The cycle also passes through "no group", so everything can be switched
	// off from the same key.
	const int32 Count = FocusGroups.Num() + 1;
	const int32 Next = ((Current + 1) + Delta % Count + Count) % Count;
	SelectFocusGroupByIndex(Next - 1);
}

const FVibH2OFocusGroup* AVibH2OStageActor::FindFocusGroup(FName GroupName) const
{
	if (GroupName.IsNone())
	{
		return nullptr;
	}
	for (const FVibH2OFocusGroup& Group : FocusGroups)
	{
		if (Group.GroupName == GroupName)
		{
			return &Group;
		}
	}
	return nullptr;
}

float AVibH2OStageActor::ComputeFocusMask(const FVibH2OSeat& Seat) const
{
	const float Amount = FMath::Clamp(FocusAmount, 0.0f, 1.0f);
	if (Amount <= 0.0f)
	{
		return 1.0f;
	}

	const FVibH2OFocusGroup* Group = FindFocusGroup(ActiveFocusGroup);
	if (Group == nullptr)
	{
		return 1.0f;
	}

	if (Group->Contains(Seat))
	{
		return 1.0f;
	}
	// Gradual dimming: nothing moves, the room plan stays readable.
	return FMath::Lerp(1.0f, FMath::Clamp(FocusDimmedValue, 0.0f, 1.0f), Amount);
}

FBox AVibH2OStageActor::GetFocusBounds() const
{
	return ComputeBubbleBounds(FindFocusGroup(ActiveFocusGroup));
}

FBox AVibH2OStageActor::GetRoomBounds() const
{
	return ComputeBubbleBounds(nullptr);
}

FBox AVibH2OStageActor::ComputeBubbleBounds(const FVibH2OFocusGroup* Group) const
{
	FBox Bounds(ForceInit);

	for (const FVibH2OSeat& Seat : Room.Seats)
	{
		if (!Seat.bOccupied)
		{
			// An empty seat drops out of the framing bounds, otherwise the
			// camera would aim at nothing.
			continue;
		}
		if (Group != nullptr && !Group->Contains(Seat))
		{
			continue;
		}

		const TObjectPtr<AVibH2OBubbleActor>* Found = Bubbles.Find(Seat.SeatIndex);
		if (Found == nullptr || !IsValid(*Found))
		{
			continue;
		}
		// The bubble's REAL world position. Never an assumed flat grid: that
		// would aim beside the target the moment the room is curved or the
		// actor moved.
		Bounds += (*Found)->GetActorLocation();
	}

	if (!Bounds.IsValid)
	{
		return FBox(GetActorLocation(), GetActorLocation());
	}

	// A little margin so the bubbles are not flush against the edge.
	const float Radius = ColumnSpacing * 0.5f * GetActorScale3D().GetMax();
	return Bounds.ExpandBy(FVector(Radius));
}

FVector AVibH2OStageActor::GetFocusCenter() const
{
	return GetFocusBounds().GetCenter();
}

float AVibH2OStageActor::GetFocusFitDistance(float HorizontalFOV) const
{
	const FBox Bounds = GetFocusBounds();
	const float SphereRadius = static_cast<float>(Bounds.GetExtent().Size());
	if (SphereRadius <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	// We frame the bounding sphere rather than the box: a sphere does not
	// depend on camera orientation, so the distance stays correct from any
	// viewing angle.
	const float HalfFovRad = FMath::DegreesToRadians(FMath::Clamp(HorizontalFOV, 1.0f, 179.0f) * 0.5f);
	const float SinHalf = FMath::Sin(HalfFovRad);
	if (SinHalf <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	return (SphereRadius / SinHalf) * FMath::Max(FocusFitMargin, 1.0f);
}

// ------------------------------------------------------------------ Sundry

// -------------------------------------------------------------------- Mood

FVibH2OMoodTrackerSettings AVibH2OStageActor::MakeMoodSettings() const
{
	FVibH2OMoodTrackerSettings Settings;
	Settings.CalmThreshold = CalmThreshold;
	Settings.ExcitedThreshold = ExcitedThreshold;
	Settings.Hysteresis = MoodHysteresis;
	Settings.DetectionSmoothingSeconds = MoodDetectionSmoothing;
	Settings.TransitionSeconds = MoodTransitionSeconds;
	Settings.Override = MoodOverride;
	return Settings;
}

void AVibH2OStageActor::UpdateMood(const UVibH2OSubsystem* Subsystem, float DeltaSeconds)
{
	const FVibH2OMoodTrackerSettings Settings = MakeMoodSettings();
	// The mean the subsystem keeps already leaves out empty seats and quiet
	// sensors. With nobody counting, there is no room to judge.
	const bool bHasData = Subsystem != nullptr && Subsystem->GetActiveIndividualCount() > 0;
	const float Mean = Subsystem != nullptr ? Subsystem->GetAverageExcitation() : 0.0f;

	const EVibH2OMood Previous = MoodTracker.GetMood();
	const bool bChanged = MoodTracker.Update(Mean, bHasData, DeltaSeconds, Settings);
	CurrentMood = MoodTracker.GetMood();
	RoomExcitation = MoodTracker.GetSmoothedExcitation();
	if (bChanged)
	{
		UE_LOG(LogVibH2O, Log, TEXT("VibH2O: la salle passe de '%s' a '%s' (excitation %.2f)."),
			*UEnum::GetDisplayValueAsText(Previous).ToString(),
			*UEnum::GetDisplayValueAsText(CurrentMood).ToString(),
			RoomExcitation);
		OnMoodChanged.Broadcast(CurrentMood, Previous);
	}

	BlendedLook = MoodPalette.Blend(MoodTracker.GetWeights());

	const float Dt = FMath::Max(DeltaSeconds, 0.0f);
	FloatClock += Dt * BlendedLook.FloatSpeedScale;
	RoomStripePhase = FMath::Frac(RoomStripePhase + Dt * BlendedLook.StripeSpeed);
}

void AVibH2OStageActor::ApplyMoodPreset(UVibH2OMoodPreset* Preset)
{
	if (Preset == nullptr)
	{
		return;
	}
	MoodPreset = Preset;
	MoodPalette = Preset->Palette;
	UE_LOG(LogVibH2O, Log, TEXT("VibH2O: preset d'ambiance '%s' charge."), *Preset->GetName());
}

void AVibH2OStageActor::SetMoodOverride(EVibH2OMoodOverride NewOverride)
{
	MoodOverride = NewOverride;
}

void AVibH2OStageActor::SnapMood()
{
	const UVibH2OSubsystem* Subsystem = GetSubsystem();
	const float Mean = Subsystem != nullptr ? Subsystem->GetAverageExcitation() : RoomExcitation;
	MoodTracker.Snap(Mean, MakeMoodSettings());
	CurrentMood = MoodTracker.GetMood();
	RoomExcitation = MoodTracker.GetSmoothedExcitation();
	BlendedLook = MoodPalette.Blend(MoodTracker.GetWeights());
}

void AVibH2OStageActor::SaveMoodPreset()
{
	if (MoodPreset == nullptr)
	{
		UE_LOG(LogVibH2O, Warning, TEXT("VibH2O: aucun preset d'ambiance choisi, rien a enregistrer."));
		return;
	}
	MoodPreset->Modify();
	MoodPreset->Palette = MoodPalette;
	MoodPreset->MarkPackageDirty();
	UE_LOG(LogVibH2O, Log, TEXT("VibH2O: palettes enregistrees dans '%s' (a sauvegarder avec l'asset)."), *MoodPreset->GetName());
}

void AVibH2OStageActor::ReloadMoodPreset()
{
	ApplyMoodPreset(MoodPreset);
}

AVibH2OBubbleActor* AVibH2OStageActor::GetBubbleAtSeat(int32 InSeatIndex) const
{
	const TObjectPtr<AVibH2OBubbleActor>* Found = Bubbles.Find(InSeatIndex);
	return (Found && IsValid(*Found)) ? Found->Get() : nullptr;
}

AVibH2OBubbleActor* AVibH2OStageActor::GetBubbleForIndividual(int32 InIndividualId) const
{
	for (const TPair<int32, TObjectPtr<AVibH2OBubbleActor>>& Entry : Bubbles)
	{
		if (IsValid(Entry.Value) && Entry.Value->IndividualId == InIndividualId)
		{
			return Entry.Value.Get();
		}
	}
	return nullptr;
}

TArray<AVibH2OBubbleActor*> AVibH2OStageActor::GetBubbles() const
{
	TArray<AVibH2OBubbleActor*> Result;
	Result.Reserve(Bubbles.Num());
	for (const TPair<int32, TObjectPtr<AVibH2OBubbleActor>>& Entry : Bubbles)
	{
		if (IsValid(Entry.Value))
		{
			Result.Add(Entry.Value.Get());
		}
	}
	return Result;
}

// ---------------------------------------------------------- Editor preview

void AVibH2OStageActor::DrawEditorPreview() const
{
#if ENABLE_DRAW_DEBUG
	const UWorld* World = GetWorld();
	if (World == nullptr || StageRoot == nullptr)
	{
		return;
	}

	const int32 Columns = FMath::Max(PreviewColumns, 1);
	const int32 Rows = FMath::Max(PreviewRows, 1);
	const FVibH2OGridParams Params = MakeGridParams(Columns, Rows);
	const FTransform StageTransform = StageRoot->GetComponentTransform();

	const float MinSpacing = FMath::Min(ColumnSpacing, RowSpacing);
	const float MarkerRadius = FMath::Max(MinSpacing * 0.12f, 2.0f);

	FVibH2OFloatParams FloatParams;
	FloatParams.Amplitude = FloatAmplitude;
	FloatParams.Speed = FloatSpeed;
	FloatParams.RotationAmount = FloatRotationAmount;

	for (int32 Row = 0; Row < Rows; ++Row)
	{
		for (int32 Column = 0; Column < Columns; ++Column)
		{
			const int32 SeatIndex = FVibH2OLayoutMath::ColumnRowToSeatIndex(Column, Row, Columns, Rows, EVibH2OSeatOrder::ColumnMajor);
			const FVibH2OSeatLayout Layout = FVibH2OLayoutMath::EvaluateGrid(Params, Column, Row);

			FVector LocalLocation = Layout.Location;
			FQuat LocalRotation = Layout.Rotation.Quaternion();
			float Radius = MarkerRadius;

			if (bPreviewAnimate)
			{
				// The same float the real bubbles use, so what is dialled in
				// here is what will be seen in game.
				if (FloatAmplitude > 0.0f)
				{
					LocalLocation += FVibH2OLayoutMath::EvaluateFloatOffset(FloatParams, SeatIndex, StageTime);
				}
				if (FloatRotationAmount > 0.0f)
				{
					LocalRotation = LocalRotation * FVibH2OLayoutMath::EvaluateFloatRotation(FloatParams, SeatIndex, StageTime).Quaternion();
				}

				// A plausible heartbeat per seat, invented here and nowhere
				// else: the editor has no data. It only makes the preview
				// breathe while the geometry is being adjusted.
				const float FakeBpm = FMath::Lerp(58.0f, 92.0f, FVibH2ONoise::UnitFloat(SeatIndex * 17 + 3));
				const float Phase = FMath::Frac(FVibH2ONoise::UnitFloat(SeatIndex * 31 + 7) + StageTime * (FakeBpm / 60.0f));
				const float Pulse = FMath::Pow(1.0f - Phase, 4.0f);
				Radius *= 1.0f + Pulse * 0.35f;
			}

			const FVector WorldLocation = StageTransform.TransformPosition(LocalLocation);

			// The first row in a lighter tone: with no landmark, a room curved
			// one way looks just like a room curved the other.
			const FColor Color = (Row == 0) ? FColor(120, 220, 255) : FColor(60, 130, 190);
			DrawDebugSphere(World, WorldLocation, Radius, 8, Color, false, -1.0f, SDPG_Foreground, 1.0f);

			if (bPreviewShowOrientation)
			{
				// The arrow is the only way to check bOrientToCenter: on
				// spheres, a rotation simply does not show.
				const FQuat WorldRotation = StageTransform.TransformRotation(LocalRotation);
				const FVector Forward = WorldRotation.GetForwardVector() * MinSpacing * 0.4f;
				DrawDebugDirectionalArrow(World, WorldLocation, WorldLocation + Forward,
					MarkerRadius * 2.0f, FColor(255, 180, 60), false, -1.0f, SDPG_Foreground, 1.0f);
			}
		}
	}

	DrawDebugSphere(World, StageTransform.GetLocation(), MarkerRadius * 1.5f, 12, FColor::Red, false, -1.0f, SDPG_Foreground, 2.0f);
#endif
}
