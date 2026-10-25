#include "VibH2OSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Interfaces/IPluginManager.h"
#include "Layout/VibH2OLayoutMath.h"
#include "Misc/Paths.h"
#include "Osc/VibH2OOscReceiver.h"
#include "Remote/VibH2ORemoteServer.h"
#include "SocketSubsystem.h"
#include "UObject/GCObject.h"
#include "UObject/ObjectKey.h"
#include "UObject/Package.h"
#include "VibH2OModule.h"
#include "VibH2OSettings.h"

// ---------------------------------------------------------------------------
// Console commands. They exist for step 3 of the roadmap: checking that the
// room and the data arrive, before any bubble exists at all.
// ---------------------------------------------------------------------------

namespace VibH2OConsole
{
	static UVibH2OSubsystem* FindSubsystem(UWorld* World)
	{
		if (World == nullptr)
		{
			return nullptr;
		}
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			return GameInstance->GetSubsystem<UVibH2OSubsystem>();
		}
		return nullptr;
	}

	static FAutoConsoleCommandWithWorld ShowDebugCommand(
		TEXT("VibH2O.ShowDebug"),
		TEXT("Bascule l'affichage de debogage VibH2O (reseau, salle, moyennes)."),
		FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
		{
			if (UVibH2OSubsystem* Subsystem = FindSubsystem(World))
			{
				Subsystem->bShowDebugOverlay = !Subsystem->bShowDebugOverlay;
				UE_LOG(LogVibH2O, Display, TEXT("VibH2O: affichage de debogage %s."),
					Subsystem->bShowDebugOverlay ? TEXT("actif") : TEXT("inactif"));
			}
			else
			{
				UE_LOG(LogVibH2O, Warning, TEXT("VibH2O: subsystem introuvable."));
			}
		}));

	static FAutoConsoleCommandWithWorld DumpRoomCommand(
		TEXT("VibH2O.DumpRoom"),
		TEXT("Ecrit le plan de salle dans le log, une ligne par rangee. C'est l'outil du test 7 x 3."),
		FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
		{
			if (UVibH2OSubsystem* Subsystem = FindSubsystem(World))
			{
				UE_LOG(LogVibH2O, Display, TEXT("\n%s"), *Subsystem->DescribeRoom());
			}
		}));

	static FAutoConsoleCommandWithWorld RestartCommand(
		TEXT("VibH2O.Restart"),
		TEXT("Redemarre l'ecoute OSC avec les reglages de projet courants."),
		FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
		{
			if (UVibH2OSubsystem* Subsystem = FindSubsystem(World))
			{
				Subsystem->StartListening();
				// The remote too - which also reloads its page from disk, so an
				// edit made on site needs no restart of the game.
				if (UVibH2OSettings::Get()->bEnableRemote)
				{
					Subsystem->StartRemote();
				}
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs IgnoreControlCommand(
		TEXT("VibH2O.IgnoreControl"),
		TEXT("VibH2O.IgnoreControl 1 : l'OSC de controle et l'iPad sont ignores (les donnees passent toujours). 0 : ils reprennent la main. Sans argument : bascule."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			if (UVibH2OSubsystem* Subsystem = FindSubsystem(World))
			{
				Subsystem->bIgnoreOscControl = Args.Num() > 0 ? FCString::Atoi(*Args[0]) != 0 : !Subsystem->bIgnoreOscControl;
				UE_LOG(LogVibH2O, Display, TEXT("VibH2O: controle OSC et iPad %s."),
					Subsystem->bIgnoreOscControl ? TEXT("IGNORES") : TEXT("actifs"));
			}
		}));
}

// ---------------------------------------------------------------------------

namespace
{
	/**
	 * Maps a raw incoming value onto [0,1] against a configured range.
	 *
	 * A degenerate range (min == max) would divide by zero inside GetRangePct
	 * and let a NaN loose into the model - and Clamp does not stop a NaN, since
	 * every comparison against one is false. A NaN excitation would then reach
	 * the material and the drift. Fall back to clamping the raw value instead.
	 */
	float NormalizeInput(float Raw, float InMin, float InMax)
	{
		if (FMath::IsNearlyEqual(InMin, InMax))
		{
			return FMath::Clamp(Raw, 0.0f, 1.0f);
		}
		return FMath::Clamp(FMath::GetRangePct(InMin, InMax, Raw), 0.0f, 1.0f);
	}
}

// Defined here, where FVibH2OOscReceiver is a complete type.
UVibH2OSubsystem::~UVibH2OSubsystem() = default;

void UVibH2OSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	RegisterBuiltInEffects();

	if (UVibH2OSettings::Get()->bAutoStartReceiver)
	{
		StartListening();
	}
	if (UVibH2OSettings::Get()->bEnableRemote)
	{
		StartRemote();
	}
	InitializeCueInputs();
}

void UVibH2OSubsystem::RegisterBuiltInEffects()
{
	// The built-in effects exist from the start, enabled. An unknown effect
	// queried later is considered enabled too: a misspelt name must not
	// silently switch something off.
	EffectStates.Add(VibH2OEffects::Float, true);
	EffectStates.Add(VibH2OEffects::Drift, true);
	EffectStates.Add(VibH2OEffects::Beat, true);
	EffectStates.Add(VibH2OEffects::Tint, true);
	EffectStates.Add(VibH2OEffects::Focus, true);
	EffectStates.Add(VibH2OEffects::Scale, true);
}

void UVibH2OSubsystem::Deinitialize()
{
	StopListening();
	StopRemote();
	StopMidiInput();
	StopKeyboardCues();
	BackgroundGuard.Release();
	Individuals.Reset();
	Room.Reset();
	Super::Deinitialize();
}

UVibH2OSubsystem* UVibH2OSubsystem::Get(const UObject* WorldContextObject)
{
	if (WorldContextObject == nullptr)
	{
		return nullptr;
	}
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}
	if (UGameInstance* GameInstance = World->GetGameInstance())
	{
		return GameInstance->GetSubsystem<UVibH2OSubsystem>();
	}
#if WITH_EDITOR
	return GetEditorPreview(World);
#else
	return nullptr;
#endif
}

#if WITH_EDITOR
namespace VibH2OEditorPreviewInternal
{
	/**
	 * One stand-in per editor world, weakly held here.
	 *
	 * Kept alive by a referencer, never by rooting: when Play stops, the
	 * editor marks every UGameInstance as garbage (PlayLevel.cpp, 5.5 and
	 * 5.8 alike), ours included, and MarkAsGarbage asserts on a rooted
	 * object - Stop crashed the editor in any map holding a stage. Not
	 * rooted, the stand-in simply dies there and is rebuilt on the next
	 * editor tick; the simulator re-sends the room by itself.
	 */
	struct FEntry
	{
		TWeakObjectPtr<UGameInstance> Outer;
		TWeakObjectPtr<UVibH2OSubsystem> Subsystem;
	};

