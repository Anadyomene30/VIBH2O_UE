#include "VibH2ORegie.h"

#include "Camera/CameraComponent.h"
#include "CameraRig_Rail.h"
#include "CineCameraComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SplineComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Osc/VibH2OOscTypes.h"
#include "Particles/ParticleSystemComponent.h"
#include "VibH2OBackdrop.h"
#include "VibH2OModule.h"
#include "VibH2OSettings.h"
#include "VibH2OStageActor.h"
#include "VibH2OSubsystem.h"
#include "VibH2OTulleRig.h"

namespace VibH2ORegieInternal
{
	// Labels are escaped rather than typed: the compiler must read them the
	// same way whatever the source encoding it assumes.
	static const FVibH2ORegieControl Controls[] = {
		{ TEXT("Exposure"), TEXT("Luminosité générale (EV)"), TEXT("Spectacle"), 4.0f, 16.0f, 0.1f },
		{ TEXT("bShowSeatNumbers"), TEXT("Numéros des spectateurs"), TEXT("Spectacle"), 0.0f, 1.0f, 1.0f },
		{ TEXT("FramingMargin"), TEXT("Cadrage : marge autour de la salle"), TEXT("Spectacle"), 0.0f, 0.8f, 0.01f },
		{ TEXT("DescentSeconds"), TEXT("Descente : durée (s)"), TEXT("Spectacle"), 1.0f, 300.0f, 1.0f },
		{ TEXT("ApproachPosition"), TEXT("Descente : s'arrête à (rail)"), TEXT("Spectacle"), 0.0f, 1.0f, 0.005f },
		{ TEXT("ArrivalSeconds"), TEXT("Arrivée : durée (s)"), TEXT("Spectacle"), 1.0f, 120.0f, 0.5f },
		{ TEXT("bParticlesFadeIn"), TEXT("Particules : sortent du brouillard"), TEXT("Spectacle"), 0.0f, 1.0f, 1.0f },
		{ TEXT("ParticlesAppearAt"), TEXT("Particules : apparaissent à (rail)"), TEXT("Spectacle"), 0.0f, 1.0f, 0.005f },

		{ TEXT("BubbleBrightness"), TEXT("Lumière des bulles"), TEXT("Ambiance"), 0.0f, 10.0f, 0.1f },
		{ TEXT("CalmColorLow"), TEXT("Calme : les plus calmes"), TEXT("Ambiance"), 0.0f, 1.0f, 0.0f },
		{ TEXT("CalmColorHigh"), TEXT("Calme : les plus agités"), TEXT("Ambiance"), 0.0f, 1.0f, 0.0f },
		{ TEXT("NormalColorLow"), TEXT("Normal : les plus calmes"), TEXT("Ambiance"), 0.0f, 1.0f, 0.0f },
		{ TEXT("NormalColorHigh"), TEXT("Normal : les plus agités"), TEXT("Ambiance"), 0.0f, 1.0f, 0.0f },
		{ TEXT("ExcitedColorLow"), TEXT("Excité : les plus calmes"), TEXT("Ambiance"), 0.0f, 1.0f, 0.0f },
		{ TEXT("ExcitedColorHigh"), TEXT("Excité : les plus agités"), TEXT("Ambiance"), 0.0f, 1.0f, 0.0f },
		{ TEXT("WaterColor"), TEXT("Couleur de l'eau"), TEXT("Ambiance"), 0.0f, 1.0f, 0.0f },
		{ TEXT("WaterLight"), TEXT("Lumière de l'eau"), TEXT("Ambiance"), 0.0f, 20.0f, 0.1f },
		{ TEXT("WaterDensity"), TEXT("Eau trouble"), TEXT("Ambiance"), 0.0f, 10.0f, 0.05f },
		{ TEXT("MoodTransitionSeconds"), TEXT("Bascule d'ambiance : durée (s)"), TEXT("Ambiance"), 0.0f, 30.0f, 0.5f },

		{ TEXT("BubbleSize"), TEXT("Taille des bulles"), TEXT("Bulles"), 0.1f, 4.0f, 0.05f },
		{ TEXT("FloatAmplitude"), TEXT("Flottement : ampleur (cm)"), TEXT("Bulles"), 0.0f, 200.0f, 1.0f },
		{ TEXT("FloatSpeed"), TEXT("Flottement : vitesse"), TEXT("Bulles"), 0.0f, 5.0f, 0.01f },
		{ TEXT("BeatPop"), TEXT("Battement : gonflement"), TEXT("Bulles"), 0.0f, 1.0f, 0.01f },
		{ TEXT("OrganicDeform"), TEXT("Déformation organique"), TEXT("Bulles"), 0.0f, 1.0f, 0.005f },

		{ TEXT("ScalePerExcitation"), TEXT("Excitation : grossissement"), TEXT("Réaction"), 0.0f, 3.0f, 0.01f },
		{ TEXT("DriftIntensity"), TEXT("Excitation : dérive (cm)"), TEXT("Réaction"), 0.0f, 500.0f, 5.0f },
		{ TEXT("IndividualContrast"), TEXT("Contraste entre spectateurs"), TEXT("Réaction"), 0.0f, 6.0f, 0.1f },
		{ TEXT("CalmThreshold"), TEXT("Seuil calme"), TEXT("Réaction"), 0.0f, 1.0f, 0.01f },
		{ TEXT("ExcitedThreshold"), TEXT("Seuil excité"), TEXT("Réaction"), 0.0f, 1.0f, 0.01f },
		{ TEXT("SyncHueTightening"), TEXT("Synchronie : couleurs qui se rejoignent"), TEXT("Réaction"), 0.0f, 1.0f, 0.01f },
		{ TEXT("SyncHalo"), TEXT("Synchronie : halo"), TEXT("Réaction"), 0.0f, 3.0f, 0.05f },
	};

