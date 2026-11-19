// The control desk: the few settings a show needs, in one place.
//
// The stage, the water, the exposure and the camera each carry dozens of
// settings, which is right for building the piece and wrong for running it:
// in rehearsal there is no time to remember which actor holds which slider.
// The desk gathers the ones that matter on the night - light, colours,
// speeds, how much the sensors move the bubbles, the camera's descent - with
// plain French names, bounded sliders, all animatable in Sequencer.
//
// It owns none of what it drives. It pushes its values into the actors that
// do the work, only when a value changes, so the fine settings left on those
// actors stay theirs. The same values are published to the tablet, which
// builds its tabs from a table the desk serves, and can be saved as named
// presets that survive a relaunch.
//
// The camera stays the director's. The desk works out the rail position that
// frames every occupied seat and writes it where the director's camera
// controller reads its end point, so the keyboard keys keep working; it moves
// the rail itself only when the tablet asks, and steps aside the moment
// anything else moves it.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "VibH2ORegie.generated.h"

class ACameraRig_Rail;
class ALevelSequenceActor;
class ULevelSequencePlayer;
class AExponentialHeightFog;
class APostProcessVolume;
class AVibH2OStageActor;
class UFXSystemComponent;
class FJsonObject;
struct FVibH2OOscMessage;

/** The surface the picture is projected on: it fixes the camera's format and the Spout resolution. */
UENUM(BlueprintType)
enum class EVibH2OOutputFormat : uint8
{
	TulleFace UMETA(DisplayName = "Tulle face - 1920 x 1440 (4:3)"),
	BacFace UMETA(DisplayName = "Bac face - 1920 x 410 (4,68:1)"),
	TulleCote UMETA(DisplayName = "Tulle cour ou jardin - 960 x 1920 (1:2)"),
	Ecran169 UMETA(DisplayName = "Écran 16:9 - 1920 x 1080"),
	Libre UMETA(DisplayName = "Libre (ne touche ni à la caméra ni à Spout)"),
};

/**
 * The settings of the desk. One struct, so a preset is one JSON object and
 * the tablet's list of controls is one table.
 */
USTRUCT(BlueprintType)
struct VIBH2O_API FVibH2ORegieSettings
{
	GENERATED_BODY()

	// ------------------------------------------------------------- Spectacle

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Spectacle", meta = (DisplayName = "Luminosité générale (EV)", ClampMin = "4.0", ClampMax = "16.0", ToolTip = "Exposition de toute l'image, de haut en bas du rail. +1 = deux fois plus clair."))
	float Exposure = 10.0f;

