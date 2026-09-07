#include "VibH2OSimulatorActor.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Layout/VibH2OLayoutMath.h"
#include "Osc/VibH2OOscTypes.h"
#include "VibH2OModule.h"
#include "VibH2OSettings.h"
#include "VibH2OSubsystem.h"

namespace VibH2OSimulatorInternal
{
	/** Network silence required before the simulator takes the stage back. */
	static constexpr double RealDataHoldSeconds = 3.0;

	static FVibH2OOscMessage MakeIntMessage(const FString& Address, int32 Value)
	{
		FVibH2OOscMessage Message(Address);
		Message.Args.Add(FVibH2OOscValue::MakeInt(Value));
		return Message;
	}

	static FVibH2OOscMessage MakeFloatMessage(const FString& Address, float Value)
	{
		FVibH2OOscMessage Message(Address);
		Message.Args.Add(FVibH2OOscValue::MakeFloat(Value));
		return Message;
	}
}

AVibH2OSimulatorActor::AVibH2OSimulatorActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SimulatorRoot")));
}

UVibH2OSubsystem* AVibH2OSimulatorActor::GetSubsystem() const
{
	return UVibH2OSubsystem::Get(this);
}

void AVibH2OSimulatorActor::BeginPlay()
{
	Super::BeginPlay();

	if (bSimulateOnBeginPlay)
	{
		StartSimulation();
	}
}

void AVibH2OSimulatorActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopSimulation();
	Super::EndPlay(EndPlayReason);
}

void AVibH2OSimulatorActor::StartSimulation()
{
	if (bSimulating)
	{
		return;
	}
	if (GetSubsystem() == nullptr)
	{
		UE_LOG(LogVibH2O, Warning, TEXT("VibH2O: simulateur '%s' sans subsystem — rien a alimenter."), *GetName());
		return;
	}

	bSimulating = true;
	bAnnouncedFirstFeed = false;
	bReportedYield = false;
	SimTime = 0.0f;
	NextFeedTime = 0.0;
	NextRoomTime = (RoomResendInterval > 0.0f) ? (FPlatformTime::Seconds() + RoomResendInterval) : 0.0;

	// Prime the network-yield watermark so pre-existing traffic does not count.
	if (UVibH2OSubsystem* Subsystem = GetSubsystem())
	{
		LastNetworkMessageCount = Subsystem->GetNetworkStats().MessagesReceived;
	}

	SendRoomNow();

	UE_LOG(LogVibH2O, Display,
		TEXT("VibH2O: simulation demarree — salle %d x %d, scenario %s, %.1f Hz par individu."),
		Columns, Rows, *UEnum::GetDisplayValueAsText(Scenario).ToString(), SendRateHz);
}

void AVibH2OSimulatorActor::StopSimulation()
{
	if (!bSimulating)
	{
		return;
	}
	bSimulating = false;
	UE_LOG(LogVibH2O, Display, TEXT("VibH2O: simulation arretee."));
}

void AVibH2OSimulatorActor::ConfigureAles()
{
	// The real venue, read from RoomMapping_Presets/ALES_FINAL.json: 23 x 4
	// with two centre aisles. Non-square AND holed - the best test room there is.
	Columns = 23;
	Rows = 4;
	AisleColumns = { 6, 16 };
	ExtraEmptySeatIndices.Reset();

	if (bSimulating)
	{
		SendRoomNow();
	}
}

void AVibH2OSimulatorActor::SetRoomSize(int32 InColumns, int32 InRows)
{
	Columns = FMath::Clamp(InColumns, 1, 64);
	Rows = FMath::Clamp(InRows, 1, 32);
	if (bSimulating)
	{
		SendRoomNow();
	}
}