	/** The water's numbers are tiny; the desk shows them a hundred times larger. */
	constexpr float WaterScale = 0.01f;

	/** How often the framing is worked out again during play. */
	constexpr double FramingInterval = 0.5;

	static FProperty* FindSetting(const FString& Id)
	{
		for (TFieldIterator<FProperty> It(FVibH2ORegieSettings::StaticStruct()); It; ++It)
		{
			if (It->GetName().Equals(Id, ESearchCase::IgnoreCase))
			{
				return *It;
			}
		}
		return nullptr;
	}

	static const FVibH2ORegieControl* FindControl(const FString& Id)
	{
		for (const FVibH2ORegieControl& Control : Controls)
		{
			if (Id.Equals(Control.Id, ESearchCase::IgnoreCase))
			{
				return &Control;
			}
		}
		return nullptr;
	}

	static bool IsColor(const FProperty* Property)
	{
		const FStructProperty* Struct = CastField<FStructProperty>(Property);
		return Struct != nullptr && Struct->Struct == TBaseStructure<FLinearColor>::Get();
	}

	static FString ToHex(const FLinearColor& Color)
	{
		const FColor C = Color.ToFColor(true);
		return FString::Printf(TEXT("#%02X%02X%02X"), C.R, C.G, C.B);
	}

	/** A colour split into its hue at full strength and that strength. */
	static void SplitColor(const FLinearColor& Color, FLinearColor& OutHue, float& OutStrength)
	{
		OutStrength = FMath::Max3(Color.R, Color.G, Color.B);
		OutHue = OutStrength > KINDA_SMALL_NUMBER
			? FLinearColor(Color.R / OutStrength, Color.G / OutStrength, Color.B / OutStrength, 1.0f)
			: FLinearColor::White;
	}

	/** Finds a Blueprint variable by its name, ignoring case: the editor shows "Duree Sequence0", the class may differ. */
	static FProperty* FindVariable(const UObject* Object, const TCHAR* Name)
	{
		for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
		{
			if (It->GetName().Equals(Name, ESearchCase::IgnoreCase))
			{
				return *It;
			}
		}
		return nullptr;
	}

	/** Writes a number into a float or double variable. Returns false if the value was already there. */
	static bool WriteNumber(UObject* Object, const TCHAR* Name, double Value)
	{
		FProperty* Property = FindVariable(Object, Name);
		if (FDoubleProperty* Double = CastField<FDoubleProperty>(Property))
		{
			double* Ptr = Double->ContainerPtrToValuePtr<double>(Object);
			if (FMath::IsNearlyEqual(*Ptr, Value, 1e-5))
			{
				return false;
			}
			*Ptr = Value;
			return true;
		}
		if (FFloatProperty* Float = CastField<FFloatProperty>(Property))
		{
			float* Ptr = Float->ContainerPtrToValuePtr<float>(Object);
			if (FMath::IsNearlyEqual(*Ptr, static_cast<float>(Value), 1e-5f))
			{
				return false;
			}
			*Ptr = static_cast<float>(Value);
			return true;
		}
		return false;
	}

	/** Writes X and/or Y of a FVector2D variable. A negative value leaves that half alone. */
	static bool WriteRange(UObject* Object, const TCHAR* Name, double X, double Y)
	{
		FStructProperty* Struct = CastField<FStructProperty>(FindVariable(Object, Name));
		if (Struct == nullptr || Struct->Struct != TBaseStructure<FVector2D>::Get())
		{
			return false;
		}
		FVector2D* Range = Struct->ContainerPtrToValuePtr<FVector2D>(Object);
		const FVector2D Before = *Range;
		if (X >= 0.0)
		{
			Range->X = X;
		}
		if (Y >= 0.0)
		{
			Range->Y = Y;
		}
		return !Range->Equals(Before, 1e-5);
	}
}

AVibH2ORegie::AVibH2ORegie()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	// After the rail and the controller have moved this frame: the desk
	// notices whoever moved the rail before it writes its own position.
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

// --------------------------------------------------------------------- Actor

void AVibH2ORegie::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ResolveConnections();
	if (!bCaptured && Stage != nullptr)
	{
		// Placing the desk must not change the look: it starts from the scene.
		CaptureFromScene();
		bCaptured = true;
	}
	ApplyChanged(false);
}

void AVibH2ORegie::BeginPlay()
{
	Super::BeginPlay();
	ResolveConnections();
	RefreshPresetNames();
	ApplyChanged(true);
	GatherParticles();
	ParticlesState = -1;

	if (UVibH2OSubsystem* Subsystem = UVibH2OSubsystem::Get(this))
	{
		RegieMessageHandle = Subsystem->OnRegieMessage.AddUObject(this, &AVibH2ORegie::HandleRegieMessage);
	}
	RefreshFraming();
}

void AVibH2ORegie::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UVibH2OSubsystem* Subsystem = UVibH2OSubsystem::Get(this))
	{
		Subsystem->OnRegieMessage.Remove(RegieMessageHandle);
	}
	RegieMessageHandle.Reset();
	Super::EndPlay(EndPlayReason);
}

void AVibH2ORegie::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Sequencer and the Details panel write the struct directly: the desk
	// notices by comparing, and pushes only what changed.
	ApplyChanged(false);

	const UWorld* World = GetWorld();
	if (World == nullptr || !World->IsGameWorld())
	{
		return;
	}

	const double Now = FPlatformTime::Seconds();
	if (Now - LastFramingTime > VibH2ORegieInternal::FramingInterval)
	{
		LastFramingTime = Now;
		RefreshFraming();
	}
	TickCamera(DeltaSeconds);
	TickParticles();
}