	static TMap<TObjectKey<UWorld>, FEntry> Entries;
	static FDelegateHandle CleanupHandle;

	/**
	 * Allocated once and never freed: a static FGCObject would be destroyed
	 * after the object system at exit, which is a crash on quit.
	 */
	class FReferencer : public FGCObject
	{
	public:
		TArray<TObjectPtr<UObject>> Objects;

		virtual void AddReferencedObjects(FReferenceCollector& Collector) override
		{
			Collector.AddReferencedObjects(Objects);
		}

		virtual FString GetReferencerName() const override
		{
			return TEXT("VibH2OEditorPreview");
		}
	};

	static FReferencer& GetReferencer()
	{
		static FReferencer* Referencer = new FReferencer();
		return *Referencer;
	}

	static void Release(FEntry& Entry)
	{
		TArray<TObjectPtr<UObject>>& Objects = GetReferencer().Objects;
		Objects.Remove(Entry.Subsystem.Get());
		Objects.Remove(Entry.Outer.Get());
		Objects.Remove(nullptr);
	}
}

UVibH2OSubsystem* UVibH2OSubsystem::GetEditorPreview(UWorld* World)
{
	using namespace VibH2OEditorPreviewInternal;

	// Plain editor worlds only: the Blueprint editor's preview worlds and the
	// like have no business simulating an audience.
	if (World == nullptr || World->WorldType != EWorldType::Editor)
	{
		return nullptr;
	}

	if (FEntry* Found = Entries.Find(TObjectKey<UWorld>(World)))
	{
		if (UVibH2OSubsystem* Existing = Found->Subsystem.Get())
		{
			return Existing;
		}
		Release(*Found);
		Entries.Remove(TObjectKey<UWorld>(World));
	}

	if (!CleanupHandle.IsValid())
	{
		// Loading another map tears the old world down: its stand-in goes with
		// it, room and individuals included.
		CleanupHandle = FWorldDelegates::OnWorldCleanup.AddLambda(
			[](UWorld* CleanedWorld, bool /*bSessionEnded*/, bool /*bCleanupResources*/)
			{
				if (FEntry* Entry = Entries.Find(TObjectKey<UWorld>(CleanedWorld)))
				{
					Release(*Entry);
					Entries.Remove(TObjectKey<UWorld>(CleanedWorld));
				}
			});
	}

	// A game-instance subsystem must live inside a game instance. A bare,
	// never-initialised one is enough - the subsystem never uses it, which is
	// also how the integration tests build theirs.
	UGameInstance* Outer = NewObject<UGameInstance>(GetTransientPackage(), NAME_None, RF_Transient);
	UVibH2OSubsystem* Subsystem = NewObject<UVibH2OSubsystem>(Outer, NAME_None, RF_Transient);
	GetReferencer().Objects.Add(Outer);
	GetReferencer().Objects.Add(Subsystem);

	// No Initialize(): that would start the receiver. Effects only.
	Subsystem->bEditorPreview = true;
	Subsystem->RegisterBuiltInEffects();

	FEntry Entry;
	Entry.Outer = Outer;
	Entry.Subsystem = Subsystem;
	Entries.Add(TObjectKey<UWorld>(World), Entry);

	UE_LOG(LogVibH2O, Display, TEXT("VibH2O: subsystem de previsualisation cree pour l'editeur (aucune ecoute reseau)."));
	return Subsystem;
}

void UVibH2OSubsystem::TickEditorPreview(float DeltaTime)
{
	if (!bEditorPreview || LastPreviewTickFrame == GFrameCounter)
	{
		return;
	}
	LastPreviewTickFrame = GFrameCounter;
	Tick(DeltaTime);
}
#endif

// ----------------------------------------------------------------- Network

bool UVibH2OSubsystem::StartListening()
{
	const UVibH2OSettings* Settings = UVibH2OSettings::Get();

	if (!Receiver.IsValid())
	{
		Receiver = MakeUnique<FVibH2OOscReceiver>();
	}

	return Receiver->Start(
		Settings->ListenAddress,
		Settings->ListenPort,
		Settings->ReceiveBufferSize,
		Settings->MaxQueuedMessages);
}

void UVibH2OSubsystem::StopListening()
{
	if (Receiver.IsValid())
	{
		Receiver->Shutdown();
		Receiver.Reset();
	}
}

bool UVibH2OSubsystem::IsListening() const
{
	return Receiver.IsValid() && Receiver->IsRunning();
}

FVibH2ONetworkStats UVibH2OSubsystem::GetNetworkStats() const
{
	FVibH2ONetworkStats Stats;
	if (Receiver.IsValid())
	{
		Stats.bListening = Receiver->IsRunning();
		Stats.Endpoint = Receiver->GetEndpointDescription();
		Stats.LastError = Receiver->GetLastError();
		Stats.PacketsReceived = Receiver->GetPacketsReceived();
		Stats.MessagesReceived = Receiver->GetMessagesReceived();
		Stats.MessagesDropped = Receiver->GetMessagesDropped();
		Stats.MalformedPackets = Receiver->GetMalformedPackets();
		Stats.QueueDepth = Receiver->GetQueueDepth();
	}
	Stats.MessagesPerSecond = MessagesPerSecond;
	return Stats;
}

// ------------------------------------------------------------------ Remote

bool UVibH2OSubsystem::StartRemote()
{
	const UVibH2OSettings* Settings = UVibH2OSettings::Get();

	FString WebRoot = Settings->RemoteWebRoot;
	if (WebRoot.IsEmpty())
	{
		// Inside the plugin, so copying the plugin folder into the show's
		// project brings the page along. Staged next to the game as loose
		// files - see VibH2O.Build.cs - so it stays editable once packaged.
		if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("VibH2O")))
		{
			WebRoot = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources"), TEXT("Remote"));
		}
	}
	else if (FPaths::IsRelative(WebRoot))
	{
		WebRoot = FPaths::Combine(FPaths::ProjectDir(), WebRoot);
	}

	if (!RemoteServer.IsValid())
	{
		RemoteServer = MakeUnique<FVibH2ORemoteServer>();
	}
	if (!RemoteServer->Start(Settings->RemoteListenAddress, Settings->RemotePort, WebRoot))
	{
		return false;
	}

	for (const FString& Url : GetRemoteUrls())
	{
		UE_LOG(LogVibH2O, Display, TEXT("VibH2O: telecommande iPad — ouvrir %s dans Safari."), *Url);
	}
	return true;
}

