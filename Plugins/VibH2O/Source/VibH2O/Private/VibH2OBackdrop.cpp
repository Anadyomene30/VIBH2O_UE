#include "VibH2OBackdrop.h"

#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InputComponent.h"
#include "Components/LightComponent.h"
#include "Components/MeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "UObject/Package.h"
#include "VibH2OModule.h"
#include "VibH2OSubsystem.h"

namespace VibH2OBackdropInternal
{
	/** Below this presence the environment counts as gone, and meshes hide. */
	static constexpr float BlackThreshold = 1.0e-3f;

	/** A mesh this large is a sky sphere or a horizon, not a prop. In cm. */
	static constexpr float SkyMeshRadius = 100000.0f;

	static bool IsPluginActor(const AActor* Actor)
	{
		// Native or Blueprint-derived: walk up to the first native class.
		for (const UClass* Class = Actor->GetClass(); Class != nullptr; Class = Class->GetSuperClass())
		{
			if (Class->HasAnyClassFlags(CLASS_Native))
			{
				return Class->GetOutermost()->GetName().StartsWith(TEXT("/Script/VibH2O"));
			}
		}
		return false;
	}

	/** Scales a colour's light, never its alpha. */
	static FLinearColor Dim(const FLinearColor& Color, float Factor)
	{
		return FLinearColor(Color.R * Factor, Color.G * Factor, Color.B * Factor, Color.A);
	}

	static FColor Dim(const FColor& Color, float Factor)
	{
		FColor Result = Dim(FLinearColor(Color), Factor).ToFColor(true);
		Result.A = Color.A;
		return Result;
	}

	static bool IsLight(const UActorComponent* Component)
	{
		return Component->IsA<ULightComponent>() || Component->IsA<USkyLightComponent>();
	}

	static bool IsFadeable(const UActorComponent* Component)
	{
		return Component->IsA<UExponentialHeightFogComponent>() || Component->IsA<USkyAtmosphereComponent>();
	}

	static bool IsHideable(const UActorComponent* Component)
	{
		// Meshes only: particle systems are what the audience keeps on black.
		return Component->IsA<UMeshComponent>() && !Component->IsEditorOnly();
	}
}

AVibH2OBackdrop::AVibH2OBackdrop()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("BackdropRoot")));

	BlackPostProcessComponent = CreateDefaultSubobject<UPostProcessComponent>(TEXT("BlackPostProcess"));
	BlackPostProcessComponent->SetupAttachment(GetRootComponent());
	BlackPostProcessComponent->bUnbound = true;
	// Above the show's volumes (10, 20): only what it overrides wins.
	BlackPostProcessComponent->Priority = 1000.0f;
	BlackPostProcessComponent->BlendWeight = 0.0f;

	BlackPostProcess.bOverride_BloomDirtMaskIntensity = true;
	BlackPostProcess.BloomDirtMaskIntensity = 0.0f;
}

void AVibH2OBackdrop::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Edited in the Details panel, scrubbed in Sequencer, or just loaded with
	// the map: in every case the environment must match Presence at once.
	ApplyPresence();
}

void AVibH2OBackdrop::BeginPlay()
{
	Super::BeginPlay();

	if (UVibH2OSubsystem* Subsystem = UVibH2OSubsystem::Get(this))
	{
		BindToSubsystem(Subsystem);
	}

	UWorld* World = GetWorld();
	if (ToggleKey.IsValid() && World != nullptr && World->IsGameWorld())
	{
		if (APlayerController* Controller = World->GetFirstPlayerController())
		{
			EnableInput(Controller);
			if (InputComponent != nullptr)
			{
				InputComponent->BindKey(ToggleKey, IE_Pressed, this, &AVibH2OBackdrop::ToggleBackdrop);
			}
		}
		else
		{
			UE_LOG(LogVibH2O, Warning, TEXT("VibH2O: fond '%s' sans controleur joueur — la touche %s est inactive."),
				*GetName(), *ToggleKey.ToString());
		}
	}

	ApplyPresence();
}