	/** The projection surface: the camera takes its format, Spout its resolution, and the framing follows. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spectacle", meta = (DisplayName = "Format de sortie", ToolTip = "La surface projetée. Fixe le format de la caméra du rail et la résolution envoyée à Resolume par Spout ; le cadrage automatique en tient compte."))
	EVibH2OOutputFormat OutputFormat = EVibH2OOutputFormat::TulleFace;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Spectacle", meta = (DisplayName = "Numéros des spectateurs", ToolTip = "Le numéro de chaque spectateur au centre de sa bulle."))
	bool bShowSeatNumbers = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Spectacle", meta = (DisplayName = "Cadrage : marge", ClampMin = "0.0", ClampMax = "0.8", ToolTip = "Espace laissé autour de la salle à l'arrivée de la caméra. 0 = bord à bord. La fin du rail s'adapte au nombre de spectateurs."))
	float FramingMargin = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Spectacle", meta = (DisplayName = "Descente : durée (s)", ClampMin = "1.0", ClampMax = "300.0", ToolTip = "Durée de la descente lointaine (touche 0)."))
	float DescentSeconds = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Spectacle", meta = (DisplayName = "Descente : s'arrête à", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Position sur le rail (0 = tout en haut, 1 = tout en bas) où finit la descente lointaine."))
	float ApproachPosition = 0.16f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Spectacle", meta = (DisplayName = "Arrivée : durée (s)", ClampMin = "1.0", ClampMax = "120.0", ToolTip = "Durée de l'arrivée sur la salle (touche 1)."))
	float ArrivalSeconds = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Spectacle", meta = (DisplayName = "Particules : sortent du brouillard", ToolTip = "Les particules du rail sont absentes au départ et apparaissent peu à peu pendant la descente."))
	bool bParticlesFadeIn = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Spectacle", meta = (DisplayName = "Particules : apparaissent à", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Position sur le rail où les particules commencent à naître."))
	float ParticlesAppearAt = 0.04f;

	// ------------------------------------------------------------------ Image

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Image", meta = (DisplayName = "Contraste", ClampMin = "0.3", ClampMax = "2.0", ToolTip = "1 = l'image d'origine. Plus haut : noirs plus noirs, blancs plus blancs."))
	float ImageContrast = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Image", meta = (DisplayName = "Saturation (couleurs vives)", ClampMin = "0.0", ClampMax = "2.0", ToolTip = "0 = noir et blanc, 1 = l'image d'origine, 2 = couleurs très vives."))
	float ImageSaturation = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Image", meta = (DisplayName = "Froid - chaud", ClampMin = "-1.0", ClampMax = "1.0", ToolTip = "0 = l'image d'origine. Négatif : plus bleu. Positif : plus orangé."))
	float ImageWarmth = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Image", meta = (DisplayName = "Halo lumineux (bloom)", ClampMin = "0.0", ClampMax = "8.0", ToolTip = "Le halo autour de ce qui brille : les bulles, les particules."))
	float ImageBloom = 0.675f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Image", meta = (DisplayName = "Bords assombris (vignettage)", ClampMin = "0.0", ClampMax = "1.0"))
	float ImageVignette = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Image", meta = (DisplayName = "Grain", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Grain de pellicule. 0 = image lisse."))
	float ImageGrain = 0.0f;

	// -------------------------------------------------------------- Tableau 2

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Tableau 2", meta = (DisplayName = "La caméra garde tout le banc", ToolTip = "En tableau 2, la caméra recule ou avance doucement et glisse de côté pour que tout le banc reste au milieu de l'image. Elle revient exactement à sa place au retour en tableau 1."))
	bool bFollowSchool = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Tableau 2", meta = (DisplayName = "Transition salle → banc : durée (s)", ClampMin = "1", ClampMax = "60", ToolTip = "Temps du passage entre les deux tableaux quand on le commande."))
	float TransitionSeconds = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Tableau 2", meta = (DisplayName = "Bulles filles par spectateur", ClampMin = "0", ClampMax = "8", ToolTip = "Chaque bulle se divise en ce nombre de sœurs au début de la transition. 0 = pas de division."))
	float Daughters = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Tableau 2", meta = (DisplayName = "Écart entre les sœurs", ClampMin = "0", ClampMax = "150", ToolTip = "Distance entre une bulle et ses filles pendant la division."))
	float SisterSpread = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Tableau 2", meta = (DisplayName = "Vortex : hauteur", ClampMin = "100", ClampMax = "5000", ToolTip = "Hauteur du cône que forme le banc."))
	float VortexHeight = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Tableau 2", meta = (DisplayName = "Vortex : largeur en bas", ClampMin = "0", ClampMax = "3000", ToolTip = "Rayon du cône à sa base."))
	float VortexBaseRadius = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Tableau 2", meta = (DisplayName = "Vortex : largeur en haut", ClampMin = "0", ClampMax = "3000", ToolTip = "Rayon du cône en haut : plus grand qu'en bas, il s'évase."))
	float VortexTopRadius = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Tableau 2", meta = (DisplayName = "Vortex : nombre de tours", ClampMin = "0", ClampMax = "10", ToolTip = "Combien de fois la spirale fait le tour de bas en haut."))
	float VortexTurns = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Tableau 2", meta = (DisplayName = "Vortex : vitesse de rotation", ClampMin = "-1", ClampMax = "1", ToolTip = "Vitesse de rotation du vortex. Négatif : dans l'autre sens."))
	float VortexSpin = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Tableau 2", meta = (DisplayName = "Banc : vitesse lente", ClampMin = "1", ClampMax = "1000", ToolTip = "Vitesse des poissons les plus calmes."))
	float SchoolMinSpeed = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Tableau 2", meta = (DisplayName = "Banc : vitesse rapide", ClampMin = "1", ClampMax = "1500", ToolTip = "Vitesse des plus excités."))
	float SchoolMaxSpeed = 420.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Tableau 2", meta = (DisplayName = "Banc : force du tourbillon", ClampMin = "0", ClampMax = "6", ToolTip = "Combien le banc est entraîné dans la rotation du vortex."))
	float SchoolSwirl = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Tableau 2", meta = (DisplayName = "Banc : nager ensemble", ClampMin = "0", ClampMax = "6", ToolTip = "Plus haut : les bulles prennent la même direction que leurs voisines."))
	float SchoolAlignment = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Tableau 2", meta = (DisplayName = "Banc : se resserrer", ClampMin = "0", ClampMax = "1", ToolTip = "Plus haut : le banc reste groupé."))
	float SchoolCohesion = 0.06f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Tableau 2", meta = (DisplayName = "Synchronie : vortex au-dessus de", ClampMin = "0", ClampMax = "1", ToolTip = "Au-dessus de ce niveau de synchronie de la salle, le banc tourne en vortex."))
	float SyncVortexAbove = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Tableau 2", meta = (DisplayName = "Synchronie : dispersion sous", ClampMin = "0", ClampMax = "1", ToolTip = "Sous ce niveau, le banc s'éparpille en familles."))
	float SyncDisperseBelow = 0.35f;

	// --------------------------------------------------------------- Ambiance

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Ambiance", meta = (DisplayName = "Calme : couleur des plus calmes", HideAlphaChannel))
	FLinearColor CalmColorLow = FLinearColor(0.02f, 0.10f, 0.45f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Ambiance", meta = (DisplayName = "Calme : couleur des plus agités", HideAlphaChannel))
	FLinearColor CalmColorHigh = FLinearColor(0.05f, 0.30f, 0.60f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Ambiance", meta = (DisplayName = "Normal : couleur des plus calmes", HideAlphaChannel))
	FLinearColor NormalColorLow = FLinearColor(0.02f, 0.30f, 0.38f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Ambiance", meta = (DisplayName = "Normal : couleur des plus agités", HideAlphaChannel))
	FLinearColor NormalColorHigh = FLinearColor(0.10f, 0.62f, 0.55f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Ambiance", meta = (DisplayName = "Excité : couleur des plus calmes", HideAlphaChannel))
	FLinearColor ExcitedColorLow = FLinearColor(0.80f, 0.40f, 0.05f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Ambiance", meta = (DisplayName = "Excité : couleur des plus agités", HideAlphaChannel))
	FLinearColor ExcitedColorHigh = FLinearColor(0.95f, 0.15f, 0.10f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Ambiance", meta = (DisplayName = "Lumière des bulles", ClampMin = "0.0", ClampMax = "10.0", ToolTip = "À monter si les bulles se fondent dans l'eau, à baisser si elles brûlent en blanc."))
	float BubbleBrightness = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Ambiance", meta = (DisplayName = "Couleur de l'eau", HideAlphaChannel))
	FLinearColor WaterColor = FLinearColor(0.07f, 0.47f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Ambiance", meta = (DisplayName = "Lumière de l'eau", ClampMin = "0.0", ClampMax = "20.0", ToolTip = "Clarté de l'eau elle-même, derrière les bulles."))
	float WaterLight = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Ambiance", meta = (DisplayName = "Eau trouble", ClampMin = "0.0", ClampMax = "10.0", ToolTip = "Densité de l'eau. Plus trouble : les bulles apparaissent plus tard dans la descente et se voient moins de loin."))
	float WaterDensity = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Ambiance", meta = (DisplayName = "Bascule d'ambiance : durée (s)", ClampMin = "0.0", ClampMax = "30.0"))
	float MoodTransitionSeconds = 4.0f;

	// ------------------------------------------------------------------ Salle

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Salle", meta = (DisplayName = "Courbure de la salle (° par siège)", ClampMin = "-15.0", ClampMax = "15.0", ToolTip = "0 = salle droite, en grille. Positif ou négatif : les rangées s'arrondissent d'un côté ou de l'autre."))
	float RoomCurvature = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Salle", meta = (DisplayName = "Rangées du fond en éventail", ToolTip = "Salle courbée : les rangées du fond s'ouvrent, comme dans un théâtre. Sans effet sur une salle droite."))
	bool bRoomFanOut = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Salle", meta = (DisplayName = "Gradins : hauteur par rangée (cm)", ClampMin = "0.0", ClampMax = "150.0", ToolTip = "Chaque rangée monte d'autant, comme des gradins. 0 = salle à plat."))
	float RoomTiers = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Salle", meta = (DisplayName = "Relief (cm)", ClampMin = "0.0", ClampMax = "100.0", ToolTip = "Petites variations de hauteur pour que la salle ne soit pas parfaitement plate. 0 = plat."))
	float RoomRelief = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Salle", meta = (DisplayName = "Écart entre sièges (cm)", ClampMin = "20.0", ClampMax = "500.0", ToolTip = "Distance entre deux sièges voisins d'une rangée : la largeur de la salle. La caméra recadre seule."))
	float SeatSpacing = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Salle", meta = (DisplayName = "Écart entre rangées (cm)", ClampMin = "20.0", ClampMax = "500.0", ToolTip = "Distance entre deux rangées : la profondeur de la salle. La caméra recadre seule."))
	float RowSpacing = 140.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Salle", meta = (DisplayName = "Allées : largeur (× un siège)", ClampMin = "0.0", ClampMax = "6.0", ToolTip = "Largeur d'une allée (colonne que le plan laisse vide), en sièges. 1 = comme un siège, 0 = l'allée se referme."))
	float AisleWidth = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Spectacle", meta = (DisplayName = "Numéros : durée du fondu (s)", ClampMin = "0.0", ClampMax = "5.0", ToolTip = "Les numéros apparaissent et disparaissent en fondu sur cette durée. 0 = d'un coup."))
	float SeatNumberFade = 1.0f;

	// ------------------------------------------------------------ Lumières

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Lumières", meta = (DisplayName = "Niveau général", ClampMin = "0.0", ClampMax = "4.0", ToolTip = "Multiplie les trois lumières des bulles. 0 = éteint."))
	float LightLevel = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Lumières", meta = (DisplayName = "Face : intensité (lux)", ClampMin = "0.0", ClampMax = "20.0"))
	float KeyLight = 2.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Lumières", meta = (DisplayName = "Face : couleur"))
	FLinearColor KeyColor = FLinearColor(1.0f, 0.93f, 0.82f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Lumières", meta = (DisplayName = "Face : direction (°)", ClampMin = "-180.0", ClampMax = "180.0"))
	float KeyAzimuth = -30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Lumières", meta = (DisplayName = "Face : hauteur (°)", ClampMin = "-30.0", ClampMax = "89.0"))
	float KeyHeight = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Lumières", meta = (DisplayName = "Contre-jour : intensité (lux)", ClampMin = "0.0", ClampMax = "20.0"))
	float BackLight = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Lumières", meta = (DisplayName = "Contre-jour : couleur"))
	FLinearColor BackColor = FLinearColor(0.45f, 0.85f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Lumières", meta = (DisplayName = "Débouchage : intensité (lux)", ClampMin = "0.0", ClampMax = "20.0"))
	float FillLight = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Lumières", meta = (DisplayName = "Débouchage : couleur"))
	FLinearColor FillColor = FLinearColor(0.25f, 0.45f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Lumières", meta = (DisplayName = "Faisceaux dans l'eau", ClampMin = "0.0", ClampMax = "4.0", ToolTip = "Combien on voit les faisceaux des trois lumières dans l'eau."))
	float LightBeams = 0.3f;

	// -------------------------------------------------------------- Cadrage

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Spectacle", meta = (DisplayName = "Cadrage : plus près (m)", ClampMin = "-150.0", ClampMax = "150.0", ToolTip = "Décale la vue d'ensemble calculée : positif = la caméra s'arrête plus près des bulles, négatif = plus loin. 0 = le calcul automatique."))
	float FramingCloser = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Spectacle", meta = (DisplayName = "Cadrage : décalage à droite (m)", ClampMin = "-30.0", ClampMax = "30.0", ToolTip = "Fait glisser l'image : positif = la caméra va vers la droite de l'image, la salle part à gauche. Visible en Play."))
	float FramingRight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Spectacle", meta = (DisplayName = "Cadrage : décalage vers le haut (m)", ClampMin = "-30.0", ClampMax = "30.0", ToolTip = "Fait glisser l'image : positif = la caméra va vers le haut de l'image, la salle descend. Visible en Play."))
	float FramingUp = 0.0f;

	// ----------------------------------------------------------------- Bulles

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Bulles", meta = (DisplayName = "Forme irrégulière", ClampMin = "0.0", ClampMax = "0.4", ToolTip = "Chaque bulle a sa propre forme, un peu aplatie ou allongée. 0 = sphères parfaites ; 0,08 = discret ; 0,2 = très marqué."))
	float ShapeIrregularity = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Bulles", meta = (DisplayName = "Taille des bulles", ClampMin = "0.1", ClampMax = "4.0"))
	float BubbleSize = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Bulles", meta = (DisplayName = "Flottement : ampleur", ClampMin = "0.0", ClampMax = "200.0", ToolTip = "De combien les bulles bougent autour de leur place, en cm."))
	float FloatAmplitude = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Bulles", meta = (DisplayName = "Flottement : vitesse", ClampMin = "0.0", ClampMax = "5.0"))
	float FloatSpeed = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Bulles", meta = (DisplayName = "Battement : gonflement", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Combien la bulle gonfle à chaque battement de cœur."))
	float BeatPop = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Bulles", meta = (DisplayName = "Déformation organique", ClampMin = "0.0", ClampMax = "1.0"))
	float OrganicDeform = 0.1f;

	// ------------------------------------------------- Réaction aux capteurs

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Réaction", meta = (DisplayName = "Excitation : grossissement", ClampMin = "0.0", ClampMax = "3.0", ToolTip = "Combien une bulle grossit quand son spectateur s'excite."))
	float ScalePerExcitation = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Réaction", meta = (DisplayName = "Excitation : dérive", ClampMin = "0.0", ClampMax = "500.0", ToolTip = "De combien une bulle très excitée s'éloigne de sa place, en cm."))
	float DriftIntensity = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Réaction", meta = (DisplayName = "Contraste entre spectateurs", ClampMin = "0.0", ClampMax = "6.0", ToolTip = "0 : toutes les bulles de la même couleur. Plus haut : chacun prend sa couleur selon son écart à la salle."))
	float IndividualContrast = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Réaction", meta = (DisplayName = "Dérive : à partir de l'excitation", ClampMin = "0.0", ClampMax = "0.99", ToolTip = "Sous ce niveau d'excitation de la personne, sa bulle reste à sa place. Au-dessus, elle s'écarte de plus en plus."))
	float DriftThreshold = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Réaction", meta = (DisplayName = "Dérive : limite (part de l'écart)", ClampMin = "0.0", ClampMax = "0.5", ToolTip = "La dérive ne dépasse jamais cette part de l'écart entre sièges : à 0,35 et 120 cm, 42 cm au plus, quelle que soit l'amplitude."))
	float DriftLimit = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Réaction", meta = (DisplayName = "Seuil calme", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Sous ce niveau d'excitation de la salle : ambiance calme."))
	float CalmThreshold = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Réaction", meta = (DisplayName = "Seuil excité", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Au-dessus de ce niveau : ambiance excitée."))
	float ExcitedThreshold = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Réaction", meta = (DisplayName = "Synchronie : couleurs qui se rejoignent", ClampMin = "0.0", ClampMax = "1.0"))
	float SyncHueTightening = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Réaction", meta = (DisplayName = "Synchronie : halo", ClampMin = "0.0", ClampMax = "3.0"))
	float SyncHalo = 1.0f;
};

/** One control as the tablet shows it. Kept outside the editor-only metadata: a packaged game has none. */
struct VIBH2O_API FVibH2ORegieControl
{
	const TCHAR* Id;
	const TCHAR* Label;
	const TCHAR* Tab;
	float Min;
	float Max;
	float Step;
	/** One line for the operator: what the setting changes on screen. */
	const TCHAR* Help = nullptr;
};

UENUM(BlueprintType)
enum class EVibH2ORegieCameraMove : uint8
{
	None UMETA(DisplayName = "Immobile"),
	Descent UMETA(DisplayName = "Descente"),
	Arrival UMETA(DisplayName = "Arrivée"),
	Glide UMETA(DisplayName = "Déplacement"),
};

UCLASS(meta = (DisplayName = "VibH2O — Régie"),
	HideCategories = (Actor, Rendering, Replication, Collision, HLOD, Input, LOD, Cooking, Physics, Networking, DataLayers, LevelInstance, WorldPartition),
	PrioritizeCategories = ("VibH2O"),
	AutoExpandCategories = ("VibH2O|Réglages"))
class VIBH2O_API AVibH2ORegie : public AActor
{
	GENERATED_BODY()

public:
	AVibH2ORegie();

