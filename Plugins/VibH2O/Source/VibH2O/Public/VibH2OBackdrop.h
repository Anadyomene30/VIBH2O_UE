// The backdrop switch: one command that fades the underwater environment out,
// leaving the bubbles and the particles alone on pure black.
//
// On a projection tulle, black is not a colour, it is transparency: what the
// image does not light, the audience sees through. Removing the backdrop is
// therefore how the performers behind the tulle come forward, and the black
// has to be true black, not a very dark blue.
//
// How it fades. The water's glow is the fog's in-scattering: the exponential
// fog's colour, its directional term and the volumetric fog's albedo and
// emission. Fading those to black leaves the fog absorbing but no longer
// glowing, so the bubbles sink into a black abyss instead of popping out,
// crisp, against nothing - the depth cue survives the backdrop. The sky
// atmosphere follows, and meshes (a sky sphere, a water surface) are hidden
// once the fade has reached black. Lights are left alone unless asked:
// the bubbles keep exactly the lighting they had.
//
// The backdrop never writes over the environment for good. The values it
// replaces are kept in this actor (and saved with the map), and handed back
// the moment the backdrop is fully present again; deleting the actor hands
// them back too.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/Scene.h"
#include "InputCoreTypes.h"

#include "VibH2OBackdrop.generated.h"

class UActorComponent;
class UMaterialParameterCollection;
class UPostProcessComponent;
class UVibH2OSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVibH2OBackdropFadeSignature, float, Presence);

/** What the backdrop changed on one component, so it can be given back. */
USTRUCT()
struct VIBH2O_API FVibH2OBackdropSnapshot
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UActorComponent> Component;

	UPROPERTY()
	FLinearColor FogInscattering = FLinearColor::Black;

	UPROPERTY()
	FLinearColor FogDirectionalInscattering = FLinearColor::Black;

	UPROPERTY()
	FLinearColor FogInscatteringTextureTint = FLinearColor::Black;

	UPROPERTY()
	FColor VolumetricFogAlbedo = FColor::Black;

	UPROPERTY()
	FLinearColor VolumetricFogEmissive = FLinearColor::Black;

	UPROPERTY()
	FLinearColor SkyLuminance = FLinearColor::Black;

	UPROPERTY()
	float Intensity = 0.0f;

	UPROPERTY()
	float VolumetricScattering = 0.0f;

	UPROPERTY()
	bool bVisible = true;
};

/**
 * Fades the underwater environment to black and back.
 */
UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "VibH2O — Fond"))
class VIBH2O_API AVibH2OBackdrop : public AActor
{
	GENERATED_BODY()

public:
	AVibH2OBackdrop();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Destroyed() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override;

	/**
	 * How present the environment is: 1 the water as designed, 0 pure black
	 * behind the bubbles. This is the value Sequencer animates; the operator
	 * commands below simply glide it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Fond", meta = (DisplayName = "Presence du fond", ClampMin = "0.0", ClampMax = "1.0"))
	float Presence = 1.0f;

	/** Duration of the operator fade, in seconds. 0 cuts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Fond", meta = (DisplayName = "Duree du fondu", ClampMin = "0.0", ForceUnits = "s"))
	float FadeSeconds = 4.0f;

	/**
	 * What makes up the environment. Fogs and sky atmospheres are faded;
	 * meshes are hidden once black is reached; lights (sky light included)
	 * only if "Eteindre les lumieres" is ticked. Particle systems are never
	 * touched, even inside these actors: they are what remains on black.
	 * "Remplir depuis la carte" fills the list with the obvious candidates.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Fond", meta = (DisplayName = "Environnement"))
	TArray<TObjectPtr<AActor>> EnvironmentActors;

	/**
	 * Also fade the lights of the environment actors (sun, sky light). Off by
	 * default: they light the bubbles too, and the point of removing the
	 * backdrop is to change what is behind the bubbles, not the bubbles.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Fond", meta = (DisplayName = "Eteindre les lumieres"))
	bool bFadeLights = false;

	/**
	 * Optional collection for the artist's own materials - a caustic, a water
	 * surface - so they can fade themselves smoothly rather than vanish at
	 * the end. Receives Presence under the name below.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Fond", meta = (DisplayName = "Collection de parametres"))
	TObjectPtr<UMaterialParameterCollection> ParameterCollection;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Fond", meta = (DisplayName = "Nom du parametre"))
	FName ParameterName = TEXT("Fond");

	/**
	 * Post-process overrides that fade in as the backdrop leaves - the image
	 * treatment of the black, not of the water. By default it switches off
	 * the bloom's dirt mask: on black, the lens dirt the bloom lights up
	 * around every bubble would print smudges onto the tulle. Exposure is
	 * deliberately not touched, so the bubbles keep their brightness.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Fond", meta = (DisplayName = "Post-process au noir"))
	FPostProcessSettings BlackPostProcess;

	/** Key that toggles the backdrop in Play. B is free in the show's inputs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Commande", meta = (DisplayName = "Touche"))
	FKey ToggleKey = EKeys::B;

	/** Obey /Fond messages (the iPad, or Max). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Commande", meta = (DisplayName = "Suivre l'OSC"))
	bool bFollowOsc = true;

	/** Fades the environment out. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "VibH2O|Fond", meta = (DisplayName = "Enlever le fond"))
	void RemoveBackdrop();

	/** Fades the environment back in. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "VibH2O|Fond", meta = (DisplayName = "Remettre le fond"))
	void RestoreBackdrop();

	/** Removes it if it is (heading towards) present, brings it back otherwise. */
	UFUNCTION(BlueprintCallable, Category = "VibH2O|Fond")
	void ToggleBackdrop();

