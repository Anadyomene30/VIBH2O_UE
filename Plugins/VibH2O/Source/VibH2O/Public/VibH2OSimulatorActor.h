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
	Dropout UMETA(DisplayName = "Coupure de capteurs"),

	/**
	 * The room's three moods in turn: a collective tide holding calm, then
	 * normal, then excited, then back, with synchrony on a period of its own
	 * so every mood is seen in step and out of step.
	 */
	MoodWave UMETA(DisplayName = "Vague collective : calme, normal, excité")
};

/**
 * In-level stand-in for the Max patch.
 *
 * It knows nothing of bubbles or stage actors: it only pushes OSC messages
 * into the subsystem, exactly as the network thread would. Everything
 * downstream - burst settling, seat-order mapping, smoothing, staleness,
 * averages - therefore runs the real code, not a shortcut.
 */
UCLASS(Blueprintable, BlueprintType, HideCategories = (Rendering, Replication, Collision, HLOD, Physics, Networking, Input, LOD, Cooking, DataLayers, WorldPartition, LevelInstance), PrioritizeCategories = ("VibH2O"), meta = (DisplayName = "VibH2O — Simulateur"))
class VIBH2O_API AVibH2OSimulatorActor : public AActor
{
	GENERATED_BODY()

public:
	AVibH2OSimulatorActor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override;
	virtual void Destroyed() override;

	// ================================================== Room plan

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Simulation|Salle", meta = (DisplayName = "Colonnes", ClampMin = "1", ClampMax = "64", ToolTip = "Nombre de colonnes de la salle simulée, allées comprises."))
	int32 Columns = 7;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Simulation|Salle", meta = (DisplayName = "Rangées", ClampMin = "1", ClampMax = "32", ToolTip = "Nombre de rangées de la salle simulée."))
	int32 Rows = 3;