	/** What the desk sets. Change it here, from the tablet, or in Sequencer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Réglages", meta = (ShowOnlyInnerProperties))
	FVibH2ORegieSettings Reglages;

	// ---------------------------------------------------------- Connections

	/** Empty: the first stage of the level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Branchements", meta = (DisplayName = "Scène (vide = la première)"))
	TObjectPtr<AVibH2OStageActor> Stage;

	/** Empty: the volume named VibH2O_Exposition, else the first bounded one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Branchements", meta = (DisplayName = "Volume d'exposition (vide = VibH2O_Exposition)"))
	TObjectPtr<APostProcessVolume> ExposureVolume;

	/** Empty: the first visible height fog of the level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Branchements", meta = (DisplayName = "Eau (brouillard, vide = le premier)"))
	TObjectPtr<AExponentialHeightFog> Water;

	/** Empty: the first light rig of the level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Branchements", meta = (DisplayName = "Lumières (vide = les premières)"))
	TObjectPtr<class AVibH2OLightRig> Lights;

	/** Empty: the first camera rail of the level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Branchements", meta = (DisplayName = "Rail de la caméra (vide = le premier)"))
	TObjectPtr<ACameraRig_Rail> Rail;

	/**
	 * The director's camera controller, whose end points the desk keeps up to
	 * date. Empty: the first actor whose class name contains the text below.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Branchements", meta = (DisplayName = "Contrôleur de caméra (vide = Camera_Controller)"))
	TObjectPtr<AActor> CameraController;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Branchements", meta = (DisplayName = "Contrôleur : nom de classe cherché"))
	FString CameraControllerClassName = TEXT("Camera_Controller");

	/** Write the framed end point and the durations into the camera controller, so the keys follow. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Branchements", meta = (DisplayName = "Mettre à jour le contrôleur de caméra"))
	bool bDriveCameraController = true;

	/**
	 * Particle systems whose effect, class or component name contains this
	 * text appear through the fog. The effect's own name - not the actor's
	 * label, which a packaged game does not have.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VibH2O|Branchements", meta = (DisplayName = "Particules : nom cherché"))
	FString ParticleNameFilter = TEXT("Dirt");

	// ------------------------------------------------------------- Conduite

	/**
	 * The show's Level Sequence. Its named markers are the steps of the cue
	 * list: GO plays up to the next marker and stops there. Empty: the first
	 * Level Sequence of the level that has markers.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Conduite", meta = (DisplayName = "Séquence du spectacle", ToolTip = "La Level Sequence du spectacle. Chaque marqueur nommé est une étape : GO joue jusqu'au marqueur suivant puis s'arrête. Vide = la première séquence de la carte qui a des marqueurs."))
	TObjectPtr<ALevelSequenceActor> ShowSequence;

	/**
	 * In Play, the keyboard drives the cue list too: Space or Page Down is
	 * GO, Page Up goes back a step. A presentation remote sends exactly
	 * these keys.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibH2O|Conduite", meta = (DisplayName = "Clavier : Espace = GO", ToolTip = "En Play : Espace ou Page suivante = GO, Page précédente = étape précédente. Une télécommande de présentation envoie justement ces touches."))
	bool bConduiteKeys = true;

	/**
	 * The camera's way down the rail as one animatable value, so a Level
	 * Sequence can carry the descent: 0 the top of the rail, 1 the approach,
	 * 2 the overall view of the room - wherever the seat count puts it.
	 * Negative: unused. Written to the rail only when it changes, so the
	 * controller's keys and the desk keep working in between.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Conduite", meta = (DisplayName = "Caméra : parcours (séquence)", UIMin = "-1.0", UIMax = "2.0", ToolTip = "Pour piloter la descente depuis une séquence : 0 = haut du rail, 1 = approche, 2 = vue d'ensemble de la salle (qui suit le nombre de spectateurs). -1 = non utilisé. N'agit que quand la valeur change."))
	float CameraPath = -1.0f;

	/**
	 * The tableau as one animatable value for a Level Sequence: 0 the room,
	 * 1 the school. Negative: unused. Like the camera path it acts only when
	 * it changes, so a sequence stopped on a step leaves the tablet, the cues
	 * and Max free to move the tableau.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "VibH2O|Conduite", meta = (DisplayName = "Tableau (séquence)", UIMin = "-1.0", UIMax = "1.0", ToolTip = "Pour passer d'un tableau à l'autre depuis une séquence : 0 = salle, 1 = banc. -1 = non utilisé. N'agit que quand la valeur change : une séquence arrêtée laisse la main à la tablette."))
	float TableauPath = -1.0f;

	/** Plays from where the show stands to the next step, and stops there. */
	UFUNCTION(BlueprintCallable, Category = "VibH2O|Conduite")
	void ConduiteGo();