void UVibH2OSubsystem::StopRemote()
{
	if (RemoteServer.IsValid())
	{
		RemoteServer->Shutdown();
		RemoteServer.Reset();
	}
}

bool UVibH2OSubsystem::IsRemoteRunning() const
{
	return RemoteServer.IsValid() && RemoteServer->IsRunning();
}

TArray<FString> UVibH2OSubsystem::GetRemoteUrls() const
{
	TArray<FString> Urls;
	if (!IsRemoteRunning())
	{
		return Urls;
	}

	const int32 Port = RemoteServer->GetPort();
	if (ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
	{
		TArray<TSharedPtr<FInternetAddr>> Addresses;
		if (SocketSubsystem->GetLocalAdapterAddresses(Addresses))
		{
			for (const TSharedPtr<FInternetAddr>& Address : Addresses)
			{
				if (!Address.IsValid() || Address->GetProtocolType() != FNetworkProtocolTypes::IPv4)
				{
					continue;
				}
				// Loopback and self-assigned addresses are no use to an iPad.
				const FString Ip = Address->ToString(/*bAppendPort=*/false);
				if (Ip.StartsWith(TEXT("127.")) || Ip.StartsWith(TEXT("169.254.")) || Ip == TEXT("0.0.0.0"))
				{
					continue;
				}
				Urls.AddUnique(FString::Printf(TEXT("http://%s:%d/"), *Ip, Port));
			}
		}
	}
	if (Urls.Num() == 0)
	{
		Urls.Add(FString::Printf(TEXT("http://127.0.0.1:%d/"), Port));
	}
	return Urls;
}

void UVibH2OSubsystem::SetRemoteState(const FString& Json)
{
	if (RemoteServer.IsValid())
	{
		RemoteServer->SetStateJson(Json);
	}
}

// -------------------------------------------------------------------- Tick

TStatId UVibH2OSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UVibH2OSubsystem, STATGROUP_Tickables);
}

bool UVibH2OSubsystem::IsTickable() const
{
	// The editor stand-in is ticked by the actors that use it, never by the
	// engine.
	return !IsTemplate() && !bEditorPreview;
}

ETickableTickType UVibH2OSubsystem::GetTickableTickType() const
{
	return IsTemplate() ? ETickableTickType::Never : ETickableTickType::Conditional;
}

void UVibH2OSubsystem::Tick(float DeltaTime)
{
	const UVibH2OSettings* Settings = UVibH2OSettings::Get();
	// Monotonic clock rather than world time: a quiet sensor must be detected
	// even if the game is paused or slowed down.
	const double Now = FPlatformTime::Seconds();

	DrainQueue();
	DrainCueInputs();

	// End of burst: the plan carries no end marker, silence stands in for one.
	if (Pending.bDirty && (Now - Pending.LastMessageTime) >= Settings->RoomMappingSettleSeconds)
	{
		RebuildRoomFromPending();
	}

	AdvanceBeats(DeltaTime);
	UpdateStaleness(Now);
	UpdateAverages();
	UpdateDebugOverlay(DeltaTime);
}

// ---------------------------------------------------------------- Messages

void UVibH2OSubsystem::DrainQueue()
{
	const bool bLog = UVibH2OSettings::Get()->bLogOscTraffic;

	FVibH2OOscMessage Message;
	// Re-checked every time: a message may end a learn capture, which closes
	// the editor stand-in's receiver from inside HandleMessage.
	while (Receiver.IsValid() && Receiver->Dequeue(Message))
	{
		++MessagesThisSecond;
		if (bLog)
		{
			UE_LOG(LogVibH2O, Verbose, TEXT("OSC < %s"), *Message.ToDebugString());
		}
		TGuardValue<bool> FromNetwork(bDrainingNetwork, true);
		HandleMessage(Message);
	}

	// The iPad's commands join the very same path as Max's: one control
	// route, whatever sent it. Bounded per frame, so a flood from the LAN
	// costs a few frames of latency, never a hitch; the rest waits its turn.
	constexpr int32 MaxRemoteCommandsPerFrame = 256;
	int32 RemoteHandled = 0;
	while (RemoteHandled < MaxRemoteCommandsPerFrame && RemoteServer.IsValid() && RemoteServer->Dequeue(Message))
	{
		++RemoteHandled;
		if (!IsRemoteAddressAllowed(Message))
		{
			// Once a second at most: a flood must not become a flood of logs.
			static double LastRefusalLog = -1.0;
			if (FPlatformTime::Seconds() - LastRefusalLog > 1.0)
			{
				LastRefusalLog = FPlatformTime::Seconds();
				UE_LOG(LogVibH2O, Warning, TEXT("VibH2O: commande iPad refusee (adresse de donnees) : %s"), *Message.Address);
			}
			continue;
		}
		if (bLog)
		{
			UE_LOG(LogVibH2O, Verbose, TEXT("iPad < %s"), *Message.ToDebugString());
		}
		TGuardValue<bool> Echo(bForceControlEcho, true);
		HandleMessage(Message);
	}
}

bool UVibH2OSubsystem::IsRemoteAddressAllowed(const FVibH2OOscMessage& Message) const
{
	const UVibH2OSettings* Settings = UVibH2OSettings::Get();
	const FString* Allowed[] = {
		&Settings->BlendPrefix, &Settings->FocusGroupPrefix, &Settings->FocusAmountPrefix,
		&Settings->EffectPrefix, &Settings->BackdropPrefix, &Settings->AttractorPrefix,
		&Settings->AttractorsGlobalPrefix, &Settings->ExclusionPrefix, &Settings->CuePrefix, &Settings->AmbiancePrefix };
	for (const FString* Prefix : Allowed)
	{
		if (!Prefix->IsEmpty() && Message.MatchesPrefix(*Prefix))
		{
			return true;
		}
	}
	return false;
}

void UVibH2OSubsystem::InjectOscMessage(const FVibH2OOscMessage& Message)
{
	// Injected messages are deliberate - cues above all: a control value
	// reaches the stage even when it repeats the last one.
	TGuardValue<bool> Echo(bForceControlEcho, true);
	HandleMessage(Message);
}

void UVibH2OSubsystem::HandleMessage(const FVibH2OOscMessage& Message)
{
	if (HandleRoomMappingMessage(Message) || HandleDataMessage(Message))
	{
		// Only what came off the socket: the simulator injects its own.
		if (bDrainingNetwork)
		{
			++NetworkDataMessageCount;
		}
		return;
	}
	// Room plan and sensor data are never cue material; everything else is,
	// the plugin's own control routes included.
	BroadcastOscCueInput(Message);
	HandleControlMessage(Message);
}