#if WITH_EDITOR
void AVibH2ORegie::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	ResolveConnections();
	ApplyChanged(false);
}
#endif

// ---------------------------------------------------------------- Connections

void AVibH2ORegie::ResolveConnections()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	if (Stage == nullptr)
	{
		for (TActorIterator<AVibH2OStageActor> It(World); It; ++It)
		{
			Stage = *It;
			break;
		}
	}
	if (ExposureVolume == nullptr)
	{
		APostProcessVolume* FirstBounded = nullptr;
		for (TActorIterator<APostProcessVolume> It(World); It; ++It)
		{
			if (It->GetActorNameOrLabel().Contains(TEXT("Exposition")))
			{
				ExposureVolume = *It;
				break;
			}
			if (FirstBounded == nullptr && !It->bUnbound)
			{
				FirstBounded = *It;
			}
		}
		if (ExposureVolume == nullptr)
		{
			ExposureVolume = FirstBounded;
		}
	}
	if (Water == nullptr)
	{
		for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
		{
			if (It->GetComponent() != nullptr && It->GetComponent()->IsVisible())
			{
				Water = *It;
				break;
			}
		}
	}
	if (Rail == nullptr)
	{
		for (TActorIterator<ACameraRig_Rail> It(World); It; ++It)
		{
			Rail = *It;
			break;
		}
	}
	if (CameraController == nullptr && !CameraControllerClassName.IsEmpty())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetClass()->GetName().Contains(CameraControllerClassName))
			{
				CameraController = *It;
				break;
			}
		}
	}
}

// ------------------------------------------------------------------- Apply

void AVibH2ORegie::ApplyAll()
{
	ResolveConnections();
	ApplyChanged(true);
}

void AVibH2ORegie::ApplyChanged(bool bForce)
{
	if (!bForce && bHasApplied && FVibH2ORegieSettings::StaticStruct()->CompareScriptStruct(&Reglages, &Applied, PPF_None))
	{
		if (bWaterPending)
		{
			ApplyToWater();
		}
		return;
	}

	if (bForce || !bHasApplied || Reglages.OutputFormat != Applied.OutputFormat)
	{
		ApplyToOutput();
	}
	ApplyToStage();
	ApplyToExposure();
	ApplyToWater();
	ApplyToCameraController();

	Applied = Reglages;
	bHasApplied = true;
}

void AVibH2ORegie::ApplyToStage()
{
	if (Stage == nullptr)
	{
		return;
	}
	const FVibH2ORegieSettings& R = Reglages;
	Stage->bShowSeatNumbers = R.bShowSeatNumbers;
	Stage->BubbleBrightness = R.BubbleBrightness;
	Stage->BaseScale = R.BubbleSize;
	Stage->FloatAmplitude = R.FloatAmplitude;
	Stage->FloatSpeed = R.FloatSpeed;
	Stage->BeatPopAmount = R.BeatPop;
	Stage->OrganicDeform = R.OrganicDeform;
	Stage->ScalePerExcitation = R.ScalePerExcitation;
	Stage->DriftIntensity = R.DriftIntensity;
	Stage->CalmThreshold = R.CalmThreshold;
	Stage->ExcitedThreshold = FMath::Max(R.ExcitedThreshold, R.CalmThreshold);
	Stage->MoodTransitionSeconds = R.MoodTransitionSeconds;

	FVibH2OMoodPalette& Palette = Stage->MoodPalette;
	Palette.Calm.ColorLow = R.CalmColorLow;
	Palette.Calm.ColorHigh = R.CalmColorHigh;
	Palette.Normal.ColorLow = R.NormalColorLow;
	Palette.Normal.ColorHigh = R.NormalColorHigh;
	Palette.Excited.ColorLow = R.ExcitedColorLow;
	Palette.Excited.ColorHigh = R.ExcitedColorHigh;
	Palette.IndividualContrast = R.IndividualContrast;
	Palette.SyncHueTightening = R.SyncHueTightening;
	Palette.SyncHalo = R.SyncHalo;
}

void AVibH2ORegie::ApplyToExposure()
{
	if (ExposureVolume == nullptr)
	{
		return;
	}
	ExposureVolume->Settings.bOverride_AutoExposureBias = true;
	ExposureVolume->Settings.AutoExposureBias = Reglages.Exposure;
}

void AVibH2ORegie::ApplyToWater()
{
	UExponentialHeightFogComponent* Fog = Water != nullptr ? Water->GetComponent() : nullptr;
	if (Fog == nullptr)
	{
		bWaterPending = false;
		return;
	}

	// The backdrop dims the fog's colours when it goes to black and puts back
	// what it found when it returns. A colour set meanwhile would be undone
	// then, so it waits for the water to be back.
	FLinearColor Current = Fog->FogInscatteringLuminance;
	FLinearColor Wanted = Reglages.WaterColor * (Reglages.WaterLight * VibH2ORegieInternal::WaterScale);
	Wanted.A = 1.0f;
	bool bBackdropAway = false;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AVibH2OBackdrop> It(World); It; ++It)
		{
			bBackdropAway = It->Presence < 1.0f;
			break;
		}
	}
	if (bBackdropAway)
	{
		bWaterPending = !Current.Equals(Wanted, 1e-6f);
	}
	else
	{
		bWaterPending = false;
		Fog->SetFogInscatteringColor(Wanted);
		FLinearColor Directional = Wanted * WaterDirectionalRatio;
		Directional.A = 1.0f;
		Fog->SetDirectionalInscatteringColor(Directional);
	}
	Fog->SetFogDensity(Reglages.WaterDensity * VibH2ORegieInternal::WaterScale);
}