void AVibH2OBackdrop::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromSubsystem();
	Super::EndPlay(EndPlayReason);
}

void AVibH2OBackdrop::Destroyed()
{
	// Deleting the switch in the editor must not leave the water black for
	// good: what it took, it gives back.
	RestoreEnvironment();
	Super::Destroyed();
}

bool AVibH2OBackdrop::ShouldTickIfViewportsOnly() const
{
	// The two buttons fade in the editor viewport too, without Play.
	return true;
}

void AVibH2OBackdrop::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bFading)
	{
		FadeElapsed += DeltaSeconds;
		const float Alpha = FadeDuration > 0.0f ? FMath::Clamp(FadeElapsed / FadeDuration, 0.0f, 1.0f) : 1.0f;
		// Eased at both ends: a fade that starts or lands abruptly reads as a
		// cut on a large surface.
		Presence = FMath::Lerp(FadeFrom, FadeTarget, FMath::SmoothStep(0.0f, 1.0f, Alpha));
		if (Alpha >= 1.0f)
		{
			Presence = FadeTarget;
			bFading = false;
		}
	}

	ApplyPresence();
}

void AVibH2OBackdrop::RemoveBackdrop()
{
	FadeTo(0.0f);
}

void AVibH2OBackdrop::RestoreBackdrop()
{
	FadeTo(1.0f);
}

void AVibH2OBackdrop::ToggleBackdrop()
{
	FadeTo(GetTargetPresence() > 0.5f ? 0.0f : 1.0f);
}

void AVibH2OBackdrop::FadeTo(float Target, float Seconds)
{
	FadeFrom = Presence;
	FadeTarget = FMath::Clamp(Target, 0.0f, 1.0f);
	FadeDuration = Seconds < 0.0f ? FadeSeconds : Seconds;
	FadeElapsed = 0.0f;
	bFading = true;

	UE_LOG(LogVibH2O, Log, TEXT("VibH2O: fond %s en %.1f s."),
		FadeTarget < 0.5f ? TEXT("enleve") : TEXT("remis"), FadeDuration);

	if (FadeDuration <= 0.0f)
	{
		Presence = FadeTarget;
		bFading = false;
		ApplyPresence();
	}
}

float AVibH2OBackdrop::GetBlackPostProcessWeight() const
{
	return BlackPostProcessComponent != nullptr ? BlackPostProcessComponent->BlendWeight : 0.0f;
}

float AVibH2OBackdrop::LightFactor(float InPresence)
{
	// The eye reads brightness roughly as a cube root, so fading light
	// linearly looks like nothing happens for most of the fade and then a
	// sudden drop at the end. Raising presence to the display gamma makes
	// the perceived darkening follow the fade evenly.
	return FMath::Pow(FMath::Clamp(InPresence, 0.0f, 1.0f), 2.2f);
}

void AVibH2OBackdrop::ApplyPresence()
{
	Presence = FMath::Clamp(Presence, 0.0f, 1.0f);

	// Cheap and stateless: refreshed every time, so edits to the settings in
	// the Details panel show at once.
	if (BlackPostProcessComponent != nullptr)
	{
		BlackPostProcessComponent->Settings = BlackPostProcess;
		BlackPostProcessComponent->BlendWeight = 1.0f - Presence;
	}

	// Fully present: hand everything back and touch nothing more, so the
	// environment stays exactly as its author left it - and editable.
	if (Presence >= 1.0f)
	{
		if (Snapshots.Num() > 0)
		{
			RestoreEnvironment();
		}
		if (AppliedPresence != Presence)
		{
			AppliedPresence = Presence;
			if (ParameterCollection != nullptr && GetWorld() != nullptr)
			{
				UKismetMaterialLibrary::SetScalarParameterValue(this, ParameterCollection, ParameterName, Presence);
			}
			OnPresenceChanged.Broadcast(Presence);
		}
		return;
	}

	TArray<UActorComponent*> Components;
	GatherComponents(Components);

	// Values are captured the moment a component starts leaving, not before:
	// whatever the artist set while the backdrop was present is what returns.
	bool bNewComponent = false;
	for (UActorComponent* Component : Components)
	{
		if (FindSnapshot(Component) == nullptr)
		{
			Capture(Component);
			bNewComponent = true;
		}
	}

	if (!bNewComponent && AppliedPresence == Presence)
	{
		return;
	}
	AppliedPresence = Presence;

	const float Factor = LightFactor(Presence);
	const bool bAtBlack = Presence <= VibH2OBackdropInternal::BlackThreshold;
	for (const FVibH2OBackdropSnapshot& Snapshot : Snapshots)
	{
		ApplyTo(Snapshot, Factor, bAtBlack);
	}

	if (ParameterCollection != nullptr && GetWorld() != nullptr)
	{
		UKismetMaterialLibrary::SetScalarParameterValue(this, ParameterCollection, ParameterName, Presence);
	}
	OnPresenceChanged.Broadcast(Presence);
}