bool UVibH2OSubsystem::ParseIndividualId(const FVibH2OOscMessage& Message, const FString& Prefix, int32& OutId) const
{
	OutId = 0;
	if (Prefix.IsEmpty())
	{
		return false;
	}

	TArray<FString> Segments;
	if (!Message.GetSegmentsAfterPrefix(Prefix, Segments) || Segments.Num() == 0)
	{
		return false;
	}
	if (!Segments[0].IsNumeric())
	{
		return false;
	}

	OutId = FCString::Atoi(*Segments[0]);
	return true;
}

bool UVibH2OSubsystem::IsEmptySeatId(int32 RawId) const
{
	const UVibH2OSettings* Settings = UVibH2OSettings::Get();
	return RawId <= 0 || Settings->EmptySeatIds.Contains(RawId);
}

bool UVibH2OSubsystem::HandleRoomMappingMessage(const FVibH2OOscMessage& Message)
{
	const UVibH2OSettings* Settings = UVibH2OSettings::Get();

	TArray<FString> Segments;
	if (!Message.GetSegmentsAfterPrefix(Settings->RoomMappingPrefix, Segments) || Segments.Num() == 0)
	{
		return false;
	}

	const FString& Key = Segments[0];

	if (Key.Equals(Settings->ColumnsSegment, ESearchCase::IgnoreCase))
	{
		Pending.Columns = Message.GetIntArg(0, Pending.Columns);
	}
	else if (Key.Equals(Settings->RowsSegment, ESearchCase::IgnoreCase))
	{
		Pending.Rows = Message.GetIntArg(0, Pending.Rows);
	}
	else if (Key.IsNumeric())
	{
		Pending.Seats.Add(FCString::Atoi(*Key), Message.GetIntArg(0, 0));
	}
	else
	{
		// Unknown segment under /RoomMapping: ignore it, but do not pretend the
		// message did not belong to the plan.
		UE_LOG(LogVibH2O, Verbose, TEXT("VibH2O: segment de plan inconnu '%s'."), *Key);
		return true;
	}

	Pending.bDirty = true;
	Pending.LastMessageTime = FPlatformTime::Seconds();
	return true;
}

bool UVibH2OSubsystem::HandleDataMessage(const FVibH2OOscMessage& Message)
{
	const UVibH2OSettings* Settings = UVibH2OSettings::Get();
	const double Now = FPlatformTime::Seconds();

	int32 IndividualId = 0;

	if (ParseIndividualId(Message, Settings->BpmPrefix, IndividualId))
	{
		if (IsEmptySeatId(IndividualId))
		{
			// Individual 0 does not exist: no data is routed to it.
			return true;
		}
		const float Bpm = Message.GetFloatArg(0, 0.0f);
		if (Bpm >= Settings->MinBpm && Bpm <= Settings->MaxBpm)
		{
			FVibH2OIndividualState& State = FindOrAddIndividual(IndividualId);
			// BPM is never smoothed: continuity is the phase's job, and
			// smoothing would only delay the response.
			State.Bpm = Bpm;
			State.LastUpdateTime = Now;
			State.bEverReceived = true;
		}
		else if (Bpm != 0.0f)
		{
			UE_LOG(LogVibH2O, Verbose, TEXT("VibH2O: BPM hors plage ignore (%f) pour l'individu %d."), Bpm, IndividualId);
		}
		return true;
	}

	if (ParseIndividualId(Message, Settings->ExcitationPrefix, IndividualId))
	{
		if (IsEmptySeatId(IndividualId))
		{
			return true;
		}
		FVibH2OIndividualState& State = FindOrAddIndividual(IndividualId);
		const float Raw = Message.GetFloatArg(0, 0.0f);
		State.ExcitationTarget = NormalizeInput(Raw, Settings->ExcitationInputMin, Settings->ExcitationInputMax);
		State.LastUpdateTime = Now;
		State.bEverReceived = true;
		return true;
	}

	if (ParseIndividualId(Message, Settings->SynchronyPrefix, IndividualId))
	{
		if (IsEmptySeatId(IndividualId))
		{
			return true;
		}
		FVibH2OIndividualState& State = FindOrAddIndividual(IndividualId);
		const float Raw = Message.GetFloatArg(0, 0.0f);
		State.SynchronyTarget = NormalizeInput(Raw, Settings->SynchronyInputMin, Settings->SynchronyInputMax);
		State.LastUpdateTime = Now;
		State.bEverReceived = true;
		return true;
	}

	if (ParseIndividualId(Message, Settings->BeatPrefix, IndividualId))
	{
		// The real-beat input. It is wired and functional, but Max does not
		// send it today: the day it does, there will be nothing to write. It
		// deliberately does NOT rewrite the phase, so as not to introduce the
		// very discontinuity everything else avoids.
		if (!IsEmptySeatId(IndividualId))
		{
			FVibH2OIndividualState& State = FindOrAddIndividual(IndividualId);
			State.LastUpdateTime = Now;
			State.bEverReceived = true;
			State.BeatPulse = 1.0f;
			State.BeatCount += 1;
			State.bBeatThisFrame = true;
			++ExternalBeatCount;
			OnBeat.Broadcast(IndividualId, State.BeatCount);
		}
		return true;
	}

	return false;
}