	/** Columns left entirely empty - the aisles. Ales has 6 and 16. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Simulation|Salle", meta = (DisplayName = "Allées (colonnes vides, 0 = 1re colonne)"))
	TArray<int32> AisleColumns;

	/** Extra empty seats, by OSC seat index (1-based), for scattered holes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Simulation|Salle", meta = (DisplayName = "Sièges vides en plus (n° OSC, 1 = premier)"))
	TArray<int32> ExtraEmptySeatIndices;

	/**
	 * Interval between full room-plan resends, in seconds. 0 sends the plan
	 * once at start. Resending exercises the rebuild-by-difference path: the
	 * room must NOT flicker when an identical plan comes in again.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Simulation|Salle", AdvancedDisplay, meta = (DisplayName = "Renvoyer le plan toutes les", ClampMin = "0.0", ForceUnits = "s", ToolTip = "0 : le plan part une fois au démarrage."))
	float RoomResendInterval = 0.0f;

	// ================================================== Data stream

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Simulation|Donnees", meta = (DisplayName = "Scénario", ToolTip = "Ce que fait le public simulé : calme, vague d'excitation, balayage de synchronie, pic collectif, coupure de capteurs, ou les trois ambiances à tour de rôle."))
	EVibH2OSimulatorScenario Scenario = EVibH2OSimulatorScenario::Nominal;

	/** Messages per individual per second. Max sends in this ballpark. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Simulation|Donnees", AdvancedDisplay, meta = (DisplayName = "Cadence d'envoi", ClampMin = "0.5", ClampMax = "60.0"))
	float SendRateHz = 10.0f;

	/** Resting heart-rate range the simulated audience is drawn from. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Simulation|Donnees", meta = (DisplayName = "BPM au repos : min", ClampMin = "30.0", ClampMax = "200.0", UIMin = "40.0", UIMax = "140.0"))
	float BaseBpmMin = 58.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Simulation|Donnees", meta = (DisplayName = "BPM au repos : max", ClampMin = "30.0", ClampMax = "200.0", UIMin = "40.0", UIMax = "140.0", ToolTip = "Chaque spectateur simulé tire son BPM au repos entre le min et le max ; l'excitation l'élève ensuite."))
	float BaseBpmMax = 92.0f;

	/** Time scale of the scenario waveforms. 1 matches the Python simulator. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Simulation|Donnees", AdvancedDisplay, meta = (DisplayName = "Vitesse du scénario", ClampMin = "0.1", ClampMax = "10.0"))
	float ScenarioSpeed = 1.0f;

	/** Dropout scenario: seconds before sensors start going quiet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Simulation|Donnees", meta = (DisplayName = "Coupure : après", ClampMin = "0.0", ForceUnits = "s", EditCondition = "Scenario == EVibH2OSimulatorScenario::Dropout", EditConditionHides))
	float DropoutAfterSeconds = 8.0f;

	/** Dropout scenario: how many sensors go quiet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Simulation|Donnees", meta = (DisplayName = "Coupure : capteurs muets", ClampMin = "1", UIMax = "50", EditCondition = "Scenario == EVibH2OSimulatorScenario::Dropout", EditConditionHides))
	int32 DropoutCount = 5;

	/** Seed of every deterministic variation. Same seed, same audience. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Simulation|Donnees", AdvancedDisplay, meta = (DisplayName = "Graine (même graine, même public)"))
	int32 Seed = 1730;

	// ================================================== Control

	/** Start simulating as soon as the game starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Simulation", meta = (DisplayName = "Simuler au lancement (Play, jeu packagé)", ToolTip = "Décocher dans la carte du spectacle, ou supprimer l'acteur. Coché, il remplace Max tant que Max n'a rien envoyé."))
	bool bSimulateOnBeginPlay = true;

	/**
	 * Simulate in the editor too, with no Play: the room lives in the
	 * viewport while it is being lit and dressed.
	 *
	 * The data goes to the editor's own stand-in subsystem, which never opens
	 * the network port - so pressing Play still gets a clean, real subsystem,
	 * and Max still reaches it. Untick to freeze the editor room and clear it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Simulation", meta = (DisplayName = "Simuler dans l'éditeur"))
	bool bSimulateInEditor = true;

	/**
	 * Step aside when real network data arrives.
	 *
	 * The whole point of this actor is to be replaced by the real Max patch
	 * without touching anything: as soon as data messages (room plan, BPM,
	 * excitation, synchrony, beat) come off the socket, the simulator pauses,
	 * and it resumes only after the network has been silent for a few seconds.
	 * Other traffic - Resolume, the iPad - does not count. Disable to force
	 * both at once (they will fight over the same individuals - only useful to
	 * observe exactly that).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Simulation", meta = (DisplayName = "S'effacer quand Max émet", ToolTip = "Coché : dès que Max envoie des données (plan de salle, BPM, excitation, synchronie, battements), le simulateur se met en pause. Les messages de Resolume ou de l'iPad ne comptent pas."))
	bool bYieldToRealNetwork = true;

	/**
	 * Once Max has spoken, never resume.
	 *
	 * Without it, the simulator takes the stage back after a few seconds of
	 * network silence - and a patch that pauses mid-show would see fake heart
	 * rates injected in front of the audience. Ticked, the first real data
	 * message silences the simulator until the end of the game; only an
	 * explicit start (button, console, Blueprint) brings it back.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Simulation", meta = (DisplayName = "Se taire pour de bon dès que Max parle", EditCondition = "bYieldToRealNetwork", ToolTip = "Coché : dès que Max a envoyé des données une fois, le simulateur se tait jusqu'à la fin de la partie. À laisser coché en représentation."))
	bool bStopForGoodOnRealData = true;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "VibH2O|Simulation", meta = (DisplayName = "Démarrer la simulation"))
	void StartSimulation();

	/**
	 * In the editor world, with bSimulateInEditor ticked, the next frame
	 * starts it again: untick that box to freeze the editor room.
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "VibH2O|Simulation", meta = (DisplayName = "Arrêter la simulation", ToolTip = "Arrête la simulation. Hors Play, décocher plutôt « Simuler dans l'éditeur » : sinon elle repart à l'image suivante."))
	void StopSimulation();

	UFUNCTION(BlueprintPure, Category = "VibH2O|Simulation")
	bool IsSimulating() const { return bSimulating; }

	/** Sends the full room plan immediately, as one burst. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "VibH2O|Simulation", meta = (DisplayName = "Renvoyer le plan de salle"))
	void SendRoomNow();

	/** Configures the real Ales room: 23 x 4, aisles at columns 6 and 16. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "VibH2O|Simulation", meta = (DisplayName = "Charger la salle d'Alès (23 × 4)"))
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
#if WITH_EDITOR
	/** Stops feeding the editor stand-in and empties its room. */
	void StopEditorSimulation();
#endif

	UVibH2OSubsystem* GetSubsystem() const;

	void FeedData();
	bool IsSeatEmpty(int32 Column, int32 Row, int32 SeatIndex) const;

	/** Excitation and synchrony targets for one seat, per the scenario. */
	void ScenarioTargets(int32 Column, int32 Row, int32 Id, float& OutExcitation, float& OutSynchrony) const;

	bool bSimulating = false;
	bool bAnnouncedFirstFeed = false;
	bool bReportedYield = false;

	/** Set when real data silenced the simulator for good; reset by StartSimulation. */
	bool bSilencedByRealData = false;

	/** Scenario clock, seconds since the simulation started. */
	float SimTime = 0.0f;

	double NextFeedTime = 0.0;
	double NextRoomTime = 0.0;

	/**
	 * The subsystem last fed. The editor's stand-in is rebuilt after every
	 * Play, empty: a new one gets the room again, or the editor would stay
	 * without a room until the simulator restarted.
	 */
	TWeakObjectPtr<UVibH2OSubsystem> FedSubsystem;

	/**
	 * Network yield: last seen count of network DATA messages (see
	 * UVibH2OSubsystem::GetNetworkDataMessageCount), and hold deadline.
	 */
	int32 LastNetworkMessageCount = 0;
	double RealDataHoldUntil = 0.0;
};