	/** Back to the step before, stopped. */
	UFUNCTION(BlueprintCallable, Category = "VibH2O|Conduite")
	void ConduitePrevious();

	/** On to the next step without playing what is in between, stopped. */
	UFUNCTION(BlueprintCallable, Category = "VibH2O|Conduite")
	void ConduiteNext();

	/** To a step by its number (0 = the start), stopped. */
	UFUNCTION(BlueprintCallable, Category = "VibH2O|Conduite")
	void ConduiteGoTo(int32 Step);

	/** Pauses or resumes the show's sequence. */
	UFUNCTION(BlueprintCallable, Category = "VibH2O|Conduite")
	void ConduitePause();

	/** The steps' names, in order: "Début", then every marker. */
	UFUNCTION(BlueprintPure, Category = "VibH2O|Conduite")
	TArray<FString> GetConduiteSteps() const;

	/** The step the show stands at or has last passed. */
	UFUNCTION(BlueprintPure, Category = "VibH2O|Conduite")
	int32 GetConduiteStep() const;

	// ------------------------------------------------------------- Readouts

	/** Rail position where the whole room is framed, worked out from the occupied seats. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "VibH2O|Diagnostic", meta = (DisplayName = "Cadrage : position calculée"))
	float FramedRailPosition = -1.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "VibH2O|Diagnostic", meta = (DisplayName = "Caméra : mouvement"))
	EVibH2ORegieCameraMove CameraMove = EVibH2ORegieCameraMove::None;

	// -------------------------------------------------------------- Actions

	/** Pixel size of a format; zero for Libre. */
	static FIntPoint GetOutputSize(EVibH2OOutputFormat Format);