bool UVibH2OSubsystem::HandleControlMessage(const FVibH2OOscMessage& Message)
{
	const UVibH2OSettings* Settings = UVibH2OSettings::Get();

	TArray<FString> Segments;

	// An empty prefix matches every address by construction (MatchesPrefix of
	// "" is always true): a cleared setting would swallow the whole stream.
	// Each control route therefore guards against it.
	if (!Settings->EffectPrefix.IsEmpty()
		&& Message.GetSegmentsAfterPrefix(Settings->EffectPrefix, Segments) && Segments.Num() >= 1)
	{
		if (bIgnoreOscControl)
		{
			return true;
		}
		const FName EffectName(*Segments[0]);
		const bool bEnabled = Message.Args.Num() > 0 ? Message.Args[0].AsBool(true) : true;
		SetEffectEnabled(EffectName, bEnabled);
		return true;
	}

	if (!Settings->BlendPrefix.IsEmpty() && Message.MatchesPrefix(Settings->BlendPrefix))
	{
		if (bIgnoreOscControl)
		{
			return true;
		}
		const float Value = FMath::Clamp(Message.GetFloatArg(0, 0.0f), 0.0f, 1.0f);
		if (bForceControlEcho || !FMath::IsNearlyEqual(Value, OscBlendAlpha))
		{
			OscBlendAlpha = Value;
			OnOscBlendAlphaChanged.Broadcast(Value);
		}
		return true;
	}

	if (!Settings->FocusAmountPrefix.IsEmpty() && Message.MatchesPrefix(Settings->FocusAmountPrefix))
	{
		if (bIgnoreOscControl)
		{
			return true;
		}
		const float Value = FMath::Clamp(Message.GetFloatArg(0, 0.0f), 0.0f, 1.0f);
		if (bForceControlEcho || !FMath::IsNearlyEqual(Value, OscFocusAmount))
		{
			OscFocusAmount = Value;
			OnOscFocusAmountChanged.Broadcast(Value);
		}
		return true;
	}

	if (!Settings->BackdropPrefix.IsEmpty() && Message.MatchesPrefix(Settings->BackdropPrefix))
	{
		if (bIgnoreOscControl)
		{
			return true;
		}
		OscBackdropTarget = FMath::Clamp(Message.GetFloatArg(0, 1.0f), 0.0f, 1.0f);
		const float FadeSeconds = Message.Args.Num() > 1 ? FMath::Max(0.0f, Message.GetFloatArg(1, 0.0f)) : -1.0f;
		OnOscBackdropChanged.Broadcast(OscBackdropTarget, FadeSeconds);
		return true;
	}

	if (!Settings->AmbiancePrefix.IsEmpty() && Message.MatchesPrefix(Settings->AmbiancePrefix))
	{
		if (bIgnoreOscControl)
		{
			return true;
		}
		// "/Ambiance calme", "/Ambiance/calme/" or "/Ambiance 1": a word as
		// argument or last segment, or the index 0 auto, 1 calm, 2 normal,
		// 3 excited - the order of EVibH2OMoodOverride.
		FString Word;
		if (Message.Args.Num() > 0 && Message.Args[0].Type == EVibH2OOscType::String)
		{
			Word = Message.Args[0].StringValue;
		}
		else if (Message.GetSegmentsAfterPrefix(Settings->AmbiancePrefix, Segments) && Segments.Num() > 0)
		{
			Word = Segments[0];
		}
		int32 Index = INDEX_NONE;
		if (!Word.IsEmpty())
		{
			Word = Word.ToLower();
			Index = Word.StartsWith(TEXT("auto")) ? 0
				: Word.StartsWith(TEXT("calm")) ? 1
				: Word.StartsWith(TEXT("norm")) ? 2
				: (Word.StartsWith(TEXT("exc")) || Word.StartsWith(TEXT("exi"))) ? 3
				: INDEX_NONE;
		}
		else if (Message.Args.Num() > 0)
		{
			Index = FMath::RoundToInt(Message.GetFloatArg(0, 0.0f));
		}
		if (Index >= 0 && Index <= 3)
		{
			OnOscAmbianceChanged.Broadcast(Index);
		}
		else
		{
			UE_LOG(LogVibH2O, Warning, TEXT("VibH2O: ambiance inconnue dans %s (attendu : auto, calme, normal, excite)."), *Message.ToDebugString());
		}
		return true;
	}

	if (!Settings->FocusGroupPrefix.IsEmpty() && Message.MatchesPrefix(Settings->FocusGroupPrefix))
	{
		if (bIgnoreOscControl)
		{
			return true;
		}
		// The group name may arrive as a string argument, or as the last
		// segment of the address: both forms occur on the Max side.
		FName GroupName = NAME_None;
		if (Message.Args.Num() > 0 && Message.Args[0].Type == EVibH2OOscType::String)
		{
			GroupName = FName(*Message.Args[0].StringValue);
		}
		else if (Message.GetSegmentsAfterPrefix(Settings->FocusGroupPrefix, Segments) && Segments.Num() > 0)
		{
			GroupName = FName(*Segments[0]);
		}

		if (bForceControlEcho || GroupName != OscFocusGroup)
		{
			OscFocusGroup = GroupName;
			OnOscFocusGroupChanged.Broadcast(GroupName);
		}
		return true;
	}

	return HandleSteeringMessage(Message);
}

bool UVibH2OSubsystem::HandleSteeringMessage(const FVibH2OOscMessage& Message)
{
	const UVibH2OSettings* Settings = UVibH2OSettings::Get();

	TArray<FString> Segments;

	// Segments are compared whole, so "/Attractors" and "/Attractor" cannot
	// swallow each other. Each route still guards against a cleared prefix,
	// which would match every address.
	if (!Settings->AttractorsGlobalPrefix.IsEmpty()
		&& Message.GetSegmentsAfterPrefix(Settings->AttractorsGlobalPrefix, Segments) && Segments.Num() >= 1)
	{
		if (bIgnoreOscControl)
		{
			return true;
		}
		if (Segments[0].Equals(TEXT("Release"), ESearchCase::IgnoreCase))
		{
			OnOscAttractorsReleased.Broadcast();
		}
		else if (Segments[0].Equals(TEXT("FadeScale"), ESearchCase::IgnoreCase))
		{
			OnOscAttractorFadeScaleChanged.Broadcast(FMath::Clamp(Message.GetFloatArg(0, 1.0f), 0.1f, 10.0f));
		}
		return true;
	}

	if (!Settings->AttractorPrefix.IsEmpty()
		&& Message.GetSegmentsAfterPrefix(Settings->AttractorPrefix, Segments) && Segments.Num() >= 1)
	{
		if (bIgnoreOscControl)
		{
			return true;
		}
		const FName Name(*Segments[0]);
		// No argument means "on": a bare address is the simplest cue to send.
		const float Value = FMath::Clamp(Message.GetFloatArg(0, 1.0f), 0.0f, 1.0f);
		if (Segments.Num() >= 2 && Segments[1].Equals(TEXT("Share"), ESearchCase::IgnoreCase))
		{
			OnOscAttractorShareChanged.Broadcast(Name, Value);
		}
		else
		{
			OnOscAttractorChanged.Broadcast(Name, Value);
		}
		return true;
	}

	if (!Settings->ExclusionPrefix.IsEmpty()
		&& Message.GetSegmentsAfterPrefix(Settings->ExclusionPrefix, Segments) && Segments.Num() >= 1)
	{
		if (bIgnoreOscControl)
		{
			return true;
		}
		const FName Name(*Segments[0]);
		if (Segments.Num() >= 2 && Segments[1].Equals(TEXT("Position"), ESearchCase::IgnoreCase))
		{
			const FVector2D Position(
				FMath::Clamp(Message.GetFloatArg(0, 0.0f), -1.0f, 1.0f),
				FMath::Clamp(Message.GetFloatArg(1, 0.0f), -1.0f, 1.0f));
			OnOscExclusionMoved.Broadcast(Name, Position);
		}
		else
		{
			OnOscExclusionChanged.Broadcast(Name, FMath::Clamp(Message.GetFloatArg(0, 1.0f), 0.0f, 1.0f));
		}
		return true;
	}

	return false;
}