void AVibH2OBackdrop::RestoreEnvironment()
{
	for (const FVibH2OBackdropSnapshot& Snapshot : Snapshots)
	{
		ApplyTo(Snapshot, 1.0f, false);
	}
	Snapshots.Reset();
}

void AVibH2OBackdrop::GatherComponents(TArray<UActorComponent*>& Out) const
{
	using namespace VibH2OBackdropInternal;

	for (const AActor* Actor : EnvironmentActors)
	{
		if (!IsValid(Actor) || Actor == this)
		{
			continue;
		}
		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (Component == nullptr)
			{
				continue;
			}
			if (IsFadeable(Component) || IsHideable(Component) || (bFadeLights && IsLight(Component)))
			{
				Out.Add(Component);
			}
		}
	}
}

FVibH2OBackdropSnapshot* AVibH2OBackdrop::FindSnapshot(const UActorComponent* Component)
{
	return Snapshots.FindByPredicate([Component](const FVibH2OBackdropSnapshot& Snapshot)
	{
		return Snapshot.Component == Component;
	});
}

void AVibH2OBackdrop::Capture(UActorComponent* Component)
{
	FVibH2OBackdropSnapshot& Snapshot = Snapshots.AddDefaulted_GetRef();
	Snapshot.Component = Component;

	if (const UExponentialHeightFogComponent* Fog = Cast<UExponentialHeightFogComponent>(Component))
	{
		Snapshot.FogInscattering = Fog->FogInscatteringLuminance;
		Snapshot.FogDirectionalInscattering = Fog->DirectionalInscatteringLuminance;
		Snapshot.FogInscatteringTextureTint = Fog->InscatteringTextureTint;
		Snapshot.VolumetricFogAlbedo = Fog->VolumetricFogAlbedo;
		Snapshot.VolumetricFogEmissive = Fog->VolumetricFogEmissive;
	}
	else if (const USkyAtmosphereComponent* Sky = Cast<USkyAtmosphereComponent>(Component))
	{
		Snapshot.SkyLuminance = Sky->SkyLuminanceFactor;
	}
	else if (const ULightComponentBase* Light = Cast<ULightComponentBase>(Component))
	{
		Snapshot.Intensity = Light->Intensity;
		Snapshot.VolumetricScattering = Light->VolumetricScatteringIntensity;
	}
	else if (const USceneComponent* Scene = Cast<USceneComponent>(Component))
	{
		Snapshot.bVisible = Scene->GetVisibleFlag();
	}
}

