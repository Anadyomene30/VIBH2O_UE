#include "VibH2OStageActor.h"

#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Layout/VibH2OLayoutMath.h"
#include "VibH2OBubbleActor.h"
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
	FlockDriver = CreateDefaultSubobject<UVibH2OVortexDriver>(TEXT("FlockDriver"));
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
		Subsystem->OnRoomChanged.AddDynamic(this, &AVibH2OStageActor::HandleRoomChanged);
		Subsystem->OnOscBlendAlphaChanged.AddDynamic(this, &AVibH2OStageActor::HandleOscBlendAlpha);
		Subsystem->OnOscFocusAmountChanged.AddDynamic(this, &AVibH2OStageActor::HandleOscFocusAmount);
		Subsystem->OnOscFocusGroupChanged.AddDynamic(this, &AVibH2OStageActor::HandleOscFocusGroup);
		bBoundToSubsystem = true;

		// A room may have arrived before this actor started up.
		if (Subsystem->HasRoom())
		{
			HandleRoomChanged(Subsystem->GetRoomRef());
		}
	}
	else
	{
		UE_LOG(LogVibH2O, Warning, TEXT("VibH2O: acteur de scene '%s' sans subsystem — aucune bulle ne sera creee."), *GetName());
	}
}

void AVibH2OStageActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bBoundToSubsystem)
	{
		if (UVibH2OSubsystem* Subsystem = GetSubsystem())
		{
			Subsystem->OnRoomChanged.RemoveDynamic(this, &AVibH2OStageActor::HandleRoomChanged);
			Subsystem->OnOscBlendAlphaChanged.RemoveDynamic(this, &AVibH2OStageActor::HandleOscBlendAlpha);
			Subsystem->OnOscFocusAmountChanged.RemoveDynamic(this, &AVibH2OStageActor::HandleOscFocusAmount);
			Subsystem->OnOscFocusGroupChanged.RemoveDynamic(this, &AVibH2OStageActor::HandleOscFocusGroup);
		}
		bBoundToSubsystem = false;
	}

	for (const TPair<int32, TObjectPtr<AVibH2OBubbleActor>>& Entry : Bubbles)
	{
		if (IsValid(Entry.Value))
		{
			Entry.Value->Destroy();
		}
	}
	Bubbles.Reset();

	Super::EndPlay(EndPlayReason);
}

bool AVibH2OStageActor::ShouldTickIfViewportsOnly() const
{
	// Ticking in the editor serves only to draw the preview: no bubble is
	// instanced there.
	return bPreviewInEditor;
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
		for (const TPair<int32, TObjectPtr<AVibH2OBubbleActor>>& Entry : Bubbles)
		{
			if (IsValid(Entry.Value))
			{
				Entry.Value->Destroy();
			}
		}
		Bubbles.Reset();
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

	if (GetWorld() != nullptr && GetWorld()->IsGameWorld())
	{
		UpdateBubbles(DeltaSeconds);
	}
#if WITH_EDITOR
	else if (bPreviewInEditor)
	{
		DrawEditorPreview();
	}
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
	if (Subsystem == nullptr || Bubbles.Num() == 0)
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

	FVibH2OFloatParams FloatParams;
	FloatParams.Amplitude = FloatAmplitude;
	FloatParams.Speed = FloatSpeed;
	FloatParams.RotationAmount = FloatRotationAmount;

	FVibH2ODriftParams DriftParams;
	DriftParams.Threshold = DriftThreshold;
	DriftParams.Intensity = DriftIntensity;
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

		// The float is added AFTER the blend, and not inside the grid driver:
		// it is a property of the bubble - it is underwater, so it floats - and
		// not of the tableau. That is how it survives the move to the vortex.
		if (bFloatEnabled && FloatAmplitude > 0.0f)
		{
			Location += FVibH2OLayoutMath::EvaluateFloatOffset(FloatParams, Context.Seed, StageTime);
		}
		if (bFloatEnabled && FloatRotationAmount > 0.0f)
		{
			Rotation = Rotation * FVibH2OLayoutMath::EvaluateFloatRotation(FloatParams, Context.Seed, StageTime).Quaternion();
		}

		// The drift belongs to the person too, not to the tableau: it follows
		// arousal, and stays bounded by construction.
		if (bDriftEnabled && DriftIntensity > 0.0f)
		{
			Location += FVibH2OLayoutMath::EvaluateDriftOffset(DriftParams, Context.Seed, StageTime, State.Excitation, MinSpacing);
		}

		float Scale = BaseScale;
		if (bScaleEnabled)
		{
			Scale *= 1.0f + State.Excitation * ScalePerExcitation + State.BeatPulse * BeatPopAmount;
		}

		Bubble->SetActorRelativeLocation(Location);
		Bubble->SetActorRelativeRotation(Rotation);
		Bubble->SetActorRelativeScale3D(FVector(Scale));

		const float Mask = bFocusEnabled ? ComputeFocusMask(Seat) : 1.0f;
		const FLinearColor Tint = bTintEnabled ? ComputeTint(State.Excitation) : CalmColor;

		Bubble->UpdateFromState(State, Mask, Tint, CollectiveSynchrony, Blend, StriationSpeedScale, DeltaSeconds);
	}
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
	FBox Bounds(ForceInit);

	const FVibH2OFocusGroup* Group = FindFocusGroup(ActiveFocusGroup);

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

FLinearColor AVibH2OStageActor::ComputeTint(float Excitation) const
{
	const float T = FMath::Clamp(Excitation, 0.0f, 1.0f);
	if (ExcitationGradient != nullptr)
	{
		return ExcitationGradient->GetLinearColorValue(T);
	}
	return FMath::Lerp(CalmColor, ExcitedColor, T);
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

	for (int32 Row = 0; Row < Rows; ++Row)
	{
		for (int32 Column = 0; Column < Columns; ++Column)
		{
			const FVibH2OSeatLayout Layout = FVibH2OLayoutMath::EvaluateGrid(Params, Column, Row);
			const FVector WorldLocation = StageTransform.TransformPosition(Layout.Location);

			// The first row in a lighter tone: with no landmark, a room curved
			// one way looks just like a room curved the other.
			const FColor Color = (Row == 0) ? FColor(120, 220, 255) : FColor(60, 130, 190);
			DrawDebugSphere(World, WorldLocation, MarkerRadius, 8, Color, false, -1.0f, SDPG_Foreground, 1.0f);

			if (bPreviewShowOrientation)
			{
				// The arrow is the only way to check bOrientToCenter: on
				// spheres, a rotation simply does not show.
				const FQuat WorldRotation = StageTransform.TransformRotation(Layout.Rotation.Quaternion());
				const FVector Forward = WorldRotation.GetForwardVector() * MinSpacing * 0.4f;
				DrawDebugDirectionalArrow(World, WorldLocation, WorldLocation + Forward,
					MarkerRadius * 2.0f, FColor(255, 180, 60), false, -1.0f, SDPG_Foreground, 1.0f);
			}
		}
	}

	DrawDebugSphere(World, StageTransform.GetLocation(), MarkerRadius * 1.5f, 12, FColor::Red, false, -1.0f, SDPG_Foreground, 2.0f);
#endif
}