// --------------------------------------------------------------- Room plan

void UVibH2OSubsystem::RebuildRoomFromPending()
{
	const UVibH2OSettings* Settings = UVibH2OSettings::Get();

	Pending.bDirty = false;

	if (Pending.Columns <= 0 || Pending.Rows <= 0)
	{
		UE_LOG(LogVibH2O, Warning,
			TEXT("VibH2O: rafale de plan recue sans dimensions utilisables (%d x %d). Salle ignoree."),
			Pending.Columns, Pending.Rows);
		Pending.Reset();
		return;
	}

	FVibH2ORoomSnapshot NewRoom;
	NewRoom.Columns = Pending.Columns;
	NewRoom.Rows = Pending.Rows;
	NewRoom.Generation = Room.Generation + 1;

	const int32 SeatCount = NewRoom.Columns * NewRoom.Rows;
	NewRoom.Seats.Reserve(SeatCount);

	int32 Ignored = 0;
	int32 OffsetRejected = 0;

	for (int32 SeatIndex = 1; SeatIndex <= SeatCount; ++SeatIndex)
	{
		const int32* Found = Pending.Seats.Find(SeatIndex);
		if (Found == nullptr && !Settings->bTreatMissingSeatsAsEmpty)
		{
			// Keep whatever the previous room said about this seat.
			if (const FVibH2OSeat* Previous = Room.FindSeatByIndex(SeatIndex))
			{
				NewRoom.Seats.Add(*Previous);
				continue;
			}
		}

		FVibH2OSeat Seat;
		Seat.SeatIndex = SeatIndex;
		if (!FVibH2OLayoutMath::SeatIndexToColumnRow(SeatIndex, NewRoom.Columns, NewRoom.Rows, Settings->SeatOrder, Seat.Column, Seat.Row))
		{
			continue;
		}

		const int32 RawId = Found ? *Found : 0;
		if (IsEmptySeatId(RawId))
		{
			// Pitfall 3: no bubble here, and excluded from every average.
			Seat.IndividualId = 0;
			Seat.bOccupied = false;
		}
		else
		{
			const int32 OffsetId = RawId + Settings->IndividualIdOffset;
			if (OffsetId <= 0)
			{
				// A misconfigured IndividualIdOffset would produce ids the data
				// routing rejects (it treats <= 0 as "nobody here"): the seat
				// would hold a bubble that can never receive data. Treat it as
				// empty and count it, so the mistake is loud instead of silent.
				Seat.IndividualId = 0;
				Seat.bOccupied = false;
				++OffsetRejected;
			}
			else
			{
				Seat.IndividualId = OffsetId;
				Seat.bOccupied = true;
			}
		}
		NewRoom.Seats.Add(Seat);
	}

	// Seat indices received outside the announced grid: a sign that columns and
	// rows do not match what the sender believes it is sending.
	for (const TPair<int32, int32>& Entry : Pending.Seats)
	{
		if (Entry.Key < 1 || Entry.Key > SeatCount)
		{
			++Ignored;
		}
	}
	if (Ignored > 0)
	{
		UE_LOG(LogVibH2O, Warning,
			TEXT("VibH2O: %d index de siege hors de la grille %d x %d ont ete ignores."),
			Ignored, NewRoom.Columns, NewRoom.Rows);
	}
	if (OffsetRejected > 0)
	{
		UE_LOG(LogVibH2O, Warning,
			TEXT("VibH2O: %d sieges vides de force — IndividualIdOffset (%d) produit des identifiants <= 0."),
			OffsetRejected, Settings->IndividualIdOffset);
	}

	const bool bSameShape = (Room.Columns == NewRoom.Columns) && (Room.Rows == NewRoom.Rows);
	const bool bIdentical = bSameShape && (Room.Seats == NewRoom.Seats);

	Pending.Reset();

	if (bIdentical)
	{
		// Nothing changed, so broadcast nothing. Without this check, every
		// resend of the plan from Max would make the whole room flicker.
		return;
	}

	Room = MoveTemp(NewRoom);
	UpdateOccupiedOrdinals();

	// Individuals no longer seated anywhere leave the model: they must stop
	// weighing on the averages.
	TSet<int32> SeatedIds;
	for (const FVibH2OSeat& Seat : Room.Seats)
	{
		if (Seat.bOccupied)
		{
			SeatedIds.Add(Seat.IndividualId);
		}
	}
	for (auto It = Individuals.CreateIterator(); It; ++It)
	{
		if (!SeatedIds.Contains(It.Key()))
		{
			It.RemoveCurrent();
		}
	}

	UE_LOG(LogVibH2O, Log,
		TEXT("VibH2O: plan de salle %d x %d — %d sieges occupes, %d vides (generation %d)."),
		Room.Columns, Room.Rows, Room.GetOccupiedCount(), Room.Seats.Num() - Room.GetOccupiedCount(), Room.Generation);

	// The full plan at Verbose level: this is what allows checking the
	// transposition on a headless machine, with
	//   -LogCmds="LogVibH2O Verbose"
	// Rebuilds are rare, so the cost is nil.
	UE_LOG(LogVibH2O, Verbose, TEXT("%s%s"), LINE_TERMINATOR, *DescribeRoom());

	OnRoomChanged.Broadcast(Room);
}

void UVibH2OSubsystem::UpdateOccupiedOrdinals()
{
	OccupiedOrdinals.Reset();
	int32 Ordinal = 0;
	for (const FVibH2OSeat& Seat : Room.Seats)
	{
		if (Seat.bOccupied)
		{
			OccupiedOrdinals.Add(Seat.SeatIndex, Ordinal++);
		}
	}
}

int32 UVibH2OSubsystem::GetOccupiedOrdinal(int32 SeatIndex) const
{
	const int32* Found = OccupiedOrdinals.Find(SeatIndex);
	return Found ? *Found : 0;
}

