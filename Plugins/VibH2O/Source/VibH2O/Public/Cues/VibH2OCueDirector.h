// The show's cue list: incoming MIDI, OSC or key -> an action on the show.
#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Cues/VibH2OCueTypes.h"
#include "GameFramework/Actor.h"

#include "VibH2OCueDirector.generated.h"

class UVibH2OSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVibH2OCueFiredSignature, const FString&, CueName, float, Value);

/**
 * Holds the cue list of a level, and plays it.
 *
 * Drop one in the level, fill in the list in the Details panel - or let it
 * learn: press "Apprendre", then launch the column in Resolume or hit the pad.
 * During the show, the operator stays in Resolume: MIDI and OSC reach Unreal
 * whichever window has the focus. See docs/CUES.md.
 *
 * Inputs reach it through the subsystem, which already owns the OSC receiver
 * and also owns the MIDI and keyboard inputs: one entry point, the same
 * threading rule for all three. The director only ever sees FVibH2OCueInput.
 *
 * Execution only happens in a game world (Play, standalone, packaged): a
 * Level Sequence does not play in the editor world. In the editor world the
 * director only learns.
 */
UCLASS(HideCategories = (Rendering, Replication, Collision, HLOD, Physics, Networking, Input, LOD, Cooking, DataLayers, WorldPartition, LevelInstance),
	PrioritizeCategories = ("VibH2O"),
	meta = (DisplayName = "VibH2O — Directeur de cues"))
class VIBH2O_API AVibH2OCueDirector : public AActor
{
	GENERATED_BODY()

public:
	AVibH2OCueDirector();

	// AActor
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void BeginDestroy() override;

	/** The cue list. Order matters only for the log: every matching cue fires. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Cues",
		meta = (TitleProperty = "Name", DisplayName = "Cues",
			ToolTip = "La liste des cues. Toutes celles qui correspondent à une entrée se déclenchent ; l'ordre ne compte que pour le log."))
	TArray<FVibH2OCue> Cues;

	/** Master switch, animatable: a sequence can mute the cues for a while. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Cues",
		meta = (DisplayName = "Cues actives", ToolTip = "Interrupteur général, animable : une séquence peut couper les cues un moment. Seuls la commande VibH2O.Cue et le bouton de test passent encore."))
	bool bCuesActive = true;

	/**
	 * A non-continuous cue does not fire twice within this delay. Resolume
	 * may send a column launch on several addresses, a pad may bounce.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Cues",
		meta = (DisplayName = "Anti-rebond", ClampMin = "0.0", UIMax = "2.0", ForceUnits = "s",
			ToolTip = "Une cue non continue ne se redéclenche pas dans ce délai : Resolume peut envoyer un lancement de colonne sur plusieurs adresses, un pad peut rebondir."))
	float MinRetriggerSeconds = 0.25f;

	/** Log every input that reaches the director, matched or not. For get-in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Cues",
		meta = (DisplayName = "Journaliser les entrées", ToolTip = "À décocher après la mise en place : écrit chaque entrée dans le log."))
	bool bLogInputs = false;

	/** Raised whenever a cue fires, whatever its action. */
	UPROPERTY(BlueprintAssignable, Category = "VibH2O|Cues")
	FVibH2OCueFiredSignature OnCueFired;

	/** Fires a cue by name, as "/Cue/<Name>" would. Returns false if unknown. */
	UFUNCTION(BlueprintCallable, Category = "VibH2O|Cues")
	bool FireCue(const FString& CueName, float Value = 1.0f);

	/** Feeds an input as if it had come from a device. For tests, and Blueprint. */
	UFUNCTION(BlueprintCallable, Category = "VibH2O|Cues")
	void HandleCueInput(const FVibH2OCueInput& Input);

	// ------------------------------------------------------------------ Test

	/** The cue the test button fires. Picked from the list's names. */
	UPROPERTY(EditAnywhere, Transient, Category = "VibH2O|Cues",
		meta = (DisplayName = "Cue à tester", GetOptions = "GetCueNames",
			ToolTip = "La cue que déclenche le bouton « Déclencher la cue choisie ». Ne marche qu'en Play."))
	FString TestCueName;

	/**
	 * Fires TestCueName, as "/Cue/<Name>" would. Outside Play nothing can play
	 * - a Level Sequence does not run in the editor world - so it only says
	 * so. Pressed on the level's director while Play runs, it fires the cue on
	 * the playing copy.
	 */
	UFUNCTION(CallInEditor, Category = "VibH2O|Cues", meta = (DisplayName = "Déclencher la cue choisie"))
	void FireTestCue();

	/** Names of the cues in the list, for the test picker. */
	UFUNCTION()
	TArray<FString> GetCueNames() const;

	// ----------------------------------------------------------------- Learn

	/** Cue to overwrite when learning; -1 appends a new one. */
	UPROPERTY(EditAnywhere, Category = "VibH2O|Apprentissage",
		meta = (DisplayName = "Cue à apprendre (-1 = nouvelle)", ClampMin = "-1",
			ToolTip = "Numéro de la cue dont « Apprendre » remplace le déclencheur (0 = la première). -1 en ajoute une nouvelle."))
	int32 LearnTargetIndex = -1;

	/** What learning is doing, or what it last captured. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "VibH2O|Apprentissage",
		meta = (DisplayName = "État", ToolTip = "Ce que fait l'apprentissage, ou ce qu'il a capturé en dernier."))
	FString LearnStatus;

#if WITH_EDITOR
	/**
	 * Listens for 30 seconds and takes the first MIDI, OSC or key input as
	 * the trigger of the cue to learn. Editor only, outside Play: the inputs
	 * are opened for the time of the capture and closed right after.
	 */
	UFUNCTION(CallInEditor, Category = "VibH2O|Apprentissage", meta = (DisplayName = "Apprendre"))
	void Learn();

	UFUNCTION(CallInEditor, Category = "VibH2O|Apprentissage", meta = (DisplayName = "Annuler l'apprentissage"))
	void CancelLearn();

	/** Lists the MIDI inputs of this PC into the status line. */
	UFUNCTION(CallInEditor, Category = "VibH2O|Apprentissage", meta = (DisplayName = "Lister les entrées MIDI"))
	void ListMidiInputs();
#endif

	/** True while a learn capture is open. */
	bool IsLearning() const { return bLearning; }

	/**
	 * Takes the input as the learned trigger. Called by the capture; public
	 * so the tests can drive the learn step without a device.
	 */
	void ApplyLearnedInput(const FVibH2OCueInput& Input);

	/** Listens to this subsystem's cue inputs. BeginPlay calls it; so do the tests. */
	void BindToSubsystem(UVibH2OSubsystem* Subsystem);
	void UnbindFromSubsystem();

private:
	void ExecuteCue(int32 CueIndex, float InputValue);
	void ExecuteSequenceAction(const FVibH2OCue& Cue);

	void HandleLearnInput(const FVibH2OCueInput& Input);
	void EndLearn(const FString& Status);
	bool TickLearn(float DeltaTime);

	TWeakObjectPtr<UVibH2OSubsystem> BoundSubsystem;
	FDelegateHandle InputHandle;

	/** Per-cue time of the last firing, for the anti-bounce. */
	TArray<double> LastFireTimes;

	bool bLearning = false;
	double LearnDeadline = 0.0;
	FTSTicker::FDelegateHandle LearnTickerHandle;

#if WITH_EDITOR
	FDelegateHandle PreBeginPieHandle;
#endif
};
