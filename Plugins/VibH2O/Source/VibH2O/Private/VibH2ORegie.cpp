#include "VibH2ORegie.h"

#include "Camera/CameraComponent.h"
#include "CameraRig_Rail.h"
#include "CineCameraComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InputComponent.h"
#include "Components/SplineComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "InputCoreTypes.h"
#include "Interfaces/Interface_PostProcessVolume.h"
#include "JsonObjectConverter.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Osc/VibH2OOscTypes.h"
#include "Particles/ParticleSystemComponent.h"
#include "VibH2OBackdrop.h"
#include "Layout/VibH2OPositionDriver.h"
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
		{ TEXT("Exposure"), TEXT("Luminosité générale (EV)"), TEXT("Image"), 4.0f, 16.0f, 0.1f, TEXT("Toute l'image plus claire ou plus sombre. +1 = deux fois plus clair.") },
		{ TEXT("ImageContrast"), TEXT("Contraste"), TEXT("Image"), 0.3f, 2.0f, 0.01f, TEXT("1 = l'image d'origine. Plus haut : noirs plus noirs, blancs plus blancs.") },
		{ TEXT("ImageSaturation"), TEXT("Saturation (couleurs vives)"), TEXT("Image"), 0.0f, 2.0f, 0.01f, TEXT("0 = noir et blanc, 1 = d'origine, 2 = couleurs tr\u00e8s vives.") },
		{ TEXT("ImageWarmth"), TEXT("Froid ← → chaud"), TEXT("Image"), -1.0f, 1.0f, 0.01f, TEXT("0 = d'origine. \u00c0 gauche plus bleu, \u00e0 droite plus orang\u00e9.") },
		{ TEXT("ImageBloom"), TEXT("Halo lumineux"), TEXT("Image"), 0.0f, 8.0f, 0.05f, TEXT("Le halo autour de ce qui brille : bulles, particules.") },
		{ TEXT("ImageVignette"), TEXT("Bords assombris"), TEXT("Image"), 0.0f, 1.0f, 0.01f, TEXT("Assombrit les bords et les coins de l'image.") },
		{ TEXT("ImageGrain"), TEXT("Grain"), TEXT("Image"), 0.0f, 1.0f, 0.01f, TEXT("Grain de pellicule. 0 = image lisse.") },
		{ TEXT("bShowSeatNumbers"), TEXT("Numéros des spectateurs"), TEXT("Spectacle"), 0.0f, 1.0f, 1.0f, TEXT("Le num\u00e9ro de chaque spectateur au centre de sa bulle.") },
		{ TEXT("FramingMargin"), TEXT("Cadrage : marge autour de la salle"), TEXT("Spectacle"), 0.0f, 0.8f, 0.01f, TEXT("Espace autour de la salle \u00e0 l'arriv\u00e9e de la cam\u00e9ra. 0 = bord \u00e0 bord.") },
		{ TEXT("DescentSeconds"), TEXT("Descente : durée (s)"), TEXT("Spectacle"), 1.0f, 300.0f, 1.0f, TEXT("Temps de la descente lointaine (bouton Descente, touche 0).") },
		{ TEXT("ApproachPosition"), TEXT("Descente : s'arrête à (rail)"), TEXT("Spectacle"), 0.0f, 1.0f, 0.005f, TEXT("O\u00f9 s'arr\u00eate la descente lointaine : 0 = haut du rail, 1 = bas.") },
		{ TEXT("ArrivalSeconds"), TEXT("Arrivée : durée (s)"), TEXT("Spectacle"), 1.0f, 120.0f, 0.5f, TEXT("Temps de l'arriv\u00e9e sur la salle (bouton Arriv\u00e9e, touche 1).") },
		{ TEXT("bParticlesFadeIn"), TEXT("Particules : sortent du brouillard"), TEXT("Spectacle"), 0.0f, 1.0f, 1.0f, TEXT("Les particules sont absentes au d\u00e9part et apparaissent pendant la descente.") },
		{ TEXT("ParticlesAppearAt"), TEXT("Particules : apparaissent à (rail)"), TEXT("Spectacle"), 0.0f, 1.0f, 0.005f, TEXT("Moment o\u00f9 elles naissent : 0,04 = environ 7 s apr\u00e8s le d\u00e9but.") },

		{ TEXT("bFollowSchool"), TEXT("La cam\u00e9ra garde tout le banc"), TEXT("Tableau 2"), 0.0f, 1.0f, 1.0f, TEXT("En tableau 2, la cam\u00e9ra recule ou avance doucement pour garder tout le banc dans l'image.") },
		{ TEXT("TransitionSeconds"), TEXT("Transition salle \u2192 banc : dur\u00e9e (s)"), TEXT("Tableau 2"), 1.0f, 60.0f, 0.5f, TEXT("Temps du passage entre les deux tableaux quand on le commande.") },
		{ TEXT("Daughters"), TEXT("Bulles filles par spectateur"), TEXT("Tableau 2"), 0.0f, 8.0f, 1.0f, TEXT("Chaque bulle se divise en ce nombre de s\u0153urs au d\u00e9but de la transition. 0 = pas de division.") },
		{ TEXT("SisterSpread"), TEXT("\u00c9cart entre les s\u0153urs"), TEXT("Tableau 2"), 0.0f, 150.0f, 1.0f, TEXT("Distance entre une bulle et ses filles pendant la division.") },
		{ TEXT("VortexHeight"), TEXT("Vortex : hauteur"), TEXT("Tableau 2"), 100.0f, 5000.0f, 10.0f, TEXT("Hauteur du c\u00f4ne que forme le banc.") },
		{ TEXT("VortexBaseRadius"), TEXT("Vortex : largeur en bas"), TEXT("Tableau 2"), 0.0f, 3000.0f, 10.0f, TEXT("Rayon du c\u00f4ne \u00e0 sa base.") },
		{ TEXT("VortexTopRadius"), TEXT("Vortex : largeur en haut"), TEXT("Tableau 2"), 0.0f, 3000.0f, 10.0f, TEXT("Rayon du c\u00f4ne en haut : plus grand qu'en bas, il s'\u00e9vase.") },
		{ TEXT("VortexTurns"), TEXT("Vortex : nombre de tours"), TEXT("Tableau 2"), 0.0f, 10.0f, 0.1f, TEXT("Combien de fois la spirale fait le tour de bas en haut.") },
		{ TEXT("VortexSpin"), TEXT("Vortex : vitesse de rotation"), TEXT("Tableau 2"), -1.0f, 1.0f, 0.01f, TEXT("Vitesse de rotation du vortex. N\u00e9gatif : dans l'autre sens.") },
		{ TEXT("SchoolMinSpeed"), TEXT("Banc : vitesse lente"), TEXT("Tableau 2"), 1.0f, 1000.0f, 5.0f, TEXT("Vitesse des poissons les plus calmes.") },
		{ TEXT("SchoolMaxSpeed"), TEXT("Banc : vitesse rapide"), TEXT("Tableau 2"), 1.0f, 1500.0f, 5.0f, TEXT("Vitesse des plus excit\u00e9s.") },
		{ TEXT("SchoolSwirl"), TEXT("Banc : force du tourbillon"), TEXT("Tableau 2"), 0.0f, 6.0f, 0.05f, TEXT("Combien le banc est entra\u00een\u00e9 dans la rotation du vortex.") },
		{ TEXT("SchoolAlignment"), TEXT("Banc : nager ensemble"), TEXT("Tableau 2"), 0.0f, 6.0f, 0.05f, TEXT("Plus haut : les bulles prennent la m\u00eame direction que leurs voisines.") },
		{ TEXT("SchoolCohesion"), TEXT("Banc : se resserrer"), TEXT("Tableau 2"), 0.0f, 1.0f, 0.01f, TEXT("Plus haut : le banc reste group\u00e9.") },
		{ TEXT("SyncVortexAbove"), TEXT("Synchronie : vortex au-dessus de"), TEXT("Tableau 2"), 0.0f, 1.0f, 0.01f, TEXT("Au-dessus de ce niveau de synchronie de la salle, le banc tourne en vortex.") },
		{ TEXT("SyncDisperseBelow"), TEXT("Synchronie : dispersion sous"), TEXT("Tableau 2"), 0.0f, 1.0f, 0.01f, TEXT("Sous ce niveau, le banc s'\u00e9parpille en familles.") },

		{ TEXT("BubbleBrightness"), TEXT("Lumière des bulles"), TEXT("Ambiance"), 0.0f, 10.0f, 0.1f, TEXT("\u00c0 monter si les bulles se fondent dans l'eau, \u00e0 baisser si elles br\u00fblent en blanc.") },
		{ TEXT("CalmColorLow"), TEXT("Calme : les plus calmes"), TEXT("Ambiance"), 0.0f, 1.0f, 0.0f, TEXT("Ambiance calme : couleur des spectateurs les plus calmes.") },
		{ TEXT("CalmColorHigh"), TEXT("Calme : les plus agités"), TEXT("Ambiance"), 0.0f, 1.0f, 0.0f, TEXT("Ambiance calme : couleur des spectateurs les plus agit\u00e9s.") },
		{ TEXT("NormalColorLow"), TEXT("Normal : les plus calmes"), TEXT("Ambiance"), 0.0f, 1.0f, 0.0f, TEXT("Ambiance normale : spectateurs les plus calmes.") },
		{ TEXT("NormalColorHigh"), TEXT("Normal : les plus agités"), TEXT("Ambiance"), 0.0f, 1.0f, 0.0f, TEXT("Ambiance normale : spectateurs les plus agit\u00e9s.") },
		{ TEXT("ExcitedColorLow"), TEXT("Excité : les plus calmes"), TEXT("Ambiance"), 0.0f, 1.0f, 0.0f, TEXT("Ambiance excit\u00e9e : spectateurs les plus calmes.") },
		{ TEXT("ExcitedColorHigh"), TEXT("Excité : les plus agités"), TEXT("Ambiance"), 0.0f, 1.0f, 0.0f, TEXT("Ambiance excit\u00e9e : spectateurs les plus agit\u00e9s.") },
		{ TEXT("WaterColor"), TEXT("Couleur de l'eau"), TEXT("Ambiance"), 0.0f, 1.0f, 0.0f, TEXT("La couleur de l'eau derri\u00e8re les bulles.") },
		{ TEXT("WaterLight"), TEXT("Lumière de l'eau"), TEXT("Ambiance"), 0.0f, 20.0f, 0.1f, TEXT("La clart\u00e9 de l'eau elle-m\u00eame.") },
		{ TEXT("WaterDensity"), TEXT("Eau trouble"), TEXT("Ambiance"), 0.0f, 10.0f, 0.05f, TEXT("Plus trouble : la salle appara\u00eet plus tard dans la descente.") },
		{ TEXT("MoodTransitionSeconds"), TEXT("Bascule d'ambiance : durée (s)"), TEXT("Ambiance"), 0.0f, 30.0f, 0.5f, TEXT("Temps pour passer d'une ambiance \u00e0 l'autre.") },

		{ TEXT("BubbleSize"), TEXT("Taille des bulles"), TEXT("Bulles"), 0.1f, 4.0f, 0.05f, TEXT("Taille de toutes les bulles.") },
		{ TEXT("FloatAmplitude"), TEXT("Flottement : ampleur (cm)"), TEXT("Bulles"), 0.0f, 200.0f, 1.0f, TEXT("De combien les bulles bougent autour de leur place.") },
		{ TEXT("FloatSpeed"), TEXT("Flottement : vitesse"), TEXT("Bulles"), 0.0f, 5.0f, 0.01f, TEXT("Vitesse de ce mouvement.") },
		{ TEXT("BeatPop"), TEXT("Battement : gonflement"), TEXT("Bulles"), 0.0f, 1.0f, 0.01f, TEXT("Combien la bulle gonfle \u00e0 chaque battement de c\u0153ur.") },
		{ TEXT("OrganicDeform"), TEXT("Déformation organique"), TEXT("Bulles"), 0.0f, 1.0f, 0.005f, TEXT("La bulle se d\u00e9forme doucement, comme vivante.") },

		{ TEXT("ScalePerExcitation"), TEXT("Excitation : grossissement"), TEXT("Réaction"), 0.0f, 3.0f, 0.01f, TEXT("Combien une bulle grossit quand son spectateur s'excite.") },
		{ TEXT("DriftIntensity"), TEXT("Excitation : dérive (cm)"), TEXT("Réaction"), 0.0f, 500.0f, 5.0f, TEXT("Combien une bulle tr\u00e8s excit\u00e9e quitte sa place.") },
		{ TEXT("IndividualContrast"), TEXT("Contraste entre spectateurs"), TEXT("Réaction"), 0.0f, 6.0f, 0.1f, TEXT("0 = toutes les bulles pareilles. Plus haut : chacun prend sa couleur.") },
		{ TEXT("CalmThreshold"), TEXT("Seuil calme"), TEXT("Réaction"), 0.0f, 1.0f, 0.01f, TEXT("Sous ce niveau d'excitation de la salle : ambiance calme.") },
		{ TEXT("ExcitedThreshold"), TEXT("Seuil excité"), TEXT("Réaction"), 0.0f, 1.0f, 0.01f, TEXT("Au-dessus de ce niveau : ambiance excit\u00e9e.") },
		{ TEXT("SyncHueTightening"), TEXT("Synchronie : couleurs qui se rejoignent"), TEXT("Réaction"), 0.0f, 1.0f, 0.01f, TEXT("Quand la salle est synchrone, les couleurs se rejoignent.") },
		{ TEXT("SyncHalo"), TEXT("Synchronie : halo"), TEXT("Réaction"), 0.0f, 3.0f, 0.05f, TEXT("Halo commun qui s'allume quand la salle est synchrone.") },
	};

	/** The water's numbers are tiny; the desk shows them a hundred times larger. */
	constexpr float WaterScale = 0.01f;

	/** How often the framing is worked out again during play. */
	constexpr double FramingInterval = 0.5;

	/** Every post-process volume of the world. 5.8 replaced the public list with an iterator. */
	static TArray<IInterface_PostProcessVolume*> GetPostProcessVolumes(const UWorld* World)
	{
#if UE_VERSION_OLDER_THAN(5, 8, 0)
		return World->PostProcessVolumes;
#else
		TArray<IInterface_PostProcessVolume*> Volumes;
		for (IInterface_PostProcessVolume& Volume : World->GetPostProcessVolumeIterator())
		{
			Volumes.Add(&Volume);
		}
		return Volumes;
#endif
	}

	static FProperty* FindSetting(const FString& Id)
	{
		return FVibH2ORegieSettings::StaticStruct()->FindPropertyByName(FName(*Id));
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
	else if (!bImageCaptured && ExposureVolume != nullptr)
	{
		// A desk placed before the image controls existed reads them now.
		CaptureImage();
	}
	if (bCaptured && !bTableau2Captured && Stage != nullptr)
	{
		// Same for the tableau 2 controls: start from what the stage plays.
		CaptureTableau2();
	}
	ApplyChanged(false);
}