void UVibH2OSubsystem::SetRoomManually(int32 InColumns, int32 InRows, const TArray<int32>& SeatIndividualIds)
{
	Pending.Reset();
	Pending.Columns = InColumns;
	Pending.Rows = InRows;
	for (int32 Index = 0; Index < SeatIndividualIds.Num(); ++Index)
	{
		Pending.Seats.Add(Index + 1, SeatIndividualIds[Index]);
	}
	RebuildRoomFromPending();
}

void UVibH2OSubsystem::ClearRoom()
{
	Room.Reset();
	Room.Generation += 1;
	Individuals.Reset();
	OccupiedOrdinals.Reset();
	Pending.Reset();
	Pending.Columns = 0;
	Pending.Rows = 0;
	OnRoomChanged.Broadcast(Room);
}

FString UVibH2OSubsystem::DescribeRoom() const
{
	if (!Room.IsValid())
	{
		return TEXT("VibH2O: aucune salle recue.");
	}

	FString Result = FString::Printf(
		TEXT("VibH2O — salle %d x %d, %d occupes / %d places (generation %d)\n"),
		Room.Columns, Room.Rows, Room.GetOccupiedCount(), Room.Seats.Num(), Room.Generation);

	// One cell per seat, row by row: this is the rendering that makes a
	// transposition immediately visible on a non-square room.
	TArray<FString> Grid;
	Grid.Init(TEXT("  .  "), Room.Columns * Room.Rows);
	for (const FVibH2OSeat& Seat : Room.Seats)
	{
		const int32 Cell = Seat.Row * Room.Columns + Seat.Column;
		if (Grid.IsValidIndex(Cell) && Seat.bOccupied)
		{
			const FVibH2OIndividualState* State = Individuals.Find(Seat.IndividualId);
			const TCHAR Marker = (State == nullptr || !State->bEverReceived) ? TEXT('?') : (State->bStale ? TEXT('!') : TEXT(' '));
			Grid[Cell] = FString::Printf(TEXT("%4d%c"), Seat.IndividualId, Marker);
		}
	}

	for (int32 Row = 0; Row < Room.Rows; ++Row)
	{
		Result += FString::Printf(TEXT("  r%-2d "), Row);
		for (int32 Column = 0; Column < Room.Columns; ++Column)
		{
			Result += Grid[Row * Room.Columns + Column];
		}
		Result += TEXT("\n");
	}
	Result += TEXT("  ( . siege vide   ? aucune donnee recue   ! capteur muet )\n");
	return Result;
}

// -------------------------------------------------------------- Individuals

FVibH2OIndividualState& UVibH2OSubsystem::FindOrAddIndividual(int32 IndividualId)
{
	if (FVibH2OIndividualState* Existing = Individuals.Find(IndividualId))
	{
		return *Existing;
	}
	FVibH2OIndividualState& State = Individuals.Add(IndividualId);
	State.IndividualId = IndividualId;
	// A starting phase of the individual's own: without it every heart would
	// beat in unison at startup, which looks like nothing at all.
	State.BeatPhase = FVibH2ONoise::UnitFloat(IndividualId * 31 + 7);
	return State;
}

bool UVibH2OSubsystem::GetIndividual(int32 IndividualId, FVibH2OIndividualState& OutState) const
{
	if (const FVibH2OIndividualState* Found = Individuals.Find(IndividualId))
	{
		OutState = *Found;
		return true;
	}
	return false;
}

const FVibH2OIndividualState* UVibH2OSubsystem::FindIndividual(int32 IndividualId) const
{
	return Individuals.Find(IndividualId);
}

void UVibH2OSubsystem::AdvanceBeats(float DeltaTime)
{
	const UVibH2OSettings* Settings = UVibH2OSettings::Get();
	const bool bBeatEnabled = IsEffectEnabled(VibH2OEffects::Beat);
	const float PulseDecay = FMath::Max(Settings->BeatPulseDecaySeconds, 0.01f);

	// Framerate-independent exponential smoothing.
	const float SmoothAlpha = (Settings->ValueSmoothingSeconds > 0.0f)
		? 1.0f - FMath::Exp(-DeltaTime / Settings->ValueSmoothingSeconds)
		: 1.0f;

	// Beats are collected during the loop and broadcast after it. A Blueprint
	// bound to OnBeat may legitimately call back into this subsystem -
	// ClearRoom, SetRoomManually - and mutating the Individuals map while it is
	// being iterated would invalidate the iterator: a crash that would only
	// ever fire on the night someone wires audio or Niagara to the beats.
	TArray<TPair<int32, int32>, TInlineAllocator<64>> PendingBeats;

	for (TPair<int32, FVibH2OIndividualState>& Entry : Individuals)
	{
		FVibH2OIndividualState& State = Entry.Value;
		State.bBeatThisFrame = false;

		State.Excitation = FMath::Lerp(State.Excitation, State.ExcitationTarget, SmoothAlpha);
		State.Synchrony = FMath::Lerp(State.Synchrony, State.SynchronyTarget, SmoothAlpha);

		// A quiet sensor stops beating. Its phase is frozen, not reset: when it
		// comes back it resumes exactly where it was, with no jump.
		if (bBeatEnabled && !State.bStale && State.Bpm > 0.0f)
		{
			int32 Beats = 0;
			State.BeatPhase = FVibH2OLayoutMath::AdvanceBeatPhase(State.BeatPhase, State.Bpm, DeltaTime, Beats);
			if (Beats > 0)
			{
				State.BeatCount += Beats;
				State.BeatPulse = 1.0f;
				State.bBeatThisFrame = true;
				PendingBeats.Emplace(State.IndividualId, State.BeatCount);
			}
		}

		State.BeatPulse = FMath::Max(0.0f, State.BeatPulse - DeltaTime / PulseDecay);
	}

	for (const TPair<int32, int32>& Beat : PendingBeats)
	{
		OnBeat.Broadcast(Beat.Key, Beat.Value);
	}
}

void UVibH2OSubsystem::UpdateStaleness(double Now)
{
	const double Timeout = UVibH2OSettings::Get()->StaleTimeoutSeconds;

	for (TPair<int32, FVibH2OIndividualState>& Entry : Individuals)
	{
		FVibH2OIndividualState& State = Entry.Value;
		if (!State.bEverReceived)
		{
			continue;
		}

		const bool bWasStale = State.bStale;
		State.bStale = (Now - State.LastUpdateTime) > Timeout;

		if (State.bStale != bWasStale)
		{
			UE_LOG(LogVibH2O, Log, TEXT("VibH2O: individu %d %s."),
				State.IndividualId, State.bStale ? TEXT("passe muet") : TEXT("est revenu"));
		}
	}
}