#if WITH_EDITOR
void AVibH2OSimulatorActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Typing a new column count in the Details panel resends the plan at once,
	// so the room resizes under the cursor instead of at the next restart.
	static const TSet<FName> RoomProperties = {
		GET_MEMBER_NAME_CHECKED(AVibH2OSimulatorActor, Columns),
		GET_MEMBER_NAME_CHECKED(AVibH2OSimulatorActor, Rows),
		GET_MEMBER_NAME_CHECKED(AVibH2OSimulatorActor, AisleColumns),
		GET_MEMBER_NAME_CHECKED(AVibH2OSimulatorActor, ExtraEmptySeatIndices)
	};

	if (bSimulating && RoomProperties.Contains(PropertyChangedEvent.GetPropertyName()))
	{
		SendRoomNow();
	}
}
#endif

bool AVibH2OSimulatorActor::IsSeatEmpty(int32 Column, int32 Row, int32 SeatIndex) const
{
	return AisleColumns.Contains(Column) || ExtraEmptySeatIndices.Contains(SeatIndex);
}

void AVibH2OSimulatorActor::SendRoomNow()
{
	UVibH2OSubsystem* Subsystem = GetSubsystem();
	if (Subsystem == nullptr)
	{
		return;
	}

	using namespace VibH2OSimulatorInternal;
	const UVibH2OSettings* Settings = UVibH2OSettings::Get();

	// Addresses are built from the project settings, not from literals: if the
	// prefixes are ever renamed to match a patch, the simulator follows.
	Subsystem->InjectOscMessage(MakeIntMessage(
		FString::Printf(TEXT("%s/%s/"), *Settings->RoomMappingPrefix, *Settings->ColumnsSegment), Columns));
	Subsystem->InjectOscMessage(MakeIntMessage(
		FString::Printf(TEXT("%s/%s/"), *Settings->RoomMappingPrefix, *Settings->RowsSegment), Rows));

	// Individuals are numbered in reading order - row by row, as the Max
	// interface displays them - while the OSC index follows the configured
	// seat order. That difference IS pitfall 4, reproduced faithfully.
	int32 NextId = 1;
	for (int32 Row = 0; Row < Rows; ++Row)
	{
		for (int32 Column = 0; Column < Columns; ++Column)
		{
			const int32 SeatIndex = FVibH2OLayoutMath::ColumnRowToSeatIndex(Column, Row, Columns, Rows, Settings->SeatOrder);
			if (SeatIndex <= 0)
			{
				continue;
			}
			const int32 Id = IsSeatEmpty(Column, Row, SeatIndex) ? 0 : NextId++;
			Subsystem->InjectOscMessage(MakeIntMessage(
				FString::Printf(TEXT("%s/%d/"), *Settings->RoomMappingPrefix, SeatIndex), Id));
		}
	}
}

void AVibH2OSimulatorActor::ScenarioTargets(int32 Column, int32 Row, int32 Id, float& OutExcitation, float& OutSynchrony) const
{
	const float T = SimTime * ScenarioSpeed;

	switch (Scenario)
	{
	case EVibH2OSimulatorScenario::Wave:
	{
		// A crest crossing the room from left to right, overshooting both
		// edges so it fully enters and fully leaves.
		const float Position = (Columns > 1) ? static_cast<float>(Column) / (Columns - 1) : 0.0f;
		const float Head = FMath::Fmod(T * 0.18f, 1.4f) - 0.2f;
		OutExcitation = FMath::Max(0.0f, 1.0f - FMath::Abs(Position - Head) * 5.0f);
		OutSynchrony = 0.5f;
		return;
	}
	case EVibH2OSimulatorScenario::SyncSweep:
		// The sweep that makes the vortex breathe: tight spiral at the top of
		// the cosine, scatter at the bottom.
		OutExcitation = 0.3f;
		OutSynchrony = 0.5f - 0.5f * FMath::Cos(T * 0.25f);
		return;

	case EVibH2OSimulatorScenario::Peak:
		OutExcitation = 0.5f - 0.5f * FMath::Cos(T * 0.4f);
		OutSynchrony = 0.4f + OutExcitation * 0.5f;
		return;

	case EVibH2OSimulatorScenario::Nominal:
	case EVibH2OSimulatorScenario::Dropout:
	default:
		OutExcitation = 0.25f + 0.15f * FMath::Sin(T * 0.3f + Column * 0.4f + Row * 0.7f);
		OutSynchrony = 0.55f;
		return;
	}
}