bool AVibH2ORegie::IsEditorWorld() const
{
	const UWorld* World = GetWorld();
	return World != nullptr && !World->IsGameWorld();
}

void AVibH2ORegie::Notice(const FString& Text)
{
	NoticeText = Text;
	++NoticeSerial;
	UE_LOG(LogVibH2O, Log, TEXT("VibH2O Regie : %s"), *Text);
}

void AVibH2ORegie::BeginPlay()
{
	Super::BeginPlay();
	ResolveConnections();
	RefreshPresetNames();
	// The saved scene is the truth at Play: the desk reads it, writes nothing.
	ApplyChanged(false);
	GatherParticles();
	ParticlesState = -1;

	if (UVibH2OSubsystem* Subsystem = UVibH2OSubsystem::Get(this))
	{
		RegieMessageHandle = Subsystem->OnRegieMessage.AddUObject(this, &AVibH2ORegie::HandleRegieMessage);
	}
	if (bConduiteKeys)
	{
		if (APlayerController* Controller = GetWorld()->GetFirstPlayerController())
		{
			EnableInput(Controller);
			InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AVibH2ORegie::ConduiteGo);
			InputComponent->BindKey(EKeys::PageDown, IE_Pressed, this, &AVibH2ORegie::ConduiteGo);
			InputComponent->BindKey(EKeys::PageUp, IE_Pressed, this, &AVibH2ORegie::ConduitePrevious);
		}
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
	TickCameraPath();

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
	// Whoever moved the rail other than the desk - a key of the camera
	// controller, a Level Sequence - has priority for a moment.
	if (Rail != nullptr)
	{
		const float RailNow = Rail->CurrentPositionOnRail;
		const bool bMovedByUs = LastWrittenRail >= 0.0f && FMath::IsNearlyEqual(RailNow, LastWrittenRail, 1e-4f);
		if (PreviousRail >= 0.0f && !FMath::IsNearlyEqual(RailNow, PreviousRail, 1e-5f) && !bMovedByUs)
		{
			ExternalRailMoveTime = Now;
		}
		PreviousRail = RailNow;
	}
	TickCamera(DeltaSeconds);
	TickFollowSchool(DeltaSeconds);
	TickParticles();
}

