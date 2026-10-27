// Backdrop tests: the fade reaches true black, gives everything back, and
// answers the /Fond address.
//
// What matters on stage is checked here rather than by eye: that at presence 0
// the fog emits strictly nothing (a tulle shows the slightest grey), that the
// bubbles' lights are left alone, and that returning to 1 restores the
// environment to the exact values its author set.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"
#include "Osc/VibH2OOscTypes.h"
#include "VibH2OBackdrop.h"
#include "VibH2OSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2OBackdropFadeTest,
	"VibH2O.Fond.NoirEtRetour",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibH2OBackdropFadeTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false, TEXT("VibH2O_TestFond"));
	if (World == nullptr)
	{
		AddError(TEXT("Impossible de creer un monde de test."));
		return false;
	}
	ON_SCOPE_EXIT
	{
		World->DestroyWorld(false);
	};

	// An environment actor as a level would hold it: a fog, a lamp, a mesh.
	AActor* Water = World->SpawnActor<AActor>();
	UExponentialHeightFogComponent* Fog = NewObject<UExponentialHeightFogComponent>(Water);
	UPointLightComponent* Lamp = NewObject<UPointLightComponent>(Water);
	UStaticMeshComponent* Sky = NewObject<UStaticMeshComponent>(Water);
	Water->SetRootComponent(Fog);
	for (UActorComponent* Component : TArray<UActorComponent*>{ Fog, Lamp, Sky })
	{
		Water->AddInstanceComponent(Component);
		Component->RegisterComponent();
	}

	const FLinearColor Inscattering(0.02f, 0.14f, 0.21f);
	const FColor Albedo(222, 206, 185);
	Fog->SetFogInscatteringColor(Inscattering);
	Fog->SetDirectionalInscatteringColor(FLinearColor(0.3f, 0.3f, 0.4f));
	Fog->SetVolumetricFogAlbedo(Albedo);
	const float FogDensity = Fog->FogDensity;
	Lamp->SetIntensity(16000.0f);

	AVibH2OBackdrop* Backdrop = World->SpawnActor<AVibH2OBackdrop>();
	if (!TestNotNull(TEXT("L'acteur de fond se cree"), Backdrop))
	{
		return false;
	}
	Backdrop->EnvironmentActors.Add(Water);

	// Halfway: dimmer, but not black.
	Backdrop->Presence = 0.5f;
	Backdrop->ApplyPresence();
	TestTrue(TEXT("A mi-course le brouillard est attenue"), Fog->FogInscatteringLuminance.B < Inscattering.B);
	TestTrue(TEXT("A mi-course il n'est pas encore noir"), Fog->FogInscatteringLuminance.B > 0.0f);
	TestTrue(TEXT("Le ciel reste visible avant le noir"), Sky->GetVisibleFlag());

	// Black: the fog emits nothing at all, but still absorbs.
	Backdrop->Presence = 0.0f;
	Backdrop->ApplyPresence();
	TestTrue(TEXT("Brouillard : diffusion strictement noire"), Fog->FogInscatteringLuminance == FLinearColor(0.0f, 0.0f, 0.0f, 1.0f));
	TestTrue(TEXT("Brouillard : terme directionnel noir"), Fog->DirectionalInscatteringLuminance == FLinearColor(0.0f, 0.0f, 0.0f, 1.0f));
	TestEqual(TEXT("Brouillard volumetrique : albedo noir"), Fog->VolumetricFogAlbedo, FColor(0, 0, 0, Albedo.A));
	TestEqual(TEXT("La densite reste : les bulles lointaines sombrent dans le noir"), Fog->FogDensity, FogDensity);
	TestFalse(TEXT("Le ciel est cache au noir"), Sky->GetVisibleFlag());
	TestEqual(TEXT("Les lumieres sont laissees aux bulles"), Lamp->Intensity, 16000.0f);
	TestEqual(TEXT("Post-process au noir pleinement applique"), Backdrop->GetBlackPostProcessWeight(), 1.0f);

	// Back: the author's exact values.
	Backdrop->Presence = 1.0f;
	Backdrop->ApplyPresence();
	TestTrue(TEXT("Retour : diffusion d'origine"), Fog->FogInscatteringLuminance == Inscattering);
	TestEqual(TEXT("Retour : albedo d'origine"), Fog->VolumetricFogAlbedo, Albedo);
	TestTrue(TEXT("Retour : ciel visible"), Sky->GetVisibleFlag());
	TestEqual(TEXT("Retour : post-process au noir retire"), Backdrop->GetBlackPostProcessWeight(), 0.0f);

	// Lights only when asked.
	Backdrop->bFadeLights = true;
	Backdrop->Presence = 0.0f;
	Backdrop->ApplyPresence();
	TestEqual(TEXT("Lumieres eteintes sur demande"), Lamp->Intensity, 0.0f);

	// Deleting the switch must not leave the water black.
	Backdrop->Destroy();
	TestTrue(TEXT("Suppression : diffusion rendue"), Fog->FogInscatteringLuminance == Inscattering);
	TestEqual(TEXT("Suppression : lumiere rendue"), Lamp->Intensity, 16000.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2OBackdropOscTest,
	"VibH2O.Fond.AdresseOsc",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibH2OBackdropOscTest::RunTest(const FString& Parameters)
{
	// Same bare GameInstance as the integration tests: the subsystem's
	// ClassWithin forbids the transient package.
	TStrongObjectPtr<UGameInstance> GameInstance(NewObject<UGameInstance>(GEngine));
	TStrongObjectPtr<UVibH2OSubsystem> Holder(NewObject<UVibH2OSubsystem>(GameInstance.Get()));
	UVibH2OSubsystem* Subsystem = Holder.Get();

	TestEqual(TEXT("Le fond est present par defaut"), Subsystem->GetOscBackdropTarget(), 1.0f);

	FVibH2OOscMessage Remove(TEXT("/Fond/"));
	Remove.Args.Add(FVibH2OOscValue::MakeInt(0));
	Subsystem->InjectOscMessage(Remove);
	TestEqual(TEXT("/Fond 0 enleve le fond"), Subsystem->GetOscBackdropTarget(), 0.0f);

	FVibH2OOscMessage Restore(TEXT("/Fond"));
	Restore.Args.Add(FVibH2OOscValue::MakeFloat(1.0f));
	Restore.Args.Add(FVibH2OOscValue::MakeFloat(8.0f));
	Subsystem->InjectOscMessage(Restore);
	TestEqual(TEXT("/Fond 1 8 le remet"), Subsystem->GetOscBackdropTarget(), 1.0f);

	Subsystem->bIgnoreOscControl = true;
	Subsystem->InjectOscMessage(Remove);
	TestEqual(TEXT("Controle OSC coupe : le fond ne bouge pas"), Subsystem->GetOscBackdropTarget(), 1.0f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
