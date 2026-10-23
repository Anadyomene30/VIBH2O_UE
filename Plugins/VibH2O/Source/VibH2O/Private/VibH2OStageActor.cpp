#include "VibH2OStageActor.h"

#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Layout/VibH2OLayoutMath.h"
#include "Layout/VibH2OSchoolMath.h"
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
	}
	BoundSubsystem.Reset();
}

void AVibH2OStageActor::DestroyBubbles()
{
	DestroyDaughters();
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

		AVibH2OBubbleActor* Bubble = SpawnBubbleActor(World, SpawnClass);
		if (Bubble == nullptr)
		{
			continue;
		}
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

	ReconcileDaughters();

	OnRoomBuilt.Broadcast(Bubbles.Num());
}

AVibH2OBubbleActor* AVibH2OStageActor::SpawnBubbleActor(UWorld* World, const UClass* SpawnClass)
{
	if (World == nullptr || SpawnClass == nullptr || StageRoot == nullptr)
	{
		return nullptr;
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
		return nullptr;
	}

	// Attaching to the stage root is what makes moving, rotating or scaling
	// the actor carry the whole room along with it.
	//
	// SnapToTarget, not KeepRelative: an unparented actor's relative
	// transform equals its world transform, so keeping it would place the
	// bubble at twice the stage offset for one frame before Tick corrects
	// it. Snapping starts it at the root, where the drivers expect it.
	Bubble->AttachToComponent(StageRoot, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	return Bubble;
}

// ---------------------------------------------------------------- Division

void AVibH2OStageActor::ReconcileDaughters()
{
	UWorld* World = GetWorld();
	const UClass* SpawnClass = BubbleClass ? BubbleClass.Get() : AVibH2OBubbleActor::StaticClass();

	const int32 Wanted = FVibH2OSchoolMath::ComputeFamilySize(DaughtersPerBubble, Bubbles.Num(), MaxSchoolSize);
	const int32 WantedDaughters = Wanted - 1;

	// A family whose mother has gone goes with her.
	for (auto It = Families.CreateIterator(); It; ++It)
	{
		if (!Bubbles.Contains(It.Key()))
		{
			for (const TObjectPtr<AVibH2OBubbleActor>& Daughter : It.Value().Daughters)
			{
				if (IsValid(Daughter))
				{
					Daughter->Destroy();
				}
			}
			It.RemoveCurrent();
		}
	}

	int32 Born = 0;
	for (const FVibH2OSeat& Seat : Room.Seats)
	{
		if (!Seat.bOccupied || !Bubbles.Contains(Seat.SeatIndex))
		{
			continue;
		}

		TArray<TObjectPtr<AVibH2OBubbleActor>>& Daughters = Families.FindOrAdd(Seat.SeatIndex).Daughters;
		Daughters.RemoveAll([](const TObjectPtr<AVibH2OBubbleActor>& Daughter) { return !IsValid(Daughter); });

		while (Daughters.Num() > WantedDaughters)
		{
			Daughters.Pop()->Destroy();
		}

		// Same rule as the mothers: an unchanged daughter stays, so a resend of
		// the room from Max does not make the school flicker.
		for (int32 Index = 0; Index < Daughters.Num(); ++Index)
		{
			if (Daughters[Index]->IndividualId != Seat.IndividualId)
			{
				Daughters[Index]->AssignDaughter(Seat, Index + 1);
			}
		}

		while (Daughters.Num() < WantedDaughters)
		{
			AVibH2OBubbleActor* Daughter = SpawnBubbleActor(World, SpawnClass);
			if (Daughter == nullptr)
			{
				break;
			}
			Daughter->AssignDaughter(Seat, Daughters.Num() + 1);
			// Born tucked away, whatever the division: the next frame places
			// her. Hiding every daughter here instead would blink the whole
			// school each time Max resends the room.
			TuckAway(Daughter);
			Daughters.Add(Daughter);
			++Born;
		}
	}

	if (Born > 0 || FamilySize != Wanted)
	{
		UE_LOG(LogVibH2O, Log, TEXT("VibH2O: %d filles par bulle (%d nees), banc de %d bulles."),
			WantedDaughters, Born, Bubbles.Num() * Wanted);
	}
	FamilySize = Wanted;
}

void AVibH2OStageActor::TuckAway(AVibH2OBubbleActor* Daughter)
{
	if (!IsValid(Daughter))
	{
		return;
	}
	Daughter->SetActorHiddenInGame(true);
	// Hidden in game is not hidden in the editor viewport: folded back to a
	// speck at the stage root, a daughter is invisible in both.
	Daughter->SetActorRelativeLocation(FVector::ZeroVector);
	Daughter->SetActorRelativeScale3D(FVector(0.001f));
}

void AVibH2OStageActor::DestroyDaughters()
{
	for (const TPair<int32, FVibH2OBubbleFamily>& Entry : Families)
	{
		for (const TObjectPtr<AVibH2OBubbleActor>& Daughter : Entry.Value.Daughters)
		{
			if (IsValid(Daughter))
			{
				Daughter->Destroy();
			}
		}
	}
	Families.Reset();
	FamilySize = 1;
	bDaughtersTucked = false;
}

void AVibH2OStageActor::TuckAwayDaughters()
{
	for (const TPair<int32, FVibH2OBubbleFamily>& Entry : Families)
	{
		for (const TObjectPtr<AVibH2OBubbleActor>& Daughter : Entry.Value.Daughters)
		{
			TuckAway(Daughter);
		}
	}
	bDaughtersTucked = true;
}

float AVibH2OStageActor::GetSchoolOrder() const
{
	const UVibH2OFlockDriver* Flock = Cast<UVibH2OFlockDriver>(FlockDriver);
	return Flock ? Flock->GetSchoolOrder() : 0.0f;
}

float AVibH2OStageActor::GetDivisionProgress() const
{
	if (FamilySize <= 1)
	{
		return 0.0f;
	}
	const float RawBlend = FMath::Clamp(BlendAlpha, 0.0f, 1.0f);
	const float Blend = BlendCurve ? FMath::Clamp(BlendCurve->GetFloatValue(RawBlend), 0.0f, 1.0f) : RawBlend;
	return FVibH2OSchoolMath::ComputeDivisionProgress(Blend, DivisionStart, DivisionEnd);
}

TArray<AVibH2OBubbleActor*> AVibH2OStageActor::GetDaughters() const
{
	TArray<AVibH2OBubbleActor*> Result;
	for (const TPair<int32, FVibH2OBubbleFamily>& Entry : Families)
	{
		for (const TObjectPtr<AVibH2OBubbleActor>& Daughter : Entry.Value.Daughters)
		{
			if (IsValid(Daughter))
			{
				Result.Add(Daughter.Get());
			}
		}
	}
	return Result;
}

TArray<AVibH2OBubbleActor*> AVibH2OStageActor::GetDaughtersOfSeat(int32 InSeatIndex) const
{
	TArray<AVibH2OBubbleActor*> Result;
	if (const FVibH2OBubbleFamily* Family = Families.Find(InSeatIndex))
	{
		for (const TObjectPtr<AVibH2OBubbleActor>& Daughter : Family->Daughters)
		{
			if (IsValid(Daughter))
			{
				Result.Add(Daughter.Get());
			}
		}
	}
	return Result;
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

	// The families follow their settings and the room, even when the count is
	// changed from a Blueprint mid-show.
	if (FVibH2OSchoolMath::ComputeFamilySize(DaughtersPerBubble, Bubbles.Num(), MaxSchoolSize) != FamilySize)
	{
		ReconcileDaughters();
	}

	// The division is a pure function of the blend: no state, so the morph runs
	// backwards as well as forwards and the sisters fold back into their mother.
	const float DivisionProgress = (FamilySize > 1)
		? FVibH2OSchoolMath::ComputeDivisionProgress(Blend, DivisionStart, DivisionEnd)
		: 0.0f;
	const float Migration = FVibH2OSchoolMath::ComputeMigrationBlend(Blend, DivisionStart, DivisionEnd, FamilySize);
	const bool bShowDaughters = DivisionProgress > 0.0f;
	if (!bShowDaughters && !bDaughtersTucked)
	{
		TuckAwayDaughters();
	}
	else if (bShowDaughters)
	{
		bDaughtersTucked = false;
	}

	// Drivers that carry state step forward once, as a whole, before anyone
	// asks where a single bubble is. A school has to move as a school.
	FVibH2OFrameContext Frame;
	Frame.Stage = this;
	Frame.DeltaSeconds = DeltaSeconds;
	Frame.TimeSeconds = StageTime;
	Frame.OccupiedCount = OccupiedCount;
	Frame.FamilySize = FamilySize;
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

	// Every body on stage: each mother, followed by her daughters while the
	// division shows. A daughter goes through exactly the same stack as her
	// mother - same data, same layers - only her seed and family pose differ.
	struct FBody
	{
		const FVibH2OSeat* Seat = nullptr;
		AVibH2OBubbleActor* Bubble = nullptr;
		int32 Member = 0;
	};
	TArray<FBody> Bodies;
	Bodies.Reserve(Bubbles.Num() * (bShowDaughters ? FamilySize : 1));
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
		Bodies.Add({ &Seat, Found->Get(), 0 });

		const FVibH2OBubbleFamily* Family = bShowDaughters ? Families.Find(Seat.SeatIndex) : nullptr;
		if (Family != nullptr)
		{
			for (int32 Index = 0; Index < Family->Daughters.Num(); ++Index)
			{
				if (IsValid(Family->Daughters[Index]))
				{
					Bodies.Add({ &Seat, Family->Daughters[Index].Get(), Index + 1 });
				}
			}
		}
	}

	for (const FBody& Body : Bodies)
	{
		const FVibH2OSeat& Seat = *Body.Seat;
		AVibH2OBubbleActor* Bubble = Body.Bubble;

		const FVibH2OIndividualState* StatePtr = Subsystem->FindIndividual(Seat.IndividualId);
		const FVibH2OIndividualState& State = StatePtr ? *StatePtr : EmptyState;

		FVibH2OLayoutContext Context;
		Context.Stage = this;
		Context.Seat = Seat;
		Context.Columns = Room.Columns;
		Context.Rows = Room.Rows;
		Context.OccupiedOrdinal = Subsystem->GetOccupiedOrdinal(Seat.SeatIndex);
		Context.OccupiedCount = OccupiedCount;
		Context.FamilyMember = Body.Member;
		Context.FamilySize = FamilySize;
		Context.Individual = State;
		Context.CollectiveSynchrony = CollectiveSynchrony;
		Context.TimeSeconds = StageTime;
		// The seed derives from the seat index, so relief and phases stay
		// stable from one launch to the next. A daughter's is derived from it.
		Context.Seed = FVibH2OSchoolMath::MakeMemberSeed(Seat.SeatIndex, Body.Member);

		// Where this sister sits in her family, and how big she is. Identity
		// for the mother while the division has not begun.
		const FVibH2ODivisionPose Division = FVibH2OSchoolMath::EvaluateDivision(
			Body.Member, FamilySize, Seat.SeatIndex, DivisionProgress, DivisionSpread);

		// The position stack, as described in docs/SPEC.md section E. The
		// family divides around the seat, then travels to the school.
		FTransform GridTransform = FTransform::Identity;
		if (GridDriver != nullptr)
		{
			GridTransform = GridDriver->EvaluateSeat(Context);
		}

		FVector Location = GridTransform.GetLocation() + Division.Offset;
		FQuat Rotation = GridTransform.GetRotation();

		if (Migration > 0.0f && FlockDriver != nullptr)
		{
			const FTransform FlockTransform = FlockDriver->EvaluateSeat(Context);
			Location = FMath::Lerp(Location, FlockTransform.GetLocation(), Migration);
			Rotation = FQuat::Slerp(Rotation, FlockTransform.GetRotation(), Migration);
			Rotation.Normalize();
		}

		// A daughter floats and drifts as her mother does while still nested
		// in her, then gradually on her own: otherwise the sisters would jiggle
		// out of the mother before she has finished dividing.
		const int32 MotherSeed = Seat.SeatIndex;
		const float Own = (Body.Member > 0) ? Division.Individuation : 1.0f;

		// The float is added AFTER the blend, and not inside the grid driver:
		// it is a property of the bubble - it is underwater, so it floats - and
		// not of the tableau. That is how it survives the move to the vortex.
		if (bFloatEnabled && FloatAmplitude > 0.0f)
		{
			FVector Float = FVibH2OLayoutMath::EvaluateFloatOffset(FloatParams, Context.Seed, StageTime);
			if (Own < 1.0f)
			{
				Float = FMath::Lerp(FVibH2OLayoutMath::EvaluateFloatOffset(FloatParams, MotherSeed, StageTime), Float, Own);
			}
			Location += Float;
		}
		if (bFloatEnabled && FloatRotationAmount > 0.0f)
		{
			Rotation = Rotation * FVibH2OLayoutMath::EvaluateFloatRotation(FloatParams, Context.Seed, StageTime).Quaternion();
		}

		// The drift belongs to the person too, not to the tableau: it follows
		// arousal, and stays bounded by construction.
		if (bDriftEnabled && DriftIntensity > 0.0f)
		{
			FVector Drift = FVibH2OLayoutMath::EvaluateDriftOffset(DriftParams, Context.Seed, StageTime, State.Excitation, MinSpacing);
			if (Own < 1.0f)
			{
				Drift = FMath::Lerp(FVibH2OLayoutMath::EvaluateDriftOffset(DriftParams, MotherSeed, StageTime, State.Excitation, MinSpacing), Drift, Own);
			}
			Location += Drift;
		}

		// The division's share of the volume is structure, not an effect: it
		// applies whether or not the scale effect is on.
		float Scale = BaseScale * Division.ScaleFactor;
		if (bScaleEnabled)
		{
			Scale *= 1.0f + State.Excitation * ScalePerExcitation + State.BeatPulse * BeatPopAmount;
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
			const float Amount = OrganicDeform * (0.7f + State.Excitation * 0.6f);
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
		if (Body.Member > 0)
		{
			Bubble->SetActorHiddenInGame(false);
		}

		const float Mask = bFocusEnabled ? ComputeFocusMask(Seat) : 1.0f;
		const FLinearColor Tint = bTintEnabled ? ComputeTint(State.Excitation) : CalmColor;

		// One number per spectator: on the mother only.
		Bubble->SetSeatLabelVisible(bShowSeatNumbers && Body.Member == 0);
		if (bShowSeatNumbers)
		{
			Bubble->OrientSeatLabel(ViewLocation);
		}

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