	/** Applies every setting at once, as if they had all just changed. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "VibH2O", meta = (DisplayName = "Appliquer tout"))
	void ApplyAll();

	/** Reads the current values of the driven actors into the desk, so placing it changes nothing. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "VibH2O", meta = (DisplayName = "Lire l'état actuel de la scène"))
	void CaptureFromScene();

	/** Takes the image as it looks now as the new "original image" the reset returns to. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "VibH2O", meta = (DisplayName = "Prendre l'image actuelle comme image d'origine"))
	void RecaptureImage();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "VibH2O", meta = (DisplayName = "Recalculer le cadrage"))
	void RefreshFraming();

	/** Puts contrast, saturation, warmth, bloom, vignette and grain back to how the scene looked when the desk was placed. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "VibH2O", meta = (DisplayName = "Revenir à l'image d'origine"))
	void ResetImage();

	UFUNCTION(BlueprintCallable, Category = "VibH2O|Caméra")
	void StartDescent();

	UFUNCTION(BlueprintCallable, Category = "VibH2O|Caméra")
	void StartArrival();

	UFUNCTION(BlueprintCallable, Category = "VibH2O|Caméra")
	void StopCamera();

	/** Moves the rail to a position, in Seconds (0 = at once). */
	UFUNCTION(BlueprintCallable, Category = "VibH2O|Caméra")
	void GlideCameraTo(float RailPosition, float Seconds);