void AVibH2OSimulatorActor::FeedData()
{
	UVibH2OSubsystem* Subsystem = GetSubsystem();
	if (Subsystem == nullptr)
	{
		return;
	}

	using namespace VibH2OSimulatorInternal;
	const UVibH2OSettings* Settings = UVibH2OSettings::Get();

	// Dropout scenario: past the delay, the first N occupied individuals stop
	// talking. The plugin must turn them stale, and nothing else.
	const bool bDropoutActive =
		(Scenario == EVibH2OSimulatorScenario::Dropout) && (SimTime >= DropoutAfterSeconds);
	int32 DroppedSoFar = 0;

	int32 FedCount = 0;

	for (const FVibH2OSeat& Seat : Subsystem->GetRoomRef().Seats)
	{
		if (!Seat.bOccupied)
		{
			continue;
		}

		if (bDropoutActive && DroppedSoFar < DropoutCount)
		{
			++DroppedSoFar;
			continue;
		}

		const int32 Id = Seat.IndividualId;

		float Excitation = 0.0f;
		float Synchrony = 0.0f;
		ScenarioTargets(Seat.Column, Seat.Row, Id, Excitation, Synchrony);

		// A heart of this individual's own: deterministic resting rate drawn
		// from the seed, a slow breathing swell, and the pull of excitation.
		const float BaseBpm = FMath::Lerp(
			FMath::Min(BaseBpmMin, BaseBpmMax), FMath::Max(BaseBpmMin, BaseBpmMax),
			FVibH2ONoise::UnitFloat(Seed * 131 + Id * 17));
		const float BreathPhase = FVibH2ONoise::UnitFloat(Seed * 131 + Id * 17 + 1) * 2.0f * PI;
		float Bpm = BaseBpm
			+ FMath::Sin(SimTime * 0.35f + BreathPhase) * 3.0f
			+ Excitation * 28.0f;
		Bpm = FMath::Clamp(Bpm, 40.0f, 190.0f);

		Subsystem->InjectOscMessage(MakeFloatMessage(
			FString::Printf(TEXT("%s/%d/"), *Settings->BpmPrefix, Id), Bpm));
		Subsystem->InjectOscMessage(MakeFloatMessage(
			FString::Printf(TEXT("%s/%d/"), *Settings->ExcitationPrefix, Id), Excitation));
		Subsystem->InjectOscMessage(MakeFloatMessage(
			FString::Printf(TEXT("%s/%d/"), *Settings->SynchronyPrefix, Id), Synchrony));
		++FedCount;
	}

	if (!bAnnouncedFirstFeed && FedCount > 0)
	{
		bAnnouncedFirstFeed = true;
		UE_LOG(LogVibH2O, Display, TEXT("VibH2O: simulation — %d individus alimentes a %.1f Hz."), FedCount, SendRateHz);
	}
}