void AVibH2ORegie::ApplyToCameraController()
{
	if (!bDriveCameraController || CameraController == nullptr)
	{
		return;
	}
	using namespace VibH2ORegieInternal;
	UObject* C = CameraController;
	bool bChanged = false;
	bChanged |= WriteRange(C, TEXT("IntervalleParcours0"), -1.0, Reglages.ApproachPosition);
	bChanged |= WriteRange(C, TEXT("IntervalleParcours1"), Reglages.ApproachPosition, FramedRailPosition >= 0.0f ? FramedRailPosition : -1.0);
	if (FramedRailPosition >= 0.0f)
	{
		bChanged |= WriteRange(C, TEXT("IntervalleParcours2"), -1.0, FramedRailPosition);
	}
	bChanged |= WriteNumber(C, TEXT("Duree Sequence0"), Reglages.DescentSeconds);
	bChanged |= WriteNumber(C, TEXT("Duree Sequence1"), Reglages.ArrivalSeconds);
	if (bChanged && GetWorld() != nullptr && !GetWorld()->IsGameWorld())
	{
		CameraController->MarkPackageDirty();
	}
}

FIntPoint AVibH2ORegie::GetOutputSize(EVibH2OOutputFormat Format)
{
	switch (Format)
	{
	case EVibH2OOutputFormat::TulleFace: return FIntPoint(1920, 1440);
	case EVibH2OOutputFormat::BacFace: return FIntPoint(1920, 410);
	case EVibH2OOutputFormat::TulleCote: return FIntPoint(960, 1920);
	case EVibH2OOutputFormat::Ecran169: return FIntPoint(1920, 1080);
	default: return FIntPoint::ZeroValue;
	}
}

UCameraComponent* AVibH2ORegie::FindRailCamera() const
{
	if (Rail == nullptr)
	{
		return nullptr;
	}
	TArray<AActor*> Attached;
	Rail->GetAttachedActors(Attached, true, true);
	for (const AActor* Actor : Attached)
	{
		if (UCameraComponent* Found = Actor->FindComponentByClass<UCameraComponent>())
		{
			return Found;
		}
	}
	return nullptr;
}

void AVibH2ORegie::ApplyToOutput()
{
	const FIntPoint Size = GetOutputSize(Reglages.OutputFormat);
	if (Size.X <= 0 || Size.Y <= 0)
	{
		return;
	}
	const float Aspect = static_cast<float>(Size.X) / Size.Y;

	// The camera: same sensor width, so the same horizontal field of view; the
	// height follows the surface. The picture is then exactly the tulle's.
	if (UCameraComponent* Camera = FindRailCamera())
	{
		if (UCineCameraComponent* Cine = Cast<UCineCameraComponent>(Camera))
		{
			FCameraFilmbackSettings Filmback = Cine->Filmback;
			Filmback.SensorHeight = Filmback.SensorWidth / Aspect;
			Cine->SetFilmback(Filmback);
		}
		Camera->SetAspectRatio(Aspect);
		Camera->SetConstraintAspectRatio(true);
	}

	// Spout: the active camera's frame at the surface's resolution - the
	// window's size no longer decides what Resolume receives.
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		// The tulles in depth send their own pictures, at their own sizes.
		if (It->IsA<AVibH2OTulleRig>())
		{
			continue;
		}
		TInlineComponentArray<UActorComponent*> Components(*It);
		for (UActorComponent* Component : Components)
		{
			if (!Component->GetClass()->GetName().Contains(TEXT("SpoutSender")))
			{
				continue;
			}
			if (FStructProperty* Resolution = CastField<FStructProperty>(Component->GetClass()->FindPropertyByName(TEXT("OutputResolution"))))
			{
				if (Resolution->Struct == TBaseStructure<FIntPoint>::Get())
				{
					*Resolution->ContainerPtrToValuePtr<FIntPoint>(Component) = Size;
				}
			}
			if (FEnumProperty* Mode = CastField<FEnumProperty>(Component->GetClass()->FindPropertyByName(TEXT("SourceMode"))))
			{
				const int64 ActiveCamera = Mode->GetEnum()->GetValueByNameString(TEXT("ActiveCamera"));
				if (ActiveCamera != INDEX_NONE)
				{
					Mode->GetUnderlyingProperty()->SetIntPropertyValue(Mode->ContainerPtrToValuePtr<void>(Component), ActiveCamera);
				}
			}
			if (!World->IsGameWorld())
			{
				Component->MarkPackageDirty();
			}
		}
	}
}

// ----------------------------------------------------------------- Capture

