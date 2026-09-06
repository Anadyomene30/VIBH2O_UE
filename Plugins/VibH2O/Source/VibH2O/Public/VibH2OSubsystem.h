// The living model of the installation: network, room plan, per-individual
// state, averages, and control messages.
//
// It knows nothing of actors, bubbles or materials. Stage actors read from it;
// it never drives them. That separation is what allows several views of the
// same room, or none at all.
#pragma once

#include "CoreMinimal.h"
#include "Osc/VibH2OOscReceiver.h"
#include "Osc/VibH2OOscTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "VibH2OTypes.h"

#include "VibH2OSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVibH2ORoomChangedSignature, const FVibH2ORoomSnapshot&, Room);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVibH2OEffectChangedSignature, FName, EffectName, bool, bEnabled);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVibH2OFocusGroupChangedSignature, FName, GroupName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVibH2OFloatControlSignature, float, Value);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVibH2OBeatSignature, int32, IndividualId, int32, BeatCount);

/** Operational figures, for the debug overlay and for get-in. */
USTRUCT(BlueprintType)
struct VIBH2O_API FVibH2ONetworkStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Reseau")
	bool bListening = false;

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Reseau")
	FString Endpoint;

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Reseau")
	FString LastError;

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Reseau")
	int32 PacketsReceived = 0;

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Reseau")
	int32 MessagesReceived = 0;

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Reseau")
	int32 MessagesDropped = 0;

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Reseau")
	int32 MalformedPackets = 0;

	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Reseau")
	int32 QueueDepth = 0;

	/** Messages handled per second, smoothed. */
	UPROPERTY(BlueprintReadOnly, Category = "VibH2O|Reseau")
	float MessagesPerSecond = 0.0f;
};

/**
 * The VibH2O subsystem.
 *
 * It drains the network thread's queue from its own tick, holds the state, and
 * broadcasts changes. All object logic happens here, on the game thread.
 */
