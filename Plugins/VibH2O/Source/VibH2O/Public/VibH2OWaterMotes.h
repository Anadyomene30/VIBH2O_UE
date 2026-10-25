// Motes suspended in the water.
//
// Real water is never empty: it carries plankton, silt and bubbles that catch
// the light, and their parallax is most of what tells the eye it is looking
// through a volume rather than at a backdrop.
//
// They are plain static mesh components, not particles - one per mote, so a
// few hundred at most (the instanced path rendered black in this scene, see
// Rebuild). The drift is a single rotation of the whole field per frame, so
// the CPU does almost nothing once they are placed.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "VibH2OWaterMotes.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

UCLASS(Blueprintable, BlueprintType, HideCategories = (Actor, Navigation, Tags, ComponentTags, Cooking, AssetUserData, Activation, Rendering, Replication, Collision, HLOD, Physics, Networking, Input, LOD, Cooking, DataLayers, WorldPartition, LevelInstance), PrioritizeCategories = ("VibH2O"), AutoExpandCategories = ("VibH2O|Particules"), meta = (DisplayName = "VibH2O — Particules d'eau"))
class VIBH2O_API AVibH2OWaterMotes : public AActor
{
	GENERATED_BODY()

public:
	AVibH2OWaterMotes();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override;

	/** How many motes. Each one is a component: stay in the hundreds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Particules", meta = (DisplayName = "Nombre", ClampMin = "0", ClampMax = "800", NoSpinbox = "true", ToolTip = "Nombre de particules (800 au plus). Chacune est un composant : rester sous 400 si l'image rame."))
	int32 Count = 320;

	/** Radius of the volume they fill, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Particules", meta = (DisplayName = "Rayon du volume", ClampMin = "50.0", UIMax = "6000.0", ForceUnits = "cm"))
	float Radius = 2600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Particules", meta = (DisplayName = "Hauteur du volume", ClampMin = "50.0", UIMax = "5000.0", ForceUnits = "cm"))
	float Height = 2000.0f;

	/** Smallest and largest mote, in cm. The spread is what gives depth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Particules", meta = (DisplayName = "Taille min", ClampMin = "0.1", UIMax = "60.0", ForceUnits = "cm"))
	float MinSize = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Particules", meta = (DisplayName = "Taille max", ClampMin = "0.1", UIMax = "60.0", ForceUnits = "cm"))
	float MaxSize = 15.0f;

	/** Same seed, same water. Changing it reshuffles the whole field. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Particules", meta = (DisplayName = "Graine"))
	int32 Seed = 7412;

	/**
	 * Speed of the slow current carrying the whole field, in turns per second.
	 *
	 * The field drifts as one body rather than mote by mote: suspended silt in
	 * a current does exactly that, and it costs a single transform per frame
	 * instead of one per mote.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Particules", meta = (DisplayName = "Courant (tours/s)", UIMin = "-0.05", UIMax = "0.05"))
	float CurrentSpeed = 0.006f;

	/** Rebuilds the field. Called on construction; exposed for Blueprint too. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "VibH2O", meta = (DisplayName = "Régénérer"))
	void Rebuild();

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Spawned;

	UPROPERTY()
	TObjectPtr<UStaticMesh> MoteMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> MoteMaterial;
};