	// -------------------------------------------------------------- Presets

	UFUNCTION(BlueprintCallable, Category = "VibH2O|Préréglages")
	bool SavePreset(const FString& Name);

	UFUNCTION(BlueprintCallable, Category = "VibH2O|Préréglages")
	bool LoadPreset(const FString& Name);

	UFUNCTION(BlueprintCallable, Category = "VibH2O|Préréglages")
	bool DeletePreset(const FString& Name);

	UFUNCTION(BlueprintPure, Category = "VibH2O|Préréglages")
	TArray<FString> ListPresets() const;

	/** Folder the presets live in: Saved/VibH2O/Regie of the project. */
	static FString GetPresetFolder();

	// ------------------------------------------------------------- Tablet

	/** The controls the tablet draws, in order. */
	static TConstArrayView<FVibH2ORegieControl> GetControls();

	/** Handles /Regie/... from the tablet or OSC. */
	void HandleRegieMessage(const FVibH2OOscMessage& Message);

	/** Sets one setting by its name, from a number or a #RRGGBB colour. */
	bool SetValueFromText(const FString& Id, const FString& Value);

	/** Adds "regie" to the tablet's state. */
	void WriteRemoteState(const TSharedRef<FJsonObject>& Root) const;

	// ------------------------------------------------------ Framing (pure)