void AVibH2OBackdrop::ApplyTo(const FVibH2OBackdropSnapshot& Snapshot, float Factor, bool bAtBlack) const
{
	using VibH2OBackdropInternal::Dim;

	UActorComponent* Component = Snapshot.Component;
	if (!IsValid(Component))
	{
		return;
	}

	if (UExponentialHeightFogComponent* Fog = Cast<UExponentialHeightFogComponent>(Component))
	{
		// Only what the fog emits goes dark. Its density stays: it still
		// swallows distant bubbles, now into black instead of blue.
		Fog->SetFogInscatteringColor(Dim(Snapshot.FogInscattering, Factor));
		Fog->SetDirectionalInscatteringColor(Dim(Snapshot.FogDirectionalInscattering, Factor));
		Fog->SetInscatteringTextureTint(Dim(Snapshot.FogInscatteringTextureTint, Factor));
		// The albedo is the share of light the volume scatters back - every
		// shaft, every glow around a lamp. At black it scatters nothing.
		Fog->SetVolumetricFogAlbedo(Dim(Snapshot.VolumetricFogAlbedo, Factor));
		Fog->SetVolumetricFogEmissive(Dim(Snapshot.VolumetricFogEmissive, Factor));
	}
	else if (USkyAtmosphereComponent* Sky = Cast<USkyAtmosphereComponent>(Component))
	{
		Sky->SetSkyLuminanceFactor(Dim(Snapshot.SkyLuminance, Factor));
	}
	else if (ULightComponent* Light = Cast<ULightComponent>(Component))
	{
		Light->SetIntensity(Snapshot.Intensity * Factor);
		Light->SetVolumetricScatteringIntensity(Snapshot.VolumetricScattering * Factor);
	}
	else if (USkyLightComponent* SkyLight = Cast<USkyLightComponent>(Component))
	{
		SkyLight->SetIntensity(Snapshot.Intensity * Factor);
		SkyLight->SetVolumetricScatteringIntensity(Snapshot.VolumetricScattering * Factor);
	}
	else if (USceneComponent* Scene = Cast<USceneComponent>(Component))
	{
		// A mesh cannot fade on its own without its material's help (see the
		// parameter collection): it waits for black, where nothing shows the
		// moment it goes.
		Scene->SetVisibility(Snapshot.bVisible && !bAtBlack);
	}
}

void AVibH2OBackdrop::CollectEnvironmentFromLevel()
{
	using namespace VibH2OBackdropInternal;

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	// The list cannot change under a faded backdrop: the values it holds
	// belong to the old list.
	RestoreEnvironment();
	Presence = 1.0f;
	bFading = false;

	Modify();
	EnvironmentActors.Reset();

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor == this || IsPluginActor(Actor))
		{
			continue;
		}

		bool bEnvironment = false;
		for (const UActorComponent* Component : Actor->GetComponents())
		{
			if (Component == nullptr)
			{
				continue;
			}
			if (IsFadeable(Component) || Component->IsA<USkyLightComponent>())
			{
				bEnvironment = true;
				break;
			}
			if (const UMeshComponent* Mesh = Cast<UMeshComponent>(Component))
			{
				if (!Mesh->IsEditorOnly() && Mesh->Bounds.SphereRadius > SkyMeshRadius)
				{
					bEnvironment = true;
					break;
				}
			}
		}
		if (bEnvironment)
		{
			EnvironmentActors.Add(Actor);
		}
	}

	UE_LOG(LogVibH2O, Log, TEXT("VibH2O: fond — %d acteurs d'environnement trouves."), EnvironmentActors.Num());
}

void AVibH2OBackdrop::BindToSubsystem(UVibH2OSubsystem* Subsystem)
{
	if (Subsystem == nullptr || BoundSubsystem.Get() == Subsystem)
	{
		return;
	}
	UnbindFromSubsystem();
	Subsystem->OnOscBackdropChanged.AddDynamic(this, &AVibH2OBackdrop::HandleOscBackdrop);
	BoundSubsystem = Subsystem;
}

void AVibH2OBackdrop::UnbindFromSubsystem()
{
	if (UVibH2OSubsystem* Subsystem = BoundSubsystem.Get())
	{
		Subsystem->OnOscBackdropChanged.RemoveDynamic(this, &AVibH2OBackdrop::HandleOscBackdrop);
	}
	BoundSubsystem.Reset();
}

void AVibH2OBackdrop::HandleOscBackdrop(float TargetPresence, float InFadeSeconds)
{
	if (bFollowOsc)
	{
		FadeTo(TargetPresence, InFadeSeconds);
	}
}