void UVibH2OSubsystem::UpdateAverages()
{
	// The averages cover only individuals actually seated whose sensor is
	// talking. An empty seat counts for nothing - forgetting that would count
	// an unoccupied place as a person at zero synchrony, which would skew the
	// whole of tableau 2.
	double SumSynchrony = 0.0;
	double SumExcitation = 0.0;
	double SumBpm = 0.0;
	int32 Count = 0;

	for (const FVibH2OSeat& Seat : Room.Seats)
	{
		if (!Seat.bOccupied)
		{
			continue;
		}
		const FVibH2OIndividualState* State = Individuals.Find(Seat.IndividualId);
		if (State == nullptr || !State->CountsTowardAverages())
		{
			continue;
		}
		SumSynchrony += State->Synchrony;
		SumExcitation += State->Excitation;
		SumBpm += State->Bpm;
		++Count;
	}

	ActiveIndividualCount = Count;
	if (Count > 0)
	{
		CollectiveSynchrony = static_cast<float>(SumSynchrony / Count);
		AverageExcitation = static_cast<float>(SumExcitation / Count);
		AverageBpm = static_cast<float>(SumBpm / Count);
	}
	else
	{
		CollectiveSynchrony = 0.0f;
		AverageExcitation = 0.0f;
		AverageBpm = 0.0f;
	}
}

// ------------------------------------------------------------------ Effects

void UVibH2OSubsystem::SetEffectEnabled(FName EffectName, bool bEnabled)
{
	if (EffectName.IsNone())
	{
		return;
	}

	bool& Stored = EffectStates.FindOrAdd(EffectName, true);
	if (Stored == bEnabled)
	{
		return;
	}
	Stored = bEnabled;

	UE_LOG(LogVibH2O, Log, TEXT("VibH2O: effet '%s' %s."), *EffectName.ToString(), bEnabled ? TEXT("actif") : TEXT("inactif"));
	OnEffectChanged.Broadcast(EffectName, bEnabled);
}

bool UVibH2OSubsystem::IsEffectEnabled(FName EffectName) const
{
	if (const bool* Found = EffectStates.Find(EffectName))
	{
		return *Found;
	}
	// An effect never named is considered enabled: a typo in a name must not
	// switch something off in silence.
	return true;
}

TArray<FName> UVibH2OSubsystem::GetKnownEffects() const
{
	TArray<FName> Names;
	EffectStates.GetKeys(Names);
	Names.Sort(FNameLexicalLess());
	return Names;
}

// ------------------------------------------------------------- Diagnostics

void UVibH2OSubsystem::UpdateDebugOverlay(float DeltaTime)
{
	RateTimer += DeltaTime;
	if (RateTimer >= 1.0f)
	{
		MessagesPerSecond = MessagesThisSecond / RateTimer;
		MessagesThisSecond = 0;
		RateTimer = 0.0f;
	}

	if (!bShowDebugOverlay || GEngine == nullptr)
	{
		return;
	}

	const FVibH2ONetworkStats Stats = GetNetworkStats();

	auto Line = [](int32 Key, const FColor& Color, const FString& Text)
	{
		GEngine->AddOnScreenDebugMessage(Key, 1.1f, Color, Text);
	};

	const FColor NetColor = Stats.bListening ? FColor::Green : FColor::Red;

	Line(770001, NetColor, FString::Printf(
		TEXT("VibH2O  ecoute %s sur %s%s"),
		Stats.bListening ? TEXT("active") : TEXT("INACTIVE"),
		*Stats.Endpoint,
		Stats.LastError.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("  [%s]"), *Stats.LastError)));

	Line(770002, FColor::White, FString::Printf(
		TEXT("   reseau   %d paquets, %d messages, %.0f msg/s, file %d, jetes %d, malformes %d"),
		Stats.PacketsReceived, Stats.MessagesReceived, Stats.MessagesPerSecond,
		Stats.QueueDepth, Stats.MessagesDropped, Stats.MalformedPackets));

	if (Room.IsValid())
	{
		Line(770003, FColor::Cyan, FString::Printf(
			TEXT("   salle    %d x %d — %d occupes, %d vides (generation %d)"),
			Room.Columns, Room.Rows, Room.GetOccupiedCount(),
			Room.Seats.Num() - Room.GetOccupiedCount(), Room.Generation));
	}
	else
	{
		Line(770003, FColor::Yellow, TEXT("   salle    aucun plan recu"));
	}

	int32 StaleCount = 0;
	for (const TPair<int32, FVibH2OIndividualState>& Entry : Individuals)
	{
		StaleCount += Entry.Value.bStale ? 1 : 0;
	}

	Line(770004, FColor::White, FString::Printf(
		TEXT("   individus %d connus, %d actifs, %d muets"),
		Individuals.Num(), ActiveIndividualCount, StaleCount));

	Line(770005, FColor::White, FString::Printf(
		TEXT("   moyennes  synchronie %.3f   excitation %.3f   bpm %.1f"),
		CollectiveSynchrony, AverageExcitation, AverageBpm));

	Line(770006, FColor::Silver, FString::Printf(
		TEXT("   controle  blend %.2f   focus %.2f sur '%s'   OSC de controle %s   battements externes %d"),
		OscBlendAlpha, OscFocusAmount, *OscFocusGroup.ToString(),
		bIgnoreOscControl ? TEXT("IGNORE") : TEXT("actif"), ExternalBeatCount));

	if (IsRemoteRunning())
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - OverlayRemoteUrlsTime > 2.0)
		{
			OverlayRemoteUrls = GetRemoteUrls();
			OverlayRemoteUrlsTime = Now;
		}
		Line(770007, FColor::Green, FString::Printf(
			TEXT("   iPad     %s   %d requetes, %d commandes"),
			*FString::Join(OverlayRemoteUrls, TEXT("  ")), RemoteServer->GetRequestCount(), RemoteServer->GetCommandCount()));
	}
	else
	{
		const FString Error = RemoteServer.IsValid() ? RemoteServer->GetLastError() : FString();
		Line(770007, FColor::Silver, FString::Printf(TEXT("   iPad     telecommande inactive%s"),
			Error.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("  [%s]"), *Error)));
	}
	const TArray<FString> MidiNames = GetOpenMidiInputs();
	Line(770008, FColor::Silver, FString::Printf(
		TEXT("   cues     MIDI %s   clavier %s   %s"),
		MidiNames.Num() > 0 ? *FString::Join(MidiNames, TEXT(", ")) : TEXT("aucune entree"),
		KeyboardSink.IsValid() && KeyboardSink->IsRunning() ? TEXT("actif") : TEXT("inactif"),
		*FVibH2OBackgroundGuard::DescribeState()));
}