	/** Glides Presence to Target. Seconds < 0 uses FadeSeconds. */
	UFUNCTION(BlueprintCallable, Category = "VibH2O|Fond")
	void FadeTo(float Target, float Seconds = -1.0f);

	/** Weight currently given to the post-process au noir: 1 - Presence. */
	float GetBlackPostProcessWeight() const;

	/** Where the current fade is heading, or Presence when none runs. */
	UFUNCTION(BlueprintPure, Category = "VibH2O|Fond")
	float GetTargetPresence() const { return bFading ? FadeTarget : Presence; }

	/**
	 * Fills Environnement with the level's fogs, sky atmospheres, sky lights
	 * and very large meshes (sky spheres). Leaves out VibH2O's own actors and
	 * every particle system: those are what remains on black.
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "VibH2O|Fond", meta = (DisplayName = "Remplir depuis la carte"))
	void CollectEnvironmentFromLevel();

	/** Raised whenever the applied presence changes. */
	UPROPERTY(BlueprintAssignable, Category = "VibH2O|Evenements")
	FVibH2OBackdropFadeSignature OnPresenceChanged;

	/** Pushes Presence to the environment now. Called every tick. */
	UFUNCTION(BlueprintCallable, Category = "VibH2O|Fond")
	void ApplyPresence();

	/** Gives every changed value back, and forgets it. */
	void RestoreEnvironment();

private:
	/**
	 * How much of each glowing quantity survives at a given presence.
	 *
	 * Presence is what the operator and Sequencer move; this decides how the
	 * fade FEELS on screen.
	 */
	static float LightFactor(float InPresence);

	void Capture(UActorComponent* Component);
	void ApplyTo(const FVibH2OBackdropSnapshot& Snapshot, float Factor, bool bAtBlack) const;
	FVibH2OBackdropSnapshot* FindSnapshot(const UActorComponent* Component);
	void GatherComponents(TArray<UActorComponent*>& Out) const;

	void BindToSubsystem(UVibH2OSubsystem* Subsystem);
	void UnbindFromSubsystem();

	UFUNCTION()
	void HandleOscBackdrop(float TargetPresence, float InFadeSeconds);

	/** The environment's own values, kept while the backdrop is not fully present. */
	UPROPERTY()
	TArray<FVibH2OBackdropSnapshot> Snapshots;

	TWeakObjectPtr<UVibH2OSubsystem> BoundSubsystem;

	/** Carries BlackPostProcess, weighted by 1 - Presence, above every volume. */
	UPROPERTY(VisibleAnywhere, Category = "VibH2O|Fond")
	TObjectPtr<UPostProcessComponent> BlackPostProcessComponent;

	bool bFading = false;
	float FadeFrom = 1.0f;
	float FadeTarget = 1.0f;
	float FadeDuration = 0.0f;
	float FadeElapsed = 0.0f;

	/** Last value pushed, so a still backdrop costs nothing per frame. */
	float AppliedPresence = 1.0f;
};
