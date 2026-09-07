// The built-in simulator: judge the installation without wiring anything up.
//
// It plays the part of Max. It emits a room plan and a continuous stream of
// per-individual data through the very same injection path the network uses -
// same addresses, built from the same project settings, same burst-and-settle
// behaviour. Drop one in the level (the demo map ships with one), press Play,
// and the room lives.
//
// The Python simulator (Tools/vibh2o_osc_sim.py) remains the tool that
// exercises the real UDP path; this actor exists for judging by eye, in the
// editor, with nothing external running. It is compiled into every build on
// purpose: rehearsing without sensors is a real show need, not a debug one.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "VibH2OSimulatorActor.generated.h"

class UVibH2OSubsystem;

/** What the simulated audience is doing. */
UENUM(BlueprintType)
enum class EVibH2OSimulatorScenario : uint8
{
	/** Calm room, gentle individual variation. */
	Nominal UMETA(DisplayName = "Nominal"),

	/** A wave of excitation crossing the room from left to right. */
	Wave UMETA(DisplayName = "Vague d'excitation"),

	/** Collective synchrony rising and falling - what drives the vortex. */
	SyncSweep UMETA(DisplayName = "Balayage de synchronie"),

	/** The whole room rises together, then falls back. */
	Peak UMETA(DisplayName = "Pic collectif"),

	/** Some sensors stop talking after a delay - the staleness path. */
	Dropout UMETA(DisplayName = "Coupure de capteurs")
};

/**
 * In-level stand-in for the Max patch.
 *
 * It knows nothing of bubbles or stage actors: it only pushes OSC messages
 * into the subsystem, exactly as the network thread would. Everything
 * downstream - burst settling, seat-order mapping, smoothing, staleness,
 * averages - therefore runs the real code, not a shortcut.
 */
UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "VibH2O — Simulateur"))
class VIBH2O_API AVibH2OSimulatorActor : public AActor
{
	GENERATED_BODY()

public:
	AVibH2OSimulatorActor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	// ================================================== Room plan

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Simulation|Salle", meta = (ClampMin = "1", ClampMax = "64"))
	int32 Columns = 7;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Simulation|Salle", meta = (ClampMin = "1", ClampMax = "32"))
	int32 Rows = 3;

	/** Columns left entirely empty - the aisles. Ales has 6 and 16. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Simulation|Salle")
	TArray<int32> AisleColumns;

	/** Extra empty seats, by OSC seat index (1-based), for scattered holes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Simulation|Salle")
	TArray<int32> ExtraEmptySeatIndices;

	/**
	 * Interval between full room-plan resends, in seconds. 0 sends the plan
	 * once at start. Resending exercises the rebuild-by-difference path: the
	 * room must NOT flicker when an identical plan comes in again.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Simulation|Salle", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float RoomResendInterval = 0.0f;

	// ================================================== Data stream

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Simulation|Donnees")
	EVibH2OSimulatorScenario Scenario = EVibH2OSimulatorScenario::Nominal;

	/** Messages per individual per second. Max sends in this ballpark. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Simulation|Donnees", meta = (ClampMin = "0.5", ClampMax = "60.0"))
	float SendRateHz = 10.0f;

	/** Resting heart-rate range the simulated audience is drawn from. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Simulation|Donnees", meta = (ClampMin = "30.0"))
	float BaseBpmMin = 58.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Simulation|Donnees", meta = (ClampMin = "30.0"))
	float BaseBpmMax = 92.0f;

	/** Time scale of the scenario waveforms. 1 matches the Python simulator. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Simulation|Donnees", meta = (ClampMin = "0.1", ClampMax = "10.0"))
	float ScenarioSpeed = 1.0f;

	/** Dropout scenario: seconds before sensors start going quiet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Simulation|Donnees", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float DropoutAfterSeconds = 8.0f;

	/** Dropout scenario: how many sensors go quiet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Simulation|Donnees", meta = (ClampMin = "1"))
	int32 DropoutCount = 5;

	/** Seed of every deterministic variation. Same seed, same audience. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Simulation|Donnees")
	int32 Seed = 1730;

	// ================================================== Control

	/** Start simulating as soon as the game starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Simulation")
	bool bSimulateOnBeginPlay = true;

	/**
	 * Step aside when real network data arrives.
	 *
	 * The whole point of this actor is to be replaced by the real Max patch
	 * without touching anything: as soon as OSC messages come off the socket,
	 * the simulator pauses, and it resumes only after the network has been
	 * silent for a few seconds. Disable to force both at once (they will fight
	 * over the same individuals - only useful to observe exactly that).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Simulation")
	bool bYieldToRealNetwork = true;

	UFUNCTION(BlueprintCallable, Category = "VibH2O|Simulation")
	void StartSimulation();

	UFUNCTION(BlueprintCallable, Category = "VibH2O|Simulation")
	void StopSimulation();

	UFUNCTION(BlueprintPure, Category = "VibH2O|Simulation")
	bool IsSimulating() const { return bSimulating; }

	/** Sends the full room plan immediately, as one burst. */
	UFUNCTION(BlueprintCallable, Category = "VibH2O|Simulation")
	void SendRoomNow();

	/** Configures the real Ales room: 23 x 4, aisles at columns 6 and 16. */
	UFUNCTION(BlueprintCallable, Category = "VibH2O|Simulation")
	void ConfigureAles();

	/**
	 * Resizes the room while it is running.
	 *
	 * The new plan goes out immediately, and the stage actor reconciles: seats
	 * that survive keep their bubble, the rest are created or destroyed. It is
	 * the same path a resize from Max would take.
	 */
	UFUNCTION(BlueprintCallable, Category = "VibH2O|Simulation")
	void SetRoomSize(int32 InColumns, int32 InRows);

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	UVibH2OSubsystem* GetSubsystem() const;

	void FeedData();
	bool IsSeatEmpty(int32 Column, int32 Row, int32 SeatIndex) const;

	/** Excitation and synchrony targets for one seat, per the scenario. */
	void ScenarioTargets(int32 Column, int32 Row, int32 Id, float& OutExcitation, float& OutSynchrony) const;

	bool bSimulating = false;
	bool bAnnouncedFirstFeed = false;
	bool bReportedYield = false;

	/** Scenario clock, seconds since the simulation started. */
	float SimTime = 0.0f;

	double NextFeedTime = 0.0;
	double NextRoomTime = 0.0;

	/** Network yield: last seen receiver message count, and hold deadline. */
	int32 LastNetworkMessageCount = 0;
	double RealDataHoldUntil = 0.0;
};