UCLASS(BlueprintType)
class VIBH2O_API UVibH2OSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual ~UVibH2OSubsystem() override;

	// UGameInstanceSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	virtual ETickableTickType GetTickableTickType() const override;

	/** Shortcut from any world context. */
	UFUNCTION(BlueprintPure, Category = "VibH2O", meta = (WorldContext = "WorldContextObject"))
	static UVibH2OSubsystem* Get(const UObject* WorldContextObject);

	// --------------------------------------------------------------- Network

	/** (Re)starts listening with the current project settings. */
	UFUNCTION(BlueprintCallable, Category = "VibH2O|Reseau")
	bool StartListening();

	UFUNCTION(BlueprintCallable, Category = "VibH2O|Reseau")
	void StopListening();

	UFUNCTION(BlueprintPure, Category = "VibH2O|Reseau")
	bool IsListening() const;

	UFUNCTION(BlueprintPure, Category = "VibH2O|Reseau")
	FVibH2ONetworkStats GetNetworkStats() const;

	// ------------------------------------------------------------- Room plan

	/**
	 * A copy of the room plan, for Blueprint. C++ should prefer GetRoomRef(),
	 * which avoids copying the seat array every frame.
	 */
	UFUNCTION(BlueprintPure, Category = "VibH2O|Salle")
	FVibH2ORoomSnapshot GetRoom() const { return Room; }

	const FVibH2ORoomSnapshot& GetRoomRef() const { return Room; }

	UFUNCTION(BlueprintPure, Category = "VibH2O|Salle")
	bool HasRoom() const { return Room.IsValid(); }

	/**
	 * Injects a room plan without going through the network. Useful to preview
	 * a room in the editor, and for tests.
	 */
	UFUNCTION(BlueprintCallable, Category = "VibH2O|Salle")
	void SetRoomManually(int32 InColumns, int32 InRows, const TArray<int32>& SeatIndividualIds);

	/** Clears the room and every individual. */
	UFUNCTION(BlueprintCallable, Category = "VibH2O|Salle")
	void ClearRoom();

	/** Renders the room as text, one line per row. For diagnosis. */
	UFUNCTION(BlueprintPure, Category = "VibH2O|Salle")
	FString DescribeRoom() const;

	// ----------------------------------------------------------- Individuals

	UFUNCTION(BlueprintPure, Category = "VibH2O|Individus")
	bool GetIndividual(int32 IndividualId, FVibH2OIndividualState& OutState) const;

	/** Direct access, no copy. Returns nullptr when the individual is unknown. */
	const FVibH2OIndividualState* FindIndividual(int32 IndividualId) const;

	UFUNCTION(BlueprintPure, Category = "VibH2O|Individus")
	int32 GetIndividualCount() const { return Individuals.Num(); }

	/** Number of individuals counting towards the averages. */
	UFUNCTION(BlueprintPure, Category = "VibH2O|Individus")
	int32 GetActiveIndividualCount() const { return ActiveIndividualCount; }

	/**
	 * Collective synchrony: the mean of the individual synchronies, empty seats
	 * and quiet sensors excluded. Max does not transmit it.
	 */
	UFUNCTION(BlueprintPure, Category = "VibH2O|Individus")
	float GetCollectiveSynchrony() const { return CollectiveSynchrony; }

	UFUNCTION(BlueprintPure, Category = "VibH2O|Individus")
	float GetAverageExcitation() const { return AverageExcitation; }

	UFUNCTION(BlueprintPure, Category = "VibH2O|Individus")
	float GetAverageBpm() const { return AverageBpm; }

	/**
	 * Rank of this seat among the occupied ones, 0-based, in seat-index order.
	 * The vortex uses it to spread the bubbles out without leaving holes where
	 * the room has them.
	 */
	UFUNCTION(BlueprintPure, Category = "VibH2O|Individus")
	int32 GetOccupiedOrdinal(int32 SeatIndex) const;

	UFUNCTION(BlueprintPure, Category = "VibH2O|Individus")
	int32 GetOccupiedCount() const { return OccupiedOrdinals.Num(); }

	// --------------------------------------------------------------- Effects

	/** Enables or disables an effect. Scope is global, for the whole room. */
	UFUNCTION(BlueprintCallable, Category = "VibH2O|Effets")
	void SetEffectEnabled(FName EffectName, bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "VibH2O|Effets")
	bool IsEffectEnabled(FName EffectName) const;

	UFUNCTION(BlueprintPure, Category = "VibH2O|Effets")
	TArray<FName> GetKnownEffects() const;

	// --------------------------------------------------------------- Control

	/**
	 * Cuts the OSC control input - /Effect, /Blend, /Focus.
	 *
	 * Sensor data keeps flowing: what this prevents is a message arriving
	 * mid-take and polluting a Sequencer track during a render.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Controle")
	bool bIgnoreOscControl = false;

	/** Last control values received, for actors that follow them. */
	UFUNCTION(BlueprintPure, Category = "VibH2O|Controle")
	float GetOscBlendAlpha() const { return OscBlendAlpha; }

	UFUNCTION(BlueprintPure, Category = "VibH2O|Controle")
	float GetOscFocusAmount() const { return OscFocusAmount; }

	UFUNCTION(BlueprintPure, Category = "VibH2O|Controle")
	FName GetOscFocusGroup() const { return OscFocusGroup; }

	// ---------------------------------------------------------------- Events

	/** Raised after the room plan has been rebuilt. */
	UPROPERTY(BlueprintAssignable, Category = "VibH2O|Evenements")
	FVibH2ORoomChangedSignature OnRoomChanged;

	/** Raised on every beat of every individual. */
	UPROPERTY(BlueprintAssignable, Category = "VibH2O|Evenements")
	FVibH2OBeatSignature OnBeat;

	UPROPERTY(BlueprintAssignable, Category = "VibH2O|Evenements")
	FVibH2OEffectChangedSignature OnEffectChanged;

	UPROPERTY(BlueprintAssignable, Category = "VibH2O|Evenements")
	FVibH2OFocusGroupChangedSignature OnOscFocusGroupChanged;

	UPROPERTY(BlueprintAssignable, Category = "VibH2O|Evenements")
	FVibH2OFloatControlSignature OnOscBlendAlphaChanged;

	UPROPERTY(BlueprintAssignable, Category = "VibH2O|Evenements")
	FVibH2OFloatControlSignature OnOscFocusAmountChanged;

	// ------------------------------------------------------------- Injection

	/**
	 * Handles an OSC message as if it had come off the network.
	 *
	 * This entry point exists for two reasons. First, to make the whole chain
	 * testable without a socket. Second, the day a direct MIDI control path is
	 * wired up, it will only have to build messages and push them in here - the
	 * three command routes (Details, Blueprint, OSC) will remain the very same
	 * functions.
	 */
	void InjectOscMessage(const FVibH2OOscMessage& Message);

	// ----------------------------------------------------------- Diagnostics

	/** Shows the network and room state on screen. */
	UPROPERTY(BlueprintReadWrite, Category = "VibH2O|Debogage")
	bool bShowDebugOverlay = false;