	/**
	 * Whether a camera standing at CameraLocation, looking along Forward,
	 * sees every corner of the room with the margin around it.
	 *
	 * @param HorizontalFOV In degrees.
	 */
	static bool IsFramed(TConstArrayView<FVector> Corners, const FVector& CameraLocation, const FVector& Forward, const FVector& Right,
		const FVector& Up, float HorizontalFOV, float AspectRatio, float Margin);

	// ---------------------------------------------------------------- Actor

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	void ResolveConnections();
	/** Resolves again only when something is still missing, at most every few seconds. */
	void ResolveMissingConnections();
	void ApplyChanged(bool bForce);
	/** Reads the driven actors into the settings, without touching the image or the output format. */
	void ReadScene();
	/** True when this setting is to be pushed in the current apply. */
	bool Changed(const TCHAR* Name) const;
	/** A line for the tablet: what just happened, or why it did not. */
	void Notice(const FString& Text);
	bool IsEditorWorld() const;
	void ApplyToStage();
	void ApplyToExposure();
	void ApplyToWater();
	void ApplyToLights();
	/** Stage state kept with a preset beyond the desk's settings: the focus groups, the sync shape. */
	void WritePresetExtras(TSharedRef<class FJsonObject> Json) const;
	void ReadPresetExtras(const TSharedRef<class FJsonObject>& Json);
	/** Creates or replaces a focus group made of these seats. */
	void SetFocusGroupSeats(const FString& GroupName, const TArray<int32>& SeatIndices);
	void ApplyToCameraController();
	void ApplyToOutput();
	void ApplyToImage();
	void CaptureImage();
	void ApplyToTableau2();
	void CaptureTableau2();
	class UCameraComponent* FindRailCamera() const;
	void TickCamera(float DeltaSeconds);
	void TickParticles();
	void GatherParticles();
	float GetRailPosition() const;
	void SetRailPosition(float Position);
	/**
	 * Shift is the camera's sideways offset (along its right and up axes)
	 * to frame with, in place of the one it has now.
	 */
	bool ComputeFramedRailPosition(float& OutPosition, bool bForSchool = false, FVector2D Shift = FVector2D::ZeroVector) const;
	/** Moves the rail camera sideways, parallel to the picture, by CameraShift. */
	void ApplyCameraShift();
	/** The director's sideways framing offset, in cm along the camera's right and up. */
	FVector2D GetManualShift() const;
	/** The framed rail position moved by the director's "closer" offset. */
	float ApplyFramingCloser(float Position) const;
	void TickFollowSchool(float DeltaSeconds);
	void TickCameraPath();
	void TickTableauPath();