void AVibH2ORegie::ResolveMissingConnections()
{
	const bool bMissing = Stage == nullptr || ExposureVolume == nullptr || Water == nullptr || Rail == nullptr
		|| (CameraController == nullptr && !CameraControllerClassName.IsEmpty());
	const double Now = FPlatformTime::Seconds();
	if (bMissing && Now - LastResolveTime > 5.0)
	{
		LastResolveTime = Now;
		ResolveConnections();
	}
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
			// Labels exist in the editor only: a packaged game relies on the
			// volume the desk saved with the map.
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
	if (ShowSequence == nullptr)
	{
		// The first sequence that has steps: a sequence without markers is
		// not a show's running order.
		for (TActorIterator<ALevelSequenceActor> It(World); It; ++It)
		{
			const ULevelSequence* Sequence = It->GetSequence();
			const UMovieScene* MovieScene = Sequence != nullptr ? Sequence->GetMovieScene() : nullptr;
			if (MovieScene != nullptr && MovieScene->GetMarkedFrames().Num() > 0)
			{
				ShowSequence = *It;
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

bool AVibH2ORegie::Changed(const TCHAR* Name) const
{
	return bApplyAll || ChangedNow.Contains(FName(Name));
}

void AVibH2ORegie::ApplyChanged(bool bForce)
{
	if (!bForce && !bHasApplied)
	{
		// Loading the map, or starting Play: what the scene holds is the
		// truth - a value set straight on the stage in rehearsal stays. The
		// desk reads it and writes nothing.
		ReadScene();
		Applied = Reglages;
		bHasApplied = true;
		// The output format is not held by the scene: it is set every time.
		ApplyToOutput();
		return;
	}

	ChangedNow.Reset();
	bApplyAll = bForce;
	if (!bForce)
	{
		// Setting by setting: moving one slider touches one value, never the
		// ones next to it that someone may have tuned by hand.
		for (TFieldIterator<FProperty> It(FVibH2ORegieSettings::StaticStruct()); It; ++It)
		{
			if (!It->Identical_InContainer(&Reglages, &Applied))
			{
				ChangedNow.Add(It->GetFName());
			}
		}
		if (ChangedNow.Num() == 0)
		{
			if (bWaterPending)
			{
				ApplyToWater();
			}
			return;
		}
	}

	if (Changed(TEXT("OutputFormat")))
	{
		ApplyToOutput();
	}
	ApplyToStage();
	ApplyToExposure();
	ApplyToImage();
	ApplyToTableau2();
	if (bForce || bWaterPending || Changed(TEXT("WaterColor")) || Changed(TEXT("WaterLight")) || Changed(TEXT("WaterDensity")))
	{
		ApplyToWater();
	}
	if (Changed(TEXT("ApproachPosition")) || Changed(TEXT("DescentSeconds")) || Changed(TEXT("ArrivalSeconds")))
	{
		ApplyToCameraController();
	}
	if (Changed(TEXT("FramingMargin")) || Changed(TEXT("OutputFormat")))
	{
		RefreshFraming();
	}

	Applied = Reglages;
	bHasApplied = true;
	bApplyAll = false;
	ChangedNow.Reset();

#if WITH_EDITOR
	// Values written into other actors of the level: make sure the level asks
	// to be saved.
	if (IsEditorWorld())
	{
		MarkPackageDirty();
	}
#endif
}

void AVibH2ORegie::ApplyToStage()
{
	if (Stage == nullptr)
	{
		return;
	}
	const FVibH2ORegieSettings& R = Reglages;
#define VIBH2O_PUSH(Setting, Target) if (Changed(TEXT(#Setting))) { Target = R.Setting; }
	VIBH2O_PUSH(bShowSeatNumbers, Stage->bShowSeatNumbers);
	VIBH2O_PUSH(BubbleBrightness, Stage->BubbleBrightness);
	VIBH2O_PUSH(BubbleSize, Stage->BaseScale);
	VIBH2O_PUSH(FloatAmplitude, Stage->FloatAmplitude);
	VIBH2O_PUSH(FloatSpeed, Stage->FloatSpeed);
	VIBH2O_PUSH(BeatPop, Stage->BeatPopAmount);
	VIBH2O_PUSH(OrganicDeform, Stage->OrganicDeform);
	VIBH2O_PUSH(ScalePerExcitation, Stage->ScalePerExcitation);
	VIBH2O_PUSH(DriftIntensity, Stage->DriftIntensity);
	VIBH2O_PUSH(CalmThreshold, Stage->CalmThreshold);
	VIBH2O_PUSH(MoodTransitionSeconds, Stage->MoodTransitionSeconds);
	if (Changed(TEXT("ExcitedThreshold")) || Changed(TEXT("CalmThreshold")))
	{
		Stage->ExcitedThreshold = FMath::Max(R.ExcitedThreshold, R.CalmThreshold);
	}

	FVibH2OMoodPalette& Palette = Stage->MoodPalette;
	VIBH2O_PUSH(CalmColorLow, Palette.Calm.ColorLow);
	VIBH2O_PUSH(CalmColorHigh, Palette.Calm.ColorHigh);
	VIBH2O_PUSH(NormalColorLow, Palette.Normal.ColorLow);
	VIBH2O_PUSH(NormalColorHigh, Palette.Normal.ColorHigh);
	VIBH2O_PUSH(ExcitedColorLow, Palette.Excited.ColorLow);
	VIBH2O_PUSH(ExcitedColorHigh, Palette.Excited.ColorHigh);
	VIBH2O_PUSH(IndividualContrast, Palette.IndividualContrast);
	VIBH2O_PUSH(SyncHueTightening, Palette.SyncHueTightening);
	VIBH2O_PUSH(SyncHalo, Palette.SyncHalo);
#undef VIBH2O_PUSH
}

void AVibH2ORegie::ApplyToExposure()
{
	if (ExposureVolume == nullptr)
	{
		return;
	}
	if (!Changed(TEXT("Exposure")))
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
	if (!CachedBackdrop.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			for (TActorIterator<AVibH2OBackdrop> It(World); It; ++It)
			{
				CachedBackdrop = *It;
				break;
			}
		}
	}
	const bool bBackdropAway = CachedBackdrop.IsValid() && CachedBackdrop->Presence < 1.0f;
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

void AVibH2ORegie::ApplyToTableau2()
{
	if (Stage == nullptr || !bTableau2Captured)
	{
		return;
	}
	const FVibH2ORegieSettings& R = Reglages;
#define VIBH2O_PUSH(Setting, Target) if (Changed(TEXT(#Setting))) { Target = R.Setting; }
	VIBH2O_PUSH(TransitionSeconds, Stage->OscBlendGlideSeconds);
	if (Changed(TEXT("Daughters")))
	{
		Stage->DaughtersPerBubble = FMath::Clamp(FMath::RoundToInt(R.Daughters), 0, 8);
	}
	VIBH2O_PUSH(SisterSpread, Stage->DivisionSpread);
	VIBH2O_PUSH(VortexHeight, Stage->VortexHeight);
	VIBH2O_PUSH(VortexBaseRadius, Stage->VortexBaseRadius);
	VIBH2O_PUSH(VortexTopRadius, Stage->VortexTopRadius);
	VIBH2O_PUSH(VortexTurns, Stage->VortexTurns);
	VIBH2O_PUSH(VortexSpin, Stage->VortexSpinSpeed);
	VIBH2O_PUSH(SyncVortexAbove, Stage->SchoolVortexAbove);
	VIBH2O_PUSH(SyncDisperseBelow, Stage->SchoolDisperseBelow);
	// The school keeps its fast fish faster than its slow ones.
	if (UVibH2OFlockDriver* Flock = Cast<UVibH2OFlockDriver>(Stage->FlockDriver))
	{
		VIBH2O_PUSH(SchoolMinSpeed, Flock->MinSpeed);
		VIBH2O_PUSH(SchoolMaxSpeed, Flock->MaxSpeed);
		VIBH2O_PUSH(SchoolSwirl, Flock->SwirlWeight);
		VIBH2O_PUSH(SchoolAlignment, Flock->AlignmentWeight);
		VIBH2O_PUSH(SchoolCohesion, Flock->CohesionWeight);
		Flock->MaxSpeed = FMath::Max(Flock->MaxSpeed, Flock->MinSpeed);
	}
#undef VIBH2O_PUSH
}

void AVibH2ORegie::CaptureTableau2()
{
	if (Stage == nullptr)
	{
		return;
	}
	FVibH2ORegieSettings& R = Reglages;
	R.TransitionSeconds = Stage->OscBlendGlideSeconds;
	R.Daughters = static_cast<float>(Stage->DaughtersPerBubble);
	R.SisterSpread = Stage->DivisionSpread;
	R.VortexHeight = Stage->VortexHeight;
	R.VortexBaseRadius = Stage->VortexBaseRadius;
	R.VortexTopRadius = Stage->VortexTopRadius;
	R.VortexTurns = Stage->VortexTurns;
	R.VortexSpin = Stage->VortexSpinSpeed;
	R.SyncVortexAbove = Stage->SchoolVortexAbove;
	R.SyncDisperseBelow = Stage->SchoolDisperseBelow;
	if (const UVibH2OFlockDriver* Flock = Cast<UVibH2OFlockDriver>(Stage->FlockDriver))
	{
		R.SchoolMinSpeed = Flock->MinSpeed;
		R.SchoolMaxSpeed = Flock->MaxSpeed;
		R.SchoolSwirl = Flock->SwirlWeight;
		R.SchoolAlignment = Flock->AlignmentWeight;
		R.SchoolCohesion = Flock->CohesionWeight;
	}
	bTableau2Captured = true;
}

void AVibH2ORegie::CaptureImage()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	// What the camera sees is, setting by setting, the value of the most
	// important volume that sets it. Read the same way, so the desk starts
	// exactly from the scene's look.
	TArray<FPostProcessVolumeProperties> Volumes;
	for (IInterface_PostProcessVolume* Volume : VibH2ORegieInternal::GetPostProcessVolumes(World))
	{
		if (Volume != nullptr)
		{
			const FPostProcessVolumeProperties Properties = Volume->GetProperties();
			if (Properties.bIsEnabled && Properties.Settings != nullptr && Properties.BlendWeight > 0.0f)
			{
				Volumes.Add(Properties);
			}
		}
	}
	Volumes.Sort([](const FPostProcessVolumeProperties& A, const FPostProcessVolumeProperties& B) { return A.Priority > B.Priority; });

	// The image sliders write on the desk's volume: a more important volume
	// setting the same values would make them silently dead.
	if (ExposureVolume != nullptr)
	{
		const float Mine = ExposureVolume->Priority;
		for (const FPostProcessVolumeProperties& Volume : Volumes)
		{
			const FPostProcessSettings& S = *Volume.Settings;
			if (Volume.Priority > Mine && Volume.Settings != &ExposureVolume->Settings
				&& (S.bOverride_ColorContrast || S.bOverride_ColorSaturation || S.bOverride_WhiteTemp || S.bOverride_BloomIntensity || S.bOverride_VignetteIntensity || S.bOverride_FilmGrainIntensity))
			{
				Notice(TEXT("Attention : un volume de post-process plus prioritaire que celui de la Régie règle aussi l'image ; certains curseurs Image n'auront pas d'effet."));
				break;
			}
		}
	}

	const FPostProcessSettings Defaults;
	auto Pick = [&Volumes, &Defaults](auto OverrideOf, auto ValueOf)
	{
		for (const FPostProcessVolumeProperties& Volume : Volumes)
		{
			if (OverrideOf(*Volume.Settings))
			{
				return ValueOf(*Volume.Settings);
			}
		}
		return ValueOf(Defaults);
	};
	BaseContrast = Pick([](const FPostProcessSettings& S) { return S.bOverride_ColorContrast; }, [](const FPostProcessSettings& S) { return S.ColorContrast; });
	BaseSaturation = Pick([](const FPostProcessSettings& S) { return S.bOverride_ColorSaturation; }, [](const FPostProcessSettings& S) { return S.ColorSaturation; });
	BaseWhiteTemp = Pick([](const FPostProcessSettings& S) { return S.bOverride_WhiteTemp; }, [](const FPostProcessSettings& S) { return S.WhiteTemp; });
	BaseBloom = Pick([](const FPostProcessSettings& S) { return S.bOverride_BloomIntensity; }, [](const FPostProcessSettings& S) { return S.BloomIntensity; });
	BaseVignette = Pick([](const FPostProcessSettings& S) { return S.bOverride_VignetteIntensity; }, [](const FPostProcessSettings& S) { return S.VignetteIntensity; });
	BaseGrain = Pick([](const FPostProcessSettings& S) { return S.bOverride_FilmGrainIntensity; }, [](const FPostProcessSettings& S) { return S.FilmGrainIntensity; });

	Reglages.ImageContrast = 1.0f;
	Reglages.ImageSaturation = 1.0f;
	Reglages.ImageWarmth = 0.0f;
	Reglages.ImageBloom = BaseBloom;
	Reglages.ImageVignette = BaseVignette;
	Reglages.ImageGrain = BaseGrain;
	bImageCaptured = true;
}

void AVibH2ORegie::ResetImage()
{
#if WITH_EDITOR
	Modify();
#endif
	if (!bImageCaptured)
	{
		CaptureImage();
	}
	Reglages.ImageContrast = 1.0f;
	Reglages.ImageSaturation = 1.0f;
	Reglages.ImageWarmth = 0.0f;
	Reglages.ImageBloom = BaseBloom;
	Reglages.ImageVignette = BaseVignette;
	Reglages.ImageGrain = BaseGrain;
	ApplyChanged(false);
}

void AVibH2ORegie::ApplyToImage()
{
	if (ExposureVolume == nullptr || !bImageCaptured)
	{
		return;
	}
	// On the desk's own volume, which outranks the others: the channel
	// balance the artist set stays, only the overall amount moves.
	FPostProcessSettings& S = ExposureVolume->Settings;
	if (Changed(TEXT("ImageContrast")))
	{
		S.bOverride_ColorContrast = true;
		S.ColorContrast = FVector4(BaseContrast.X, BaseContrast.Y, BaseContrast.Z, BaseContrast.W * Reglages.ImageContrast);
	}
	if (Changed(TEXT("ImageSaturation")))
	{
		S.bOverride_ColorSaturation = true;
		S.ColorSaturation = FVector4(BaseSaturation.X, BaseSaturation.Y, BaseSaturation.Z, BaseSaturation.W * Reglages.ImageSaturation);
	}
	if (Changed(TEXT("ImageWarmth")))
	{
		S.bOverride_WhiteTemp = true;
		S.WhiteTemp = FMath::Clamp(BaseWhiteTemp + 2500.0f * Reglages.ImageWarmth, 1500.0f, 15000.0f);
	}
	if (Changed(TEXT("ImageBloom")))
	{
		S.bOverride_BloomIntensity = true;
		S.BloomIntensity = Reglages.ImageBloom;
	}
	if (Changed(TEXT("ImageVignette")))
	{
		S.bOverride_VignetteIntensity = true;
		S.VignetteIntensity = Reglages.ImageVignette;
	}
	if (Changed(TEXT("ImageGrain")))
	{
		S.bOverride_FilmGrainIntensity = true;
		S.FilmGrainIntensity = Reglages.ImageGrain;
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

void AVibH2ORegie::ReadScene()
{
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
		if (bTableau2Captured)
		{
			CaptureTableau2();
		}
	}

	// The exposure the camera actually gets: the most important volume that
	// sets it, the desk's own included.
	if (UWorld* World = GetWorld())
	{
		TArray<FPostProcessVolumeProperties> Volumes;
		for (IInterface_PostProcessVolume* Volume : VibH2ORegieInternal::GetPostProcessVolumes(World))
		{
			if (Volume != nullptr)
			{
				const FPostProcessVolumeProperties Properties = Volume->GetProperties();
				if (Properties.bIsEnabled && Properties.Settings != nullptr && Properties.Settings->bOverride_AutoExposureBias)
				{
					Volumes.Add(Properties);
				}
			}
		}
		if (ExposureVolume != nullptr && ExposureVolume->Settings.bOverride_AutoExposureBias)
		{
			R.Exposure = ExposureVolume->Settings.AutoExposureBias;
		}
		else if (Volumes.Num() > 0)
		{
			Volumes.Sort([](const FPostProcessVolumeProperties& A, const FPostProcessVolumeProperties& B) { return A.Priority > B.Priority; });
			R.Exposure = Volumes[0].Settings->AutoExposureBias;
		}
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

	// The director's camera controller: its end of descent and its durations
	// are read, not imposed.
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
		auto ReadNumber = [this](const TCHAR* Name, float& Out)
		{
			const FProperty* Property = FindVariable(CameraController, Name);
			if (const FDoubleProperty* Double = CastField<FDoubleProperty>(Property))
			{
				Out = static_cast<float>(Double->GetPropertyValue_InContainer(CameraController));
			}
			else if (const FFloatProperty* Float = CastField<FFloatProperty>(Property))
			{
				Out = Float->GetPropertyValue_InContainer(CameraController);
			}
		};
		ReadNumber(TEXT("Duree Sequence0"), R.DescentSeconds);
		ReadNumber(TEXT("Duree Sequence1"), R.ArrivalSeconds);
	}
}

void AVibH2ORegie::CaptureFromScene()
{
#if WITH_EDITOR
	Modify();
#endif
	ResolveConnections();
	if (!bImageCaptured)
	{
		CaptureImage();
	}
	if (!bTableau2Captured)
	{
		CaptureTableau2();
	}
	ReadScene();

	// What was read is what is applied: nothing moves - except the output
	// format, which the scene does not hold and the next apply sets.
	Applied = Reglages;
	Applied.OutputFormat = Reglages.OutputFormat == EVibH2OOutputFormat::Libre ? EVibH2OOutputFormat::TulleFace : EVibH2OOutputFormat::Libre;
	bHasApplied = true;
}

void AVibH2ORegie::RecaptureImage()
{
#if WITH_EDITOR
	Modify();
#endif
	ResolveConnections();
	CaptureImage();
	Applied.ImageContrast = Reglages.ImageContrast;
	Applied.ImageSaturation = Reglages.ImageSaturation;
	Applied.ImageWarmth = Reglages.ImageWarmth;
	Applied.ImageBloom = Reglages.ImageBloom;
	Applied.ImageVignette = Reglages.ImageVignette;
	Applied.ImageGrain = Reglages.ImageGrain;
	Notice(TEXT("Image actuelle prise comme image d'origine."));
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

bool AVibH2ORegie::ComputeFramedRailPosition(float& OutPosition, bool bForSchool) const
{
	if (Stage == nullptr || Rail == nullptr || Stage->GetBubbles().Num() == 0)
	{
		return false;
	}
	// The end point of the descent is framed for the seating plan: in
	// tableau 2 the bubbles leave their seats, and it must not chase them.
	// Following the school is a separate request.
	if (!bForSchool && Stage->BlendAlpha > 0.01f)
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

	// The room at rest - it does not breathe with the audience - or, when
	// following the school, where the bubbles actually are.
	const FBox Room = bForSchool ? Stage->GetRoomBounds() : Stage->GetSeatPlanBounds();
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
	ResolveMissingConnections();
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

void AVibH2ORegie::TickCameraPath()
{
	if (!AppliedCameraPath.IsSet())
	{
		// The value saved with the level is where things already stand:
		// loading the map or starting Play moves nothing.
		AppliedCameraPath = CameraPath;
		return;
	}
	if (CameraPath < 0.0f || FMath::IsNearlyEqual(CameraPath, AppliedCameraPath.GetValue(), 1e-5f))
	{
		AppliedCameraPath = CameraPath;
		return;
	}
	AppliedCameraPath = CameraPath;
	ResolveMissingConnections();
	if (FramedRailPosition < 0.0f)
	{
		RefreshFraming();
	}
	// The sequence has the camera: a move started from the tablet gives way.
	if (CameraMove != EVibH2ORegieCameraMove::None)
	{
		StopCamera();
	}
	const float Approach = Reglages.ApproachPosition;
	const float Framed = FramedRailPosition >= 0.0f ? FramedRailPosition : Approach;
	const float Path = FMath::Clamp(CameraPath, 0.0f, 2.0f);
	SetRailPosition(Path <= 1.0f ? FMath::Lerp(0.0f, Approach, Path) : FMath::Lerp(Approach, Framed, Path - 1.0f));
}

void AVibH2ORegie::TickFollowSchool(float DeltaSeconds)
{
	if (Stage == nullptr || Rail == nullptr || CameraMove != EVibH2ORegieCameraMove::None)
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	const bool bInSchool = Stage->BlendAlpha > 0.05f;

	if (!bInSchool || !Reglages.bFollowSchool)
	{
		// Back in the room: return to the room's overall view, once.
		if (bFollowingSchool)
		{
			bFollowingSchool = false;
			if (bInSchool == false && FramedRailPosition >= 0.0f)
			{
				GlideCameraTo(FramedRailPosition, FMath::Max(Reglages.ArrivalSeconds * 0.5f, 2.0f));
			}
		}
		return;
	}
	if (Now - ExternalRailMoveTime < 1.0)
	{
		return;
	}

	// A few times a second is enough: the school moves slowly, and the
	// camera follows it more slowly still.
	if (Now - LastSchoolFramingTime > 0.25)
	{
		LastSchoolFramingTime = Now;
		float Target = 0.0f;
		if (ComputeFramedRailPosition(Target, true))
		{
			SchoolTarget = Target;
		}
	}
	if (SchoolTarget < 0.0f)
	{
		return;
	}
	bFollowingSchool = true;
	const float Current = Rail->CurrentPositionOnRail;
	if (FMath::Abs(SchoolTarget - Current) < 0.0005f)
	{
		return;
	}
	// About three seconds to settle: no pumping when the school breathes.
	const float Alpha = 1.0f - FMath::Exp(-FMath::Max(DeltaSeconds, 0.0f) / 3.0f);
	SetRailPosition(FMath::Lerp(Current, SchoolTarget, Alpha));
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
		// The effect's own name first: a packaged game has no actor labels.
		const bool bActorMatches = Actor->GetClass()->GetName().Contains(ParticleNameFilter)
#if WITH_EDITOR
			|| Actor->GetActorLabel().Contains(ParticleNameFilter)
#endif
			;
		TInlineComponentArray<UFXSystemComponent*> Components(Actor);
		for (UFXSystemComponent* Component : Components)
		{
			const UFXSystemAsset* Asset = Component->GetFXSystemAsset();
			if (bActorMatches || Component->GetName().Contains(ParticleNameFilter) || (Asset != nullptr && Asset->GetName().Contains(ParticleNameFilter)))
			{
				Particles.Add(Component);
			}
		}
	}
	if (Particles.Num() == 0)
	{
		UE_LOG(LogVibH2O, Warning, TEXT("VibH2O Regie : aucun systeme de particules dont le nom contient \"%s\" - elles ne sortiront pas du brouillard."), *ParticleNameFilter);
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

// ---------------------------------------------------------------- Conduite

ULevelSequencePlayer* AVibH2ORegie::GetShowPlayer() const
{
	return ShowSequence != nullptr ? ShowSequence->GetSequencePlayer() : nullptr;
}

TArray<AVibH2ORegie::FConduiteStep> AVibH2ORegie::GatherConduiteSteps() const
{
	TArray<FConduiteStep> Steps;
	const ULevelSequence* Sequence = ShowSequence != nullptr ? ShowSequence->GetSequence() : nullptr;
	const UMovieScene* MovieScene = Sequence != nullptr ? Sequence->GetMovieScene() : nullptr;
	if (MovieScene == nullptr)
	{
		return Steps;
	}
	const FFrameRate Tick = MovieScene->GetTickResolution();
	const FFrameRate Display = MovieScene->GetDisplayRate();
	const FFrameNumber Start = MovieScene->GetPlaybackRange().HasLowerBound() ? MovieScene->GetPlaybackRange().GetLowerBoundValue() : FFrameNumber(0);

	Steps.Add({ TEXT("Début"), FFrameRate::TransformTime(FFrameTime(Start), Tick, Display) });
	TArray<FMovieSceneMarkedFrame> Marks = MovieScene->GetMarkedFrames();
	Marks.Sort([](const FMovieSceneMarkedFrame& A, const FMovieSceneMarkedFrame& B) { return A.FrameNumber < B.FrameNumber; });
	for (const FMovieSceneMarkedFrame& Mark : Marks)
	{
		if (Mark.FrameNumber <= Start)
		{
			// A marker on the very first frame names the start.
			Steps[0].Label = Mark.Label.IsEmpty() ? Steps[0].Label : Mark.Label;
			continue;
		}
		Steps.Add({ Mark.Label.IsEmpty() ? FString::Printf(TEXT("Étape %d"), Steps.Num()) : Mark.Label,
			FFrameRate::TransformTime(FFrameTime(Mark.FrameNumber), Tick, Display) });
	}
	return Steps;
}

TArray<FString> AVibH2ORegie::GetConduiteSteps() const
{
	TArray<FString> Names;
	for (const FConduiteStep& Step : GatherConduiteSteps())
	{
		Names.Add(Step.Label);
	}
	return Names;
}

int32 AVibH2ORegie::GetConduiteStep() const
{
	const ULevelSequencePlayer* Player = GetShowPlayer();
	const TArray<FConduiteStep> Steps = GatherConduiteSteps();
	if (Player == nullptr || Steps.Num() == 0)
	{
		return -1;
	}
	// The last step at or before the play head - half a frame of slack, so a
	// stop exactly on a marker reads as that marker.
	const FFrameTime Now = Player->GetCurrentTime().Time + FFrameTime(0, 0.5f);
	int32 Index = 0;
	for (int32 Step = 0; Step < Steps.Num(); ++Step)
	{
		if (Steps[Step].Time <= Now)
		{
			Index = Step;
		}
	}
	return Index;
}

void AVibH2ORegie::JumpConduite(const TArray<FConduiteStep>& Steps, int32 Index)
{
	ULevelSequencePlayer* Player = GetShowPlayer();
	if (Player == nullptr || !Steps.IsValidIndex(Index))
	{
		return;
	}
	Player->Pause();
	FMovieSceneSequencePlaybackParams Params(Steps[Index].Time, EUpdatePositionMethod::Jump);
	Player->SetPlaybackPosition(Params);
	UE_LOG(LogVibH2O, Log, TEXT("VibH2O Conduite : etape %d \"%s\"."), Index, *Steps[Index].Label);
}

void AVibH2ORegie::ConduiteGo()
{
	ResolveConnections();
	ULevelSequencePlayer* Player = GetShowPlayer();
	const TArray<FConduiteStep> Steps = GatherConduiteSteps();
	if (Player == nullptr || Steps.Num() == 0)
	{
		Notice(TEXT("Conduite : aucune séquence du spectacle. Dans Unreal, Régie → Séquence du spectacle."));
		return;
	}
	const int32 Next = GetConduiteStep() + 1;
	if (Steps.IsValidIndex(Next))
	{
		// Up to the next marker, and stop there: the next GO goes on.
		FMovieSceneSequencePlaybackParams To(Steps[Next].Time, EUpdatePositionMethod::Play);
		Player->PlayTo(To, FMovieSceneSequencePlayToParams());
		UE_LOG(LogVibH2O, Log, TEXT("VibH2O Conduite : GO vers \"%s\"."), *Steps[Next].Label);
	}
	else
	{
		// Past the last marker: the end of the sequence.
		Player->Play();
		UE_LOG(LogVibH2O, Log, TEXT("VibH2O Conduite : GO jusqu'a la fin."));
	}
}

void AVibH2ORegie::ConduitePrevious()
{
	ResolveConnections();
	const TArray<FConduiteStep> Steps = GatherConduiteSteps();
	const ULevelSequencePlayer* Player = GetShowPlayer();
	if (Player == nullptr || Steps.Num() == 0)
	{
		return;
	}
	const int32 Current = GetConduiteStep();
	// Inside a step: back to its start. On a step's very start: the one before.
	const bool bAtStart = FMath::Abs((Player->GetCurrentTime().Time - Steps[Current].Time).AsDecimal()) < 0.5;
	JumpConduite(Steps, bAtStart ? FMath::Max(Current - 1, 0) : Current);
}

void AVibH2ORegie::ConduiteNext()
{
	ResolveConnections();
	const TArray<FConduiteStep> Steps = GatherConduiteSteps();
	JumpConduite(Steps, FMath::Min(GetConduiteStep() + 1, Steps.Num() - 1));
}

void AVibH2ORegie::ConduiteGoTo(int32 Step)
{
	ResolveConnections();
	JumpConduite(GatherConduiteSteps(), Step);
}

void AVibH2ORegie::ConduitePause()
{
	ULevelSequencePlayer* Player = GetShowPlayer();
	if (Player == nullptr)
	{
		return;
	}
	if (Player->IsPlaying())
	{
		Player->Pause();
	}
	else
	{
		Player->Play();
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
	Notice(bSaved ? FString::Printf(TEXT("Préréglage \"%s\" enregistré."), *Clean)
		: FString::Printf(TEXT("Préréglage \"%s\" NON enregistré (dossier Saved protégé ?)."), *Clean));
	RefreshPresetNames();
	return bSaved;
}

bool AVibH2ORegie::LoadPreset(const FString& Name)
{
	const FString Clean = FPaths::MakeValidFileName(Name.TrimStartAndEnd());
	FString Json;
	if (Clean.IsEmpty() || !FFileHelper::LoadFileToString(Json, *FPaths::Combine(GetPresetFolder(), Clean + TEXT(".json"))))
	{
		Notice(FString::Printf(TEXT("Préréglage \"%s\" introuvable."), *Name));
		return false;
	}
	// Starting from the current values: a preset saved before a setting
	// existed leaves that setting as it is.
	FVibH2ORegieSettings Loaded = Reglages;
	if (!FJsonObjectConverter::JsonObjectStringToUStruct(Json, &Loaded, 0, 0))
	{
		Notice(FString::Printf(TEXT("Préréglage \"%s\" illisible."), *Clean));
		return false;
	}
	// A hand-edited file cannot push a value off its slider.
	for (const FVibH2ORegieControl& Control : VibH2ORegieInternal::Controls)
	{
		if (FFloatProperty* Float = CastField<FFloatProperty>(VibH2ORegieInternal::FindSetting(Control.Id)))
		{
			float* Value = Float->ContainerPtrToValuePtr<float>(&Loaded);
			*Value = FMath::Clamp(*Value, Control.Min, Control.Max);
		}
	}
	Reglages = Loaded;
	ApplyChanged(false);
	Notice(FString::Printf(TEXT("Préréglage \"%s\" rappelé."), *Clean));
	return true;
}

bool AVibH2ORegie::DeletePreset(const FString& Name)
{
	const FString Clean = FPaths::MakeValidFileName(Name.TrimStartAndEnd());
	const bool bDeleted = !Clean.IsEmpty() && IFileManager::Get().Delete(*FPaths::Combine(GetPresetFolder(), Clean + TEXT(".json")));
	Notice(bDeleted ? FString::Printf(TEXT("Préréglage \"%s\" supprimé."), *Clean) : FString::Printf(TEXT("Préréglage \"%s\" introuvable."), *Clean));
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
	FString AllWords = FString::Join(Words, TEXT(" "));
	// The tablet sends a name as one token, "~" then percent-encoded: spaces,
	// dashes and leading zeros arrive untouched by the number parser.
	if (Words.Num() == 1 && Words[0].StartsWith(TEXT("~")))
	{
		const FString Encoded = Words[0].Mid(1);
		TArray<uint8> Bytes;
		for (int32 Index = 0; Index < Encoded.Len(); ++Index)
		{
			if (Encoded[Index] == TEXT('%') && Index + 2 < Encoded.Len() && FChar::IsHexDigit(Encoded[Index + 1]) && FChar::IsHexDigit(Encoded[Index + 2]))
			{
				Bytes.Add(static_cast<uint8>(FParse::HexDigit(Encoded[Index + 1]) * 16 + FParse::HexDigit(Encoded[Index + 2])));
				Index += 2;
			}
			else
			{
				Bytes.Add(static_cast<uint8>(Encoded[Index]));
			}
		}
		Bytes.Add(0);
		AllWords = UTF8_TO_TCHAR(reinterpret_cast<const char*>(Bytes.GetData()));
	}

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

	if (Head == TEXT("conduite"))
	{
		const FString Command = (Segments.Num() > 1 ? Segments[1] : (Words.Num() > 0 ? Words[0] : FString())).ToLower();
		if (Command == TEXT("go"))
		{
			ConduiteGo();
		}
		else if (Command.StartsWith(TEXT("prec")) || Command.StartsWith(TEXT("prev")))
		{
			ConduitePrevious();
		}
		else if (Command.StartsWith(TEXT("suiv")) || Command.StartsWith(TEXT("next")))
		{
			ConduiteNext();
		}
		else if (Command.StartsWith(TEXT("pause")))
		{
			ConduitePause();
		}
		else if (Command.StartsWith(TEXT("aller")) && Words.Num() > 0)
		{
			ConduiteGoTo(FCString::Atoi(*Words.Last()));
		}
		return;
	}

	if (Head == TEXT("image"))
	{
		ResetImage();
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
		Entry->SetStringField(TEXT("help"), Control.Help != nullptr ? Control.Help : TEXT(""));
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
	// The running order, for the tablet's first tab.
	const TSharedRef<FJsonObject> Conduite = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> StepNames;
	for (const FString& Name : GetConduiteSteps())
	{
		StepNames.Add(MakeShared<FJsonValueString>(Name));
	}
	Conduite->SetArrayField(TEXT("steps"), StepNames);
	Conduite->SetNumberField(TEXT("current"), GetConduiteStep());
	const ULevelSequencePlayer* ShowPlayer = GetShowPlayer();
	Conduite->SetBoolField(TEXT("playing"), ShowPlayer != nullptr && ShowPlayer->IsPlaying());
	Conduite->SetStringField(TEXT("sequence"), ShowSequence != nullptr && ShowSequence->GetSequence() != nullptr ? ShowSequence->GetSequence()->GetName() : FString());
	Desk->SetObjectField(TEXT("conduite"), Conduite);

	const TSharedRef<FJsonObject> Last = MakeShared<FJsonObject>();
	Last->SetNumberField(TEXT("serial"), NoticeSerial);
	Last->SetStringField(TEXT("text"), NoticeText);
	Desk->SetObjectField(TEXT("notice"), Last);

	Root->SetObjectField(TEXT("regie"), Desk);
}