void AVibH2ORegie::CaptureFromScene()
{
	ResolveConnections();
	FVibH2ORegieSettings& R = Reglages;

	if (Stage != nullptr)
	{
		R.bShowSeatNumbers = Stage->bShowSeatNumbers;
		R.BubbleBrightness = Stage->BubbleBrightness;
		R.BubbleSize = Stage->BaseScale;
		R.FloatAmplitude = Stage->FloatAmplitude;
		R.FloatSpeed = Stage->FloatSpeed;
		R.BeatPop = Stage->BeatPopAmount;
		R.OrganicDeform = Stage->OrganicDeform;
		R.ScalePerExcitation = Stage->ScalePerExcitation;
		R.DriftIntensity = Stage->DriftIntensity;
		R.CalmThreshold = Stage->CalmThreshold;
		R.ExcitedThreshold = Stage->ExcitedThreshold;
		R.MoodTransitionSeconds = Stage->MoodTransitionSeconds;
		const FVibH2OMoodPalette& Palette = Stage->MoodPalette;
		R.CalmColorLow = Palette.Calm.ColorLow;
		R.CalmColorHigh = Palette.Calm.ColorHigh;
		R.NormalColorLow = Palette.Normal.ColorLow;
		R.NormalColorHigh = Palette.Normal.ColorHigh;
		R.ExcitedColorLow = Palette.Excited.ColorLow;
		R.ExcitedColorHigh = Palette.Excited.ColorHigh;
		R.IndividualContrast = Palette.IndividualContrast;
		R.SyncHueTightening = Palette.SyncHueTightening;
		R.SyncHalo = Palette.SyncHalo;
	}
	if (ExposureVolume != nullptr && ExposureVolume->Settings.bOverride_AutoExposureBias)
	{
		R.Exposure = ExposureVolume->Settings.AutoExposureBias;
	}
	if (const UExponentialHeightFogComponent* Fog = Water != nullptr ? Water->GetComponent() : nullptr)
	{
		FLinearColor Hue;
		float Strength = 0.0f;
		VibH2ORegieInternal::SplitColor(Fog->FogInscatteringLuminance, Hue, Strength);
		R.WaterColor = Hue;
		R.WaterLight = Strength / VibH2ORegieInternal::WaterScale;
		R.WaterDensity = Fog->FogDensity / VibH2ORegieInternal::WaterScale;
		const float Directional = FMath::Max3(Fog->DirectionalInscatteringLuminance.R, Fog->DirectionalInscatteringLuminance.G, Fog->DirectionalInscatteringLuminance.B);
		WaterDirectionalRatio = Strength > KINDA_SMALL_NUMBER ? Directional / Strength : WaterDirectionalRatio;
	}
	if (CameraController != nullptr)
	{
		using namespace VibH2ORegieInternal;
		if (const FStructProperty* Range = CastField<FStructProperty>(FindVariable(CameraController, TEXT("IntervalleParcours0"))))
		{
			if (Range->Struct == TBaseStructure<FVector2D>::Get())
			{
				R.ApproachPosition = static_cast<float>(Range->ContainerPtrToValuePtr<FVector2D>(CameraController)->Y);
			}
		}
	}

	// What was read is what is applied: nothing moves - except the output
	// format, which the scene does not hold and the next apply sets.
	Applied = R;
	Applied.OutputFormat = R.OutputFormat == EVibH2OOutputFormat::Libre ? EVibH2OOutputFormat::TulleFace : EVibH2OOutputFormat::Libre;
	bHasApplied = true;
}

// ----------------------------------------------------------------- Framing

bool AVibH2ORegie::IsFramed(TConstArrayView<FVector> Corners, const FVector& CameraLocation, const FVector& Forward, const FVector& Right,
	const FVector& Up, float HorizontalFOV, float AspectRatio, float Margin)
{
	const float TanH = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(HorizontalFOV, 1.0f, 170.0f) * 0.5f));
	const float TanV = TanH / FMath::Max(AspectRatio, 0.1f);
	const float Grow = 1.0f + FMath::Max(Margin, 0.0f);
	for (const FVector& Corner : Corners)
	{
		const FVector ToCorner = Corner - CameraLocation;
		const float Depth = FVector::DotProduct(ToCorner, Forward);
		if (Depth <= KINDA_SMALL_NUMBER)
		{
			return false;
		}
		if (FMath::Abs(FVector::DotProduct(ToCorner, Right)) * Grow > Depth * TanH
			|| FMath::Abs(FVector::DotProduct(ToCorner, Up)) * Grow > Depth * TanV)
		{
			return false;
		}
	}
	return true;
}

bool AVibH2ORegie::ComputeFramedRailPosition(float& OutPosition) const
{
	if (Stage == nullptr || Rail == nullptr || Stage->GetBubbles().Num() == 0)
	{
		return false;
	}
	// Framed for the seating plan, not the school: in tableau 2 the bubbles
	// leave their seats, and the camera's end point must not chase them.
	if (Stage->BlendAlpha > 0.01f)
	{
		return false;
	}

	USplineComponent* Spline = Rail->GetRailSplineComponent();
	if (Spline == nullptr)
	{
		return false;
	}

	// The camera riding the rail: the first camera attached under it.
	const UCameraComponent* Camera = FindRailCamera();
	if (Camera == nullptr)
	{
		return false;
	}

	const FBox Room = Stage->GetRoomBounds();
	FVector Corners[8];
	Room.GetVertices(Corners);

	const float Length = Spline->GetSplineLength();
	const float Current = Rail->CurrentPositionOnRail;
	const FVector Offset = Camera->GetComponentLocation() - Spline->GetLocationAtDistanceAlongSpline(Current * Length, ESplineCoordinateSpace::World);
	const FQuat View = Camera->GetComponentQuat();
	const FVector Forward = View.GetForwardVector();
	const FVector Right = View.GetRightVector();
	const FVector Up = View.GetUpVector();
	// A cine camera's own ratio is only refreshed on its tick: read the sensor.
	float AspectRatio = Camera->AspectRatio > 0.0f ? Camera->AspectRatio : 16.0f / 9.0f;
	if (const UCineCameraComponent* Cine = Cast<UCineCameraComponent>(Camera))
	{
		if (Cine->Filmback.SensorHeight > 0.0f)
		{
			AspectRatio = Cine->Filmback.SensorWidth / Cine->Filmback.SensorHeight;
		}
	}

	// Down the rail, the room grows in the picture: the end point is the last
	// position that still holds it all. Sampled, then refined between the
	// two samples that straddle the edge.
	constexpr int32 Samples = 400;
	int32 LastFramed = INDEX_NONE;
	for (int32 Index = 0; Index <= Samples; ++Index)
	{
		const float P = static_cast<float>(Index) / Samples;
		const FVector Location = Spline->GetLocationAtDistanceAlongSpline(P * Length, ESplineCoordinateSpace::World) + Offset;
		if (IsFramed(Corners, Location, Forward, Right, Up, Camera->FieldOfView, AspectRatio, Reglages.FramingMargin))
		{
			LastFramed = Index;
		}
		else if (LastFramed != INDEX_NONE)
		{
			break;
		}
	}
	if (LastFramed == INDEX_NONE)
	{
		return false;
	}

	float Low = static_cast<float>(LastFramed) / Samples;
	float High = FMath::Min(1.0f, static_cast<float>(LastFramed + 1) / Samples);
	for (int32 Step = 0; Step < 12; ++Step)
	{
		const float Mid = 0.5f * (Low + High);
		const FVector Location = Spline->GetLocationAtDistanceAlongSpline(Mid * Length, ESplineCoordinateSpace::World) + Offset;
		if (IsFramed(Corners, Location, Forward, Right, Up, Camera->FieldOfView, AspectRatio, Reglages.FramingMargin))
		{
			Low = Mid;
		}
		else
		{
			High = Mid;
		}
	}
	OutPosition = Low;
	return true;
}