void AVibH2OSimulatorActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UWorld* World = GetWorld();
	if (!bSimulating || World == nullptr || !World->IsGameWorld())
	{
		return;
	}

	UVibH2OSubsystem* Subsystem = GetSubsystem();
	if (Subsystem == nullptr)
	{
		return;
	}

	const double Now = FPlatformTime::Seconds();

	// Step aside for the real thing. The simulator is a stand-in for Max, not
	// a competitor: the moment genuine packets come off the socket, feeding
	// pauses, and it resumes only after a few seconds of network silence.
	if (bYieldToRealNetwork && Subsystem->IsListening())
	{
		const int32 Received = Subsystem->GetNetworkStats().MessagesReceived;
		if (Received > LastNetworkMessageCount)
		{
			LastNetworkMessageCount = Received;
			RealDataHoldUntil = Now + VibH2OSimulatorInternal::RealDataHoldSeconds;
		}
		if (Now < RealDataHoldUntil)
		{
			if (!bReportedYield)
			{
				bReportedYield = true;
				UE_LOG(LogVibH2O, Display, TEXT("VibH2O: simulation en pause — de vraies donnees reseau arrivent."));
			}
			return;
		}
		if (bReportedYield)
		{
			bReportedYield = false;
			UE_LOG(LogVibH2O, Display, TEXT("VibH2O: simulation reprise — le reseau est silencieux."));
		}
	}

	// The scenario clock only advances while the simulator actually feeds:
	// pausing freezes the audience instead of teleporting it forward.
	SimTime += DeltaSeconds;

	if (RoomResendInterval > 0.0f && Now >= NextRoomTime)
	{
		NextRoomTime = Now + RoomResendInterval;
		SendRoomNow();
	}

	if (Now >= NextFeedTime)
	{
		NextFeedTime = Now + 1.0 / FMath::Max(SendRateHz, 0.5f);
		FeedData();
	}
}

// ---------------------------------------------------------------------------
// Console command. "VibH2O.Simulate" toggles; "VibH2O.Simulate ales", "22x8"
// or "off" say what to simulate. Spawns a transient simulator when the level
// has none, so judging a room never requires editing the map first.
// ---------------------------------------------------------------------------

namespace VibH2OSimulatorInternal
{
	static void SimulateCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			return;
		}

		AVibH2OSimulatorActor* Simulator = nullptr;
		for (TActorIterator<AVibH2OSimulatorActor> It(World); It; ++It)
		{
			Simulator = *It;
			break;
		}

		const FString Arg = (Args.Num() > 0) ? Args[0].ToLower() : FString();

		if (Arg == TEXT("off"))
		{
			if (Simulator != nullptr)
			{
				Simulator->StopSimulation();
			}
			return;
		}

		if (Simulator == nullptr)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.ObjectFlags |= RF_Transient;
			Simulator = World->SpawnActor<AVibH2OSimulatorActor>(SpawnParams);
			if (Simulator == nullptr)
			{
				UE_LOG(LogVibH2O, Warning, TEXT("VibH2O: impossible de creer un simulateur."));
				return;
			}
			Simulator->bSimulateOnBeginPlay = false;
		}

		if (Arg == TEXT("ales"))
		{
			Simulator->ConfigureAles();
		}
		else if (!Arg.IsEmpty())
		{
			FString Left;
			FString Right;
			if (Arg.Split(TEXT("x"), &Left, &Right) && Left.IsNumeric() && Right.IsNumeric())
			{
				Simulator->AisleColumns.Reset();
				Simulator->ExtraEmptySeatIndices.Reset();
				Simulator->SetRoomSize(FCString::Atoi(*Left), FCString::Atoi(*Right));
			}
			else
			{
				UE_LOG(LogVibH2O, Warning,
					TEXT("VibH2O: argument '%s' non compris — attendu 'ales', 'off' ou 'CxR' (ex. 22x8)."), *Arg);
				return;
			}
		}
		else if (Simulator->IsSimulating())
		{
			// Bare command on a running simulator: it is a toggle.
			Simulator->StopSimulation();
			return;
		}

		if (Simulator->IsSimulating())
		{
			Simulator->SendRoomNow();
		}
		else
		{
			Simulator->StartSimulation();
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs GVibH2OSimulateCommand(
		TEXT("VibH2O.Simulate"),
		TEXT("Simule le patch Max sans rien brancher. Sans argument: bascule. 'ales', 'CxR' (ex. 22x8) ou 'off'."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SimulateCommand));
}