private:
	void DrainQueue();
	void HandleMessage(const FVibH2OOscMessage& Message);

	bool HandleRoomMappingMessage(const FVibH2OOscMessage& Message);
	bool HandleDataMessage(const FVibH2OOscMessage& Message);
	bool HandleControlMessage(const FVibH2OOscMessage& Message);

	void RebuildRoomFromPending();
	void UpdateOccupiedOrdinals();

	void AdvanceBeats(float DeltaTime);
	void UpdateStaleness(double Now);
	void UpdateAverages();
	void UpdateDebugOverlay(float DeltaTime);

	FVibH2OIndividualState& FindOrAddIndividual(int32 IndividualId);
	bool IsEmptySeatId(int32 RawId) const;

	/** Extracts the individual id from an address of the form "/Prefix/<id>/". */
	bool ParseIndividualId(const FVibH2OOscMessage& Message, const FString& Prefix, int32& OutId) const;

	/**
	 * The receiver is held by unique pointer. Its header is included above
	 * rather than forward-declared: the code UHT generates instantiates the
	 * TUniquePtr destructor, which requires a complete type. The receiver header
	 * pulls in nothing heavy - the socket types are themselves forward-declared
	 * there.
	 */
	TUniquePtr<FVibH2OOscReceiver> Receiver;

	UPROPERTY(Transient)
	FVibH2ORoomSnapshot Room;

	UPROPERTY(Transient)
	TMap<int32, FVibH2OIndividualState> Individuals;

	/** SeatIndex -> rank among the occupied seats. */
	TMap<int32, int32> OccupiedOrdinals;

	/**
	 * Buffer for the burst in progress. The plan arrives with no end marker, so
	 * we accumulate and only build after a silence.
	 */
	struct FPendingRoom
	{
		int32 Columns = 0;
		int32 Rows = 0;
		TMap<int32, int32> Seats;
		bool bDirty = false;
		double LastMessageTime = 0.0;

		void Reset()
		{
			Seats.Reset();
			bDirty = false;
		}
	};
	FPendingRoom Pending;

	TMap<FName, bool> EffectStates;

	float CollectiveSynchrony = 0.0f;
	float AverageExcitation = 0.0f;
	float AverageBpm = 0.0f;
	int32 ActiveIndividualCount = 0;

	float OscBlendAlpha = 0.0f;
	float OscFocusAmount = 0.0f;
	FName OscFocusGroup = NAME_None;

	/** Throughput counters, for the debug overlay. */
	int32 MessagesThisSecond = 0;
	float MessagesPerSecond = 0.0f;
	float RateTimer = 0.0f;

	/** Beat messages received on the wired-but-unused input. */
	int32 ExternalBeatCount = 0;
};