void AVibH2ORegie::RefreshFraming()
{
	ResolveConnections();
	float Position = 0.0f;
	if (ComputeFramedRailPosition(Position) && !FMath::IsNearlyEqual(Position, FramedRailPosition, 1e-4f))
	{
		FramedRailPosition = Position;
		ApplyToCameraController();
	}
}

// ------------------------------------------------------------------ Camera

float AVibH2ORegie::GetRailPosition() const
{
	return Rail != nullptr ? Rail->CurrentPositionOnRail : 0.0f;
}

void AVibH2ORegie::SetRailPosition(float Position)
{
	if (Rail == nullptr)
	{
		return;
	}
	Rail->CurrentPositionOnRail = FMath::Clamp(Position, 0.0f, 1.0f);
	LastWrittenRail = Rail->CurrentPositionOnRail;
}

void AVibH2ORegie::StartDescent()
{
	ResolveConnections();
	MoveFrom = 0.0f;
	MoveTo = Reglages.ApproachPosition;
	MoveDuration = Reglages.DescentSeconds;
	MoveElapsed = 0.0f;
	CameraMove = EVibH2ORegieCameraMove::Descent;
	SetRailPosition(MoveFrom);
}

void AVibH2ORegie::StartArrival()
{
	ResolveConnections();
	RefreshFraming();
	MoveFrom = GetRailPosition();
	MoveTo = FramedRailPosition >= 0.0f ? FramedRailPosition : MoveFrom;
	MoveDuration = Reglages.ArrivalSeconds;
	MoveElapsed = 0.0f;
	CameraMove = EVibH2ORegieCameraMove::Arrival;
	LastWrittenRail = MoveFrom;
}

void AVibH2ORegie::GlideCameraTo(float RailPosition, float Seconds)
{
	ResolveConnections();
	MoveFrom = GetRailPosition();
	MoveTo = FMath::Clamp(RailPosition, 0.0f, 1.0f);
	MoveDuration = FMath::Max(Seconds, 0.0f);
	MoveElapsed = 0.0f;
	CameraMove = EVibH2ORegieCameraMove::Glide;
	LastWrittenRail = MoveFrom;
	if (MoveDuration <= 0.0f)
	{
		SetRailPosition(MoveTo);
		CameraMove = EVibH2ORegieCameraMove::None;
	}
}

void AVibH2ORegie::StopCamera()
{
	CameraMove = EVibH2ORegieCameraMove::None;
}

void AVibH2ORegie::TickCamera(float DeltaSeconds)
{
	if (CameraMove == EVibH2ORegieCameraMove::None || Rail == nullptr)
	{
		return;
	}
	// Someone else moved the rail - a key of the camera controller, a Level
	// Sequence: the desk steps aside rather than fight it frame by frame.
	if (LastWrittenRail >= 0.0f && !FMath::IsNearlyEqual(Rail->CurrentPositionOnRail, LastWrittenRail, 1e-4f))
	{
		CameraMove = EVibH2ORegieCameraMove::None;
		return;
	}
	MoveElapsed += DeltaSeconds;
	const float Alpha = MoveDuration > 0.0f ? FMath::Clamp(MoveElapsed / MoveDuration, 0.0f, 1.0f) : 1.0f;
	SetRailPosition(FMath::Lerp(MoveFrom, MoveTo, FMath::SmoothStep(0.0f, 1.0f, Alpha)));
	if (Alpha >= 1.0f)
	{
		CameraMove = EVibH2ORegieCameraMove::None;
	}
}

// --------------------------------------------------------------- Particles

void AVibH2ORegie::GatherParticles()
{
	Particles.Reset();
	UWorld* World = GetWorld();
	if (World == nullptr || ParticleNameFilter.IsEmpty())
	{
		return;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const AActor* Actor = *It;
		if (Actor->GetClass()->GetName().StartsWith(TEXT("VibH2O")))
		{
			continue;
		}
		const bool bActorMatches = Actor->GetActorNameOrLabel().Contains(ParticleNameFilter)
			|| Actor->GetClass()->GetName().Contains(ParticleNameFilter);
		TInlineComponentArray<UFXSystemComponent*> Components(Actor);
		for (UFXSystemComponent* Component : Components)
		{
			if (bActorMatches || Component->GetName().Contains(ParticleNameFilter))
			{
				Particles.Add(Component);
			}
		}
	}
	UE_LOG(LogVibH2O, Log, TEXT("VibH2O Regie : %d systemes de particules suivis (filtre \"%s\")."), Particles.Num(), *ParticleNameFilter);
}