	struct FConduiteStep
	{
		FString Label;
		/** In the sequence's display rate. */
		FFrameTime Time;
	};
	ULevelSequencePlayer* GetShowPlayer() const;
	TArray<FConduiteStep> GatherConduiteSteps() const;
	void JumpConduite(const TArray<FConduiteStep>& Steps, int32 Index);

	UPROPERTY(Transient)
	FVibH2ORegieSettings Applied;

	/** False until the desk has read the scene once: placing it must not change the look. */
	UPROPERTY()
	bool bCaptured = false;

	/**
	 * The look the scene had before the desk touched it, read through every
	 * post-process volume in priority order: the desk's image sliders work
	 * from it, and "back to the original image" returns to it.
	 */
	UPROPERTY()
	bool bImageCaptured = false;

	/** False until the tableau 2 settings have been read from the stage: a desk placed before they existed reads them once. */
	UPROPERTY()
	bool bTableau2Captured = false;

	UPROPERTY()
	FVector4 BaseContrast = FVector4(1.0, 1.0, 1.0, 1.0);

	UPROPERTY()
	FVector4 BaseSaturation = FVector4(1.0, 1.0, 1.0, 1.0);

	UPROPERTY()
	float BaseWhiteTemp = 6500.0f;

	UPROPERTY()
	float BaseBloom = 0.675f;

	UPROPERTY()
	float BaseVignette = 0.4f;

	UPROPERTY()
	float BaseGrain = 0.0f;

	/** How much brighter the water glows toward the sun than elsewhere, read from the scene. */
	UPROPERTY()
	float WaterDirectionalRatio = 2.3f;

	TArray<FString> PresetNames;
	void RefreshPresetNames();

	bool bHasApplied = false;
	bool bWaterPending = false;

	/** Names of the settings that differ from what was last pushed; empty with bApplyAll. */
	TSet<FName> ChangedNow;
	bool bApplyAll = false;
	double LastResolveTime = -1000.0;
	TWeakObjectPtr<class AVibH2OBackdrop> CachedBackdrop;

	FString NoticeText;
	int32 NoticeSerial = 0;

	// Camera move.
	float MoveFrom = 0.0f;
	float MoveTo = 0.0f;
	float MoveElapsed = 0.0f;
	float MoveDuration = 0.0f;
	float LastWrittenRail = -1.0f;
	/** Last time something other than the desk moved the rail: the desk keeps off for a moment. */
	double ExternalRailMoveTime = -1000.0;
	float PreviousRail = -1.0f;
	bool bFollowingSchool = false;
	float SchoolTarget = -1.0f;
	double LastSchoolFramingTime = 0.0;
	/** CameraPath as last written to the rail; unset until the first tick, which only records it. */
	TOptional<float> AppliedCameraPath;
	TOptional<float> AppliedTableauPath;
	/**
	 * Sideways offset of the rail camera, along its right and up axes: in
	 * tableau 2 it keeps the wandering school in the middle of the picture.
	 * Zero in tableau 1, where the camera sits exactly as placed.
	 */
	FVector2D CameraShift = FVector2D::ZeroVector;
	FVector2D SchoolShiftTarget = FVector2D::ZeroVector;
	TWeakObjectPtr<class UCameraComponent> ShiftedCamera;
	FVector ShiftBaseLocation = FVector::ZeroVector;
	double LastFramingTime = 0.0;

	// Particles.
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<UFXSystemComponent>> Particles;
	int32 ParticlesState = -1;

	FDelegateHandle RegieMessageHandle;
};
