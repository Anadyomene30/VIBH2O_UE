// Motes suspended in the water.
//
// Real water is never empty: it carries plankton, silt and bubbles that catch
// the light, and their parallax is most of what tells the eye it is looking
// through a volume rather than at a backdrop.
//
// They are one instanced mesh component, not particles: several thousand
// instances cost one draw call, and the drift lives in the material's world
// position offset, so the CPU does nothing at all once they are placed.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "VibH2OWaterMotes.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "VibH2O — Particules d'eau"))
class VIBH2O_API AVibH2OWaterMotes : public AActor
{
	GENERATED_BODY()

public:
	AVibH2OWaterMotes();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override;

	/** How many motes. Several thousand stay cheap: it is one draw call. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Particules", meta = (ClampMin = "0", ClampMax = "800"))
	int32 Count = 320;

	/** Radius of the volume they fill, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Particules", meta = (ClampMin = "50.0", ForceUnits = "cm"))
	float Radius = 2600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Particules", meta = (ClampMin = "50.0", ForceUnits = "cm"))
	float Height = 2000.0f;

	/** Smallest and largest mote, in cm. The spread is what gives depth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Particules", meta = (ClampMin = "0.1"))
	float MinSize = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Particules", meta = (ClampMin = "0.1"))
	float MaxSize = 15.0f;

	/** Same seed, same water. Changing it reshuffles the whole field. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Particules")
	int32 Seed = 7412;

	/**
	 * Speed of the slow current carrying the whole field, in turns per second.
	 *
	 * The field drifts as one body rather than mote by mote: suspended silt in
	 * a current does exactly that, and it costs a single transform per frame
	 * instead of two thousand six hundred.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Particules")
	float CurrentSpeed = 0.006f;

	/** Rebuilds the field. Called on construction; exposed for Blueprint too. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "VibH2O|Particules")
	void Rebuild();

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Spawned;

	UPROPERTY()
	TObjectPtr<UStaticMesh> MoteMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> MoteMaterial;
};