void AVibH2ORegie::TickParticles()
{
	if (Particles.Num() == 0)
	{
		return;
	}
	// Absent above the threshold, born below it: their own spawn rate then
	// fills them in over a few seconds - they come out of the water rather
	// than switch on.
	const bool bWanted = !Reglages.bParticlesFadeIn || GetRailPosition() >= Reglages.ParticlesAppearAt;
	const int32 State = bWanted ? 1 : 0;
	if (State == ParticlesState)
	{
		return;
	}
	const bool ParticlesWereOff = ParticlesState == 0;
	ParticlesState = State;
	for (const TWeakObjectPtr<UFXSystemComponent>& Weak : Particles)
	{
		if (UFXSystemComponent* Component = Weak.Get())
		{
			if (bWanted)
			{
				// Reset only when the desk itself had put them out: at the
				// start of Play they are left as they are.
				if (!Component->IsActive() || ParticlesWereOff)
				{
					Component->Activate(true);
				}
			}
			else
			{
				Component->DeactivateImmediate();
			}
		}
	}
}

// ----------------------------------------------------------------- Presets

FString AVibH2ORegie::GetPresetFolder()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("VibH2O"), TEXT("Regie"));
}

void AVibH2ORegie::RefreshPresetNames()
{
	PresetNames = ListPresets();
}

TArray<FString> AVibH2ORegie::ListPresets() const
{
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(GetPresetFolder(), TEXT("*.json")), true, false);
	TArray<FString> Names;
	for (const FString& File : Files)
	{
		Names.Add(FPaths::GetBaseFilename(File));
	}
	Names.Sort();
	return Names;
}

bool AVibH2ORegie::SavePreset(const FString& Name)
{
	const FString Clean = FPaths::MakeValidFileName(Name.TrimStartAndEnd());
	if (Clean.IsEmpty())
	{
		return false;
	}
	FString Json;
	if (!FJsonObjectConverter::UStructToJsonObjectString(Reglages, Json))
	{
		return false;
	}
	const bool bSaved = FFileHelper::SaveStringToFile(Json, *FPaths::Combine(GetPresetFolder(), Clean + TEXT(".json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	UE_LOG(LogVibH2O, Log, TEXT("VibH2O Regie : preset \"%s\" %s."), *Clean, bSaved ? TEXT("enregistre") : TEXT("NON enregistre"));
	RefreshPresetNames();
	return bSaved;
}

bool AVibH2ORegie::LoadPreset(const FString& Name)
{
	const FString Clean = FPaths::MakeValidFileName(Name.TrimStartAndEnd());
	FString Json;
	if (Clean.IsEmpty() || !FFileHelper::LoadFileToString(Json, *FPaths::Combine(GetPresetFolder(), Clean + TEXT(".json"))))
	{
		UE_LOG(LogVibH2O, Warning, TEXT("VibH2O Regie : preset \"%s\" introuvable."), *Name);
		return false;
	}
	// Starting from the current values: a preset saved before a setting
	// existed leaves that setting as it is.
	FVibH2ORegieSettings Loaded = Reglages;
	if (!FJsonObjectConverter::JsonObjectStringToUStruct(Json, &Loaded, 0, 0))
	{
		UE_LOG(LogVibH2O, Warning, TEXT("VibH2O Regie : preset \"%s\" illisible."), *Clean);
		return false;
	}
	Reglages = Loaded;
	ApplyChanged(false);
	return true;
}

bool AVibH2ORegie::DeletePreset(const FString& Name)
{
	const FString Clean = FPaths::MakeValidFileName(Name.TrimStartAndEnd());
	const bool bDeleted = !Clean.IsEmpty() && IFileManager::Get().Delete(*FPaths::Combine(GetPresetFolder(), Clean + TEXT(".json")));
	RefreshPresetNames();
	return bDeleted;
}

// ------------------------------------------------------------------ Tablet

TConstArrayView<FVibH2ORegieControl> AVibH2ORegie::GetControls()
{
	return VibH2ORegieInternal::Controls;
}

bool AVibH2ORegie::SetValueFromText(const FString& Id, const FString& Value)
{
	using namespace VibH2ORegieInternal;
	const FVibH2ORegieControl* Control = FindControl(Id);
	FProperty* Property = Control != nullptr ? FindSetting(Control->Id) : nullptr;
	if (Property == nullptr)
	{
		UE_LOG(LogVibH2O, Warning, TEXT("VibH2O Regie : reglage inconnu \"%s\"."), *Id);
		return false;
	}

	void* Ptr = Property->ContainerPtrToValuePtr<void>(&Reglages);
	if (IsColor(Property))
	{
		FString Hex = Value.TrimStartAndEnd();
		Hex.RemoveFromStart(TEXT("#"));
		if (Hex.Len() != 6)
		{
			return false;
		}
		*static_cast<FLinearColor*>(Ptr) = FLinearColor(FColor::FromHex(Hex));
	}
	else if (FBoolProperty* Bool = CastField<FBoolProperty>(Property))
	{
		const FString Lower = Value.TrimStartAndEnd().ToLower();
		Bool->SetPropertyValue(Ptr, Lower == TEXT("true") || Lower == TEXT("on") || FCString::Atof(*Lower) > 0.5f);
	}
	else if (FFloatProperty* Float = CastField<FFloatProperty>(Property))
	{
		if (!Value.TrimStartAndEnd().IsNumeric())
		{
			return false;
		}
		Float->SetPropertyValue(Ptr, FMath::Clamp(FCString::Atof(*Value), Control->Min, Control->Max));
	}
	else
	{
		return false;
	}
	ApplyChanged(false);
	return true;
}

void AVibH2ORegie::HandleRegieMessage(const FVibH2OOscMessage& Message)
{
	const UVibH2OSettings* Settings = UVibH2OSettings::Get();
	TArray<FString> Segments;
	Message.GetSegmentsAfterPrefix(Settings->RegiePrefix, Segments);

	// Every argument as text, so "/Regie/Preset/Save Bleu final" keeps its space.
	TArray<FString> Words;
	for (int32 Index = 0; Index < Message.Args.Num(); ++Index)
	{
		const FVibH2OOscValue& Arg = Message.Args[Index];
		// An integer stays an integer: "Final 2" must not come back as "Final 2.0".
		Words.Add(Arg.Type == EVibH2OOscType::String ? Arg.StringValue
			: Arg.Type == EVibH2OOscType::Int32 ? FString::FromInt(Message.GetIntArg(Index, 0))
			: FString::SanitizeFloat(Message.GetFloatArg(Index, 0.0f)));
	}
	const FString AllWords = FString::Join(Words, TEXT(" "));

	if (Segments.Num() == 0)
	{
		return;
	}
	const FString Head = Segments[0].ToLower();

	if (Head == TEXT("camera"))
	{
		const FString Command = (Segments.Num() > 1 ? Segments[1] : (Words.Num() > 0 ? Words[0] : FString())).ToLower();
		if (Command.StartsWith(TEXT("desc")))
		{
			StartDescent();
		}
		else if (Command.StartsWith(TEXT("arr")))
		{
			StartArrival();
		}
		else if (Command.StartsWith(TEXT("stop")))
		{
			StopCamera();
		}
		else if (Command.StartsWith(TEXT("dep")) || Command.StartsWith(TEXT("haut")))
		{
			GlideCameraTo(0.0f, 0.0f);
		}
		else if (Command.StartsWith(TEXT("ens")))
		{
			RefreshFraming();
			GlideCameraTo(FramedRailPosition >= 0.0f ? FramedRailPosition : GetRailPosition(), 0.0f);
		}
		return;
	}

	if (Head == TEXT("preset"))
	{
		const FString Action = Segments.Num() > 1 ? Segments[1].ToLower() : FString();
		if (Action == TEXT("save"))
		{
			SavePreset(AllWords);
		}
		else if (Action == TEXT("load"))
		{
			LoadPreset(AllWords);
		}
		else if (Action == TEXT("delete"))
		{
			DeletePreset(AllWords);
		}
		return;
	}

	if (Head == TEXT("set") && Words.Num() >= 2)
	{
		SetValueFromText(Words[0], Words[1]);
		return;
	}

	// "/Regie/Exposure 10.5": the setting as the address.
	if (Words.Num() >= 1)
	{
		SetValueFromText(Segments[0], Words[0]);
	}
}

void AVibH2ORegie::WriteRemoteState(const TSharedRef<FJsonObject>& Root) const
{
	using namespace VibH2ORegieInternal;
	const TSharedRef<FJsonObject> Desk = MakeShared<FJsonObject>();

	const TSharedRef<FJsonObject> Values = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> ControlList;
	for (const FVibH2ORegieControl& Control : Controls)
	{
		const FProperty* Property = FindSetting(Control.Id);
		if (Property == nullptr)
		{
			continue;
		}
		const void* Ptr = Property->ContainerPtrToValuePtr<void>(&Reglages);
		FString Kind;
		if (IsColor(Property))
		{
			Kind = TEXT("color");
			Values->SetStringField(Control.Id, ToHex(*static_cast<const FLinearColor*>(Ptr)));
		}
		else if (const FBoolProperty* Bool = CastField<FBoolProperty>(Property))
		{
			Kind = TEXT("bool");
			Values->SetBoolField(Control.Id, Bool->GetPropertyValue(Ptr));
		}
		else if (const FFloatProperty* Float = CastField<FFloatProperty>(Property))
		{
			Kind = TEXT("float");
			Values->SetNumberField(Control.Id, Float->GetPropertyValue(Ptr));
		}
		else
		{
			continue;
		}

		const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("id"), Control.Id);
		Entry->SetStringField(TEXT("label"), Control.Label);
		Entry->SetStringField(TEXT("tab"), Control.Tab);
		Entry->SetStringField(TEXT("kind"), Kind);
		Entry->SetNumberField(TEXT("min"), Control.Min);
		Entry->SetNumberField(TEXT("max"), Control.Max);
		Entry->SetNumberField(TEXT("step"), Control.Step);
		ControlList.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Desk->SetObjectField(TEXT("values"), Values);
	Desk->SetArrayField(TEXT("controls"), ControlList);

	TArray<TSharedPtr<FJsonValue>> Presets;
	for (const FString& Name : PresetNames)
	{
		Presets.Add(MakeShared<FJsonValueString>(Name));
	}
	Desk->SetArrayField(TEXT("presets"), Presets);

	Desk->SetNumberField(TEXT("rail"), GetRailPosition());
	Desk->SetNumberField(TEXT("framed"), FramedRailPosition);
	Desk->SetStringField(TEXT("move"),
		CameraMove == EVibH2ORegieCameraMove::Descent ? TEXT("descente")
		: CameraMove == EVibH2ORegieCameraMove::Arrival ? TEXT("arrivee")
		: CameraMove == EVibH2ORegieCameraMove::Glide ? TEXT("deplacement")
		: TEXT(""));
	Root->SetObjectField(TEXT("regie"), Desk);
}
