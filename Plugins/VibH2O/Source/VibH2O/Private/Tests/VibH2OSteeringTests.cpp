// Tests for the hand-steered layers: attractor fades, room shares, exclusion
// zones, and the iPad remote's server.
//
// What matters on stage is not that a value reaches 1, but HOW it gets there:
// no jump, no bounce when the operator changes their mind, no bubble crossing a
// performer. Each test below measures one of those, frame by frame.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "Common/TcpSocketBuilder.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Misc/Paths.h"
#include "Remote/VibH2ORemoteServer.h"
#include "SocketSubsystem.h"
#include "Sockets.h"
#include "Steering/VibH2OSteeringMath.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace VibH2OSteeringTestUtils
{
	static constexpr float Frame = 1.0f / 60.0f;
	static constexpr int32 RemoteTestPort = 9188;
}

// ---------------------------------------------------------------------------
// The fade: smooth start, smooth landing, smooth change of mind.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2OSteeringFadeTest,
	"VibH2O.Steering.FonduActivation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FVibH2OSteeringFadeTest::RunTest(const FString& Parameters)
{
	using namespace VibH2OSteeringTestUtils;

	const float Settle = 2.0f;

	// 1. Activation: leaves gently, never overshoots, arrives on time.
	{
		FVibH2OSpring Spring;
		float Previous = 0.0f;
		float PreviousStep = 0.0f;
		float MaxAcceleration = 0.0f;
		bool bMonotonic = true;
		bool bBounded = true;
		float AtSettle = 0.0f;

		for (int32 Index = 1; Index <= 240; ++Index)
		{
			Spring.Step(1.0f, Settle, Frame);
			const float Step = Spring.Value - Previous;
			bMonotonic &= Step >= -1.0e-6f;
			bBounded &= Spring.Value <= 1.0f + 1.0e-6f;
			MaxAcceleration = FMath::Max(MaxAcceleration, FMath::Abs(Step - PreviousStep));
			if (Index == 1)
			{
				// A linear ramp would cover Frame / Settle on its first frame;
				// the spring starts from rest, so well under half of that.
				TestTrue(FString::Printf(TEXT("Le fondu demarre en douceur (premiere image %.5f)"), Spring.Value), Spring.Value < 0.5f * Frame / Settle);
			}
			if (Index == FMath::RoundToInt(Settle / Frame))
			{
				AtSettle = Spring.Value;
			}
			PreviousStep = Step;
			Previous = Spring.Value;
		}

		TestTrue(TEXT("Le fondu ne revient jamais en arriere"), bMonotonic);
		TestTrue(TEXT("Le fondu ne depasse jamais sa cible"), bBounded);
		TestTrue(FString::Printf(TEXT("A la duree reglee, 99 %% du chemin est fait (%.4f)"), AtSettle), AtSettle >= 0.985f && AtSettle <= 1.0f);
		TestEqual(TEXT("Apres le fondu, la valeur est exactement 1"), Spring.Value, 1.0f);
		TestEqual(TEXT("Apres le fondu, la vitesse est exactement nulle"), Spring.Velocity, 0.0f);
		// No frame changes speed abruptly: what the eye reads as "smooth". The
		// spring peaks near 0.0033 per frame at 60 images/s; a linear ramp
		// starting or reversing would show 0.008 to 0.017.
		TestTrue(FString::Printf(TEXT("Aucune secousse : acceleration max %.5f par image"), MaxAcceleration), MaxAcceleration < 0.005f);
	}

	// 2. Change of mind half way: the velocity turns round, it does not jump.
	{
		FVibH2OSpring Spring;
		float Previous = 0.0f;
		float PreviousStep = 0.0f;
		float MaxAcceleration = 0.0f;
		float MaxStep = 0.0f;

		for (int32 Index = 1; Index <= 480; ++Index)
		{
			const float Target = Index <= 60 ? 1.0f : 0.0f;
			Spring.Step(Target, Settle, Frame);
			const float Step = Spring.Value - Previous;
			if (Index > 1)
			{
				MaxAcceleration = FMath::Max(MaxAcceleration, FMath::Abs(Step - PreviousStep));
			}
			MaxStep = FMath::Max(MaxStep, FMath::Abs(Step));
			PreviousStep = Step;
			Previous = Spring.Value;
		}

		TestTrue(FString::Printf(TEXT("Contre-ordre a mi-course : pas de saut (pas max %.4f)"), MaxStep), MaxStep < 0.03f);
		TestTrue(FString::Printf(TEXT("Contre-ordre a mi-course : pas de rebond (acceleration max %.5f)"), MaxAcceleration), MaxAcceleration < 0.005f);
		TestEqual(TEXT("Contre-ordre : retour exact a 0"), Spring.Value, 0.0f);
	}

	// 3. Frame rate: the same fade at 30 and at 144 images per second.
	{
		FVibH2OSpring Slow;
		FVibH2OSpring Fast;
		for (int32 Index = 0; Index < 30; ++Index)
		{
			Slow.Step(1.0f, Settle, 1.0f / 30.0f);
		}
		for (int32 Index = 0; Index < 144; ++Index)
		{
			Fast.Step(1.0f, Settle, 1.0f / 144.0f);
		}
		TestTrue(FString::Printf(TEXT("Independant des images par seconde (%.4f contre %.4f)"), Slow.Value, Fast.Value),
			FMath::IsNearlyEqual(Slow.Value, Fast.Value, 1.0e-3f));

		// A half-second hitch - a shader compile - must not overshoot either.
		FVibH2OSpring Hitch;
		Hitch.Step(1.0f, Settle, 0.5f);
		Hitch.Step(1.0f, Settle, 0.5f);
		TestTrue(TEXT("Un a-coup de 0,5 s ne fait pas depasser la cible"), Hitch.Value <= 1.0f && Hitch.Value > 0.5f);
	}

	// 4. Staggered arrivals: a flow, and the right average.
	{
		int32 Arrived = 0;
		int32 Waiting = 0;
		double DurationSum = 0.0;
		const int32 Count = 200;
		for (int32 Seed = 1; Seed <= Count; ++Seed)
		{
			const float Duration = FVibH2OSteeringMath::StaggeredDuration(4.0f, Seed, 0.6f);
			DurationSum += Duration;
			FVibH2OSpring Spring;
			for (int32 Index = 0; Index < 240; ++Index)
			{
				Spring.Step(1.0f, Duration, Frame);
			}
			// After 4 s, the quick ones have landed, the slow ones not yet.
			Arrived += Spring.Value >= 0.99f ? 1 : 0;
			Waiting += Spring.Value < 0.98f ? 1 : 0;
		}
		TestTrue(FString::Printf(TEXT("Etalement : les premieres sont arrivees (%d)"), Arrived), Arrived > Count / 5);
		TestTrue(FString::Printf(TEXT("Etalement : les dernieres sont en route (%d)"), Waiting), Waiting > Count / 10);
		TestTrue(TEXT("Etalement : la duree moyenne reste celle reglee"), FMath::IsNearlyEqual(DurationSum / Count, 4.0, 0.15));
		TestEqual(TEXT("Etalement nul : tout le monde ensemble"), FVibH2OSteeringMath::StaggeredDuration(4.0f, 17, 0.0f), 4.0f);
	}

	// 5. Several attractors: stacking never jumps.
	{
		const FVector Base(0.0, 0.0, 0.0);
		const FVector TargetA(1000.0, 0.0, 0.0);
		const FVector TargetB(0.0, 1000.0, 0.0);

		const FVector Half = FVibH2OSteeringMath::BlendTowardTargets(Base, { TargetA }, { 0.5f });
		TestTrue(TEXT("Un attracteur a mi-course : mi-chemin"), Half.Equals(FVector(500.0, 0.0, 0.0), 0.01));

		const FVector BelowOne = FVibH2OSteeringMath::BlendTowardTargets(Base, { TargetA, TargetB }, { 0.5f, 0.4999f });
		const FVector AboveOne = FVibH2OSteeringMath::BlendTowardTargets(Base, { TargetA, TargetB }, { 0.5f, 0.5001f });
		TestTrue(TEXT("Deux attracteurs : continu au passage d'un poids total de 1"), BelowOne.Equals(AboveOne, 0.2));

		const FVector Both = FVibH2OSteeringMath::BlendTowardTargets(Base, { TargetA, TargetB }, { 1.0f, 1.0f });
		TestTrue(TEXT("Deux attracteurs pleins : la bulle partage entre les deux"), Both.Equals(FVector(500.0, 500.0, 0.0), 0.01));

		const FVector None = FVibH2OSteeringMath::BlendTowardTargets(Base, { TargetA }, { 0.0f });
		TestTrue(TEXT("Attracteur relache : retour exact a la place d'origine"), None.Equals(Base, 0.0));
	}

	return true;
}

// ---------------------------------------------------------------------------
// The share of the room: stable, and splittable without overlap.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2OSteeringShareTest,
	"VibH2O.Steering.PartDeLaSalle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FVibH2OSteeringShareTest::RunTest(const FString& Parameters)
{
	const int32 Count = 2000;
	int32 InThird = 0;
	int32 Overlap = 0;
	int32 Nobody = 0;

	for (int32 Seed = 1; Seed <= Count; ++Seed)
	{
		const float Hash = FVibH2OSteeringMath::SelectionHash(Seed);
		TestEqual(TEXT("Le tirage d'une bulle est stable"), FVibH2OSteeringMath::SelectionHash(Seed), Hash);

		InThird += FVibH2OSteeringMath::IsInShare(Hash, 0.0f, 0.3f) ? 1 : 0;

		// Two attractors, back to back on the circle, wrapping round 1.
		const bool bLeft = FVibH2OSteeringMath::IsInShare(Hash, 0.8f, 0.5f);
		const bool bRight = FVibH2OSteeringMath::IsInShare(Hash, 0.3f, 0.5f);
		Overlap += (bLeft && bRight) ? 1 : 0;
		Nobody += (!bLeft && !bRight) ? 1 : 0;
	}

	const float Ratio = static_cast<float>(InThird) / Count;
	TestTrue(FString::Printf(TEXT("Une part de 30 %% appelle ~30 %% de la salle (%.1f %%)"), Ratio * 100.0f), FMath::Abs(Ratio - 0.3f) < 0.04f);
	TestEqual(TEXT("Deux parts adjacentes ne se recouvrent pas"), Overlap, 0);
	TestEqual(TEXT("Deux parts adjacentes de 50 % couvrent toute la salle"), Nobody, 0);
	TestTrue(TEXT("Part de 100 % : tout le monde"), FVibH2OSteeringMath::IsInShare(0.999f, 0.5f, 1.0f));
	TestFalse(TEXT("Part nulle : personne"), FVibH2OSteeringMath::IsInShare(0.0f, 0.0f, 0.0f));

	// The cloud fills its volume, and stays inside it while breathing.
	float MaxRadius = 0.0f;
	double MeanRadius = 0.0;
	for (int32 Seed = 1; Seed <= Count; ++Seed)
	{
		for (float Time = 0.0f; Time < 20.0f; Time += 2.3f)
		{
			const float Radius = FVibH2OSteeringMath::CloudPoint(Seed, Time, 0.12f, 0.07f).Size();
			MaxRadius = FMath::Max(MaxRadius, Radius);
		}
		MeanRadius += FVibH2OSteeringMath::CloudPoint(Seed, 0.0f, 0.0f, 0.0f).Size();
	}
	MeanRadius /= Count;
	TestTrue(FString::Printf(TEXT("Le nuage reste dans sa sphere en respirant (%.3f)"), MaxRadius), MaxRadius <= 1.0f + 1.0e-4f);
	// Uniform in volume: the mean radius of a uniform ball is 3/4.
	TestTrue(FString::Printf(TEXT("Le nuage remplit son volume, sans paquet au centre (rayon moyen %.3f)"), MeanRadius), FMath::Abs(MeanRadius - 0.75) < 0.04);

	return true;
}

// ---------------------------------------------------------------------------
// Exclusion: nobody inside, nobody jumping, nobody disturbed far away.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2OSteeringExclusionTest,
	"VibH2O.Steering.ProjectionHorsZone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FVibH2OSteeringExclusionTest::RunTest(const FString& Parameters)
{
	using namespace VibH2OSteeringTestUtils;

	for (const EVibH2OZoneShapeKind Shape : { EVibH2OZoneShapeKind::Ellipse, EVibH2OZoneShapeKind::Rectangle })
	{
		const TCHAR* ShapeName = (Shape == EVibH2OZoneShapeKind::Ellipse) ? TEXT("ellipse") : TEXT("rectangle");

		FVibH2OZonePushParams Params;
		Params.Shape = Shape;
		Params.HalfSize = FVector2D(90.0, 130.0);
		Params.Margin = 60.0f;
		Params.GlideRadiansPerSecond = 2.6f;
		Params.Weight = 1.0f;

		// 1. A whole grid of points: none is left inside, the centre included.
		int32 Inside = 0;
		int32 Moved = 0;
		int32 Checked = 0;
		for (double Y = -400.0; Y <= 400.0; Y += 10.0)
		{
			for (double Z = -400.0; Z <= 400.0; Z += 10.0)
			{
				FVibH2OZoneBubbleState State;
				const FVector2D Point(Y, Z);
				const FVector2D Out = FVibH2OSteeringMath::PushOutOfZone(Point, Params, State, Checked, Frame);
				Inside += FVibH2OSteeringMath::ShapeNorm(Out, Params.HalfSize, Shape) < 1.0f ? 1 : 0;

				const bool bFar = FVibH2OSteeringMath::ShapeNorm(Point, Params.HalfSize, Shape) - 1.0f
					>= Params.Margin / static_cast<float>(FMath::Min(Params.HalfSize.X, Params.HalfSize.Y));
				if (bFar && !Out.Equals(Point, 0.0))
				{
					++Moved;
				}
				++Checked;
			}
		}
		TestEqual(FString::Printf(TEXT("%s : aucune bulle ne reste dans la zone (%d points)"), ShapeName, Checked), Inside, 0);
		TestEqual(FString::Printf(TEXT("%s : rien ne bouge au-dela de la marge"), ShapeName), Moved, 0);

		// 2. A bubble whose path goes straight through the middle: it slides
		// round the edge, it never jumps across - in front of the performer.
		{
			FVibH2OZoneBubbleState State;
			FVector2D Previous = FVibH2OSteeringMath::PushOutOfZone(FVector2D(-400.0, 0.0), Params, State, 7, Frame);
			double MaxStep = 0.0;
			int32 InsideOnTheWay = 0;
			for (double Y = -399.0; Y <= 400.0; Y += 1.0)
			{
				const FVector2D Out = FVibH2OSteeringMath::PushOutOfZone(FVector2D(Y, 0.0), Params, State, 7, Frame);
				MaxStep = FMath::Max(MaxStep, (Out - Previous).Size());
				InsideOnTheWay += FVibH2OSteeringMath::ShapeNorm(Out, Params.HalfSize, Shape) < 1.0f ? 1 : 0;
				Previous = Out;
			}
			// One centimetre a frame in; the glide adds its own bounded speed.
			// A jump across the zone would be some 180 cm in a single frame.
			TestTrue(FString::Printf(TEXT("%s : traversee par le centre sans saut (pas max %.1f cm)"), ShapeName, MaxStep), MaxStep < 12.0);
			TestEqual(FString::Printf(TEXT("%s : traversee sans jamais entrer"), ShapeName), InsideOnTheWay, 0);
		}

		// 2b. The control: with no memory of its side - a turn rate so high it
		// follows the real side at once - the same crossing DOES jump. A test
		// that could not see the jump would prove nothing above.
		{
			FVibH2OZonePushParams Forgetful = Params;
			Forgetful.GlideRadiansPerSecond = 1.0e6f;
			FVibH2OZoneBubbleState State;
			FVector2D Previous = FVibH2OSteeringMath::PushOutOfZone(FVector2D(-400.0, 0.0), Forgetful, State, 7, Frame);
			double MaxStep = 0.0;
			for (double Y = -399.0; Y <= 400.0; Y += 1.0)
			{
				const FVector2D Out = FVibH2OSteeringMath::PushOutOfZone(FVector2D(Y, 0.0), Forgetful, State, 7, Frame);
				MaxStep = FMath::Max(MaxStep, (Out - Previous).Size());
				Previous = Out;
			}
			TestTrue(FString::Printf(TEXT("%s : temoin sans memoire du cote, le saut est bien detecte (%.0f cm)"), ShapeName, MaxStep), MaxStep > 100.0);
		}

		// 3. Switched off, the zone touches nothing; half on, half the push.
		{
			FVibH2OZoneBubbleState State;
			FVibH2OZonePushParams Off = Params;
			Off.Weight = 0.0f;
			const FVector2D Point(10.0, 20.0);
			TestTrue(FString::Printf(TEXT("%s : zone eteinte, aucun effet"), ShapeName),
				FVibH2OSteeringMath::PushOutOfZone(Point, Off, State, 3, Frame).Equals(Point, 0.0));

			FVibH2OZoneBubbleState StateFull;
			FVibH2OZoneBubbleState StateHalf;
			FVibH2OZonePushParams Half = Params;
			Half.Weight = 0.5f;
			const FVector2D Full = FVibH2OSteeringMath::PushOutOfZone(Point, Params, StateFull, 3, Frame);
			const FVector2D Partial = FVibH2OSteeringMath::PushOutOfZone(Point, Half, StateHalf, 3, Frame);
			TestTrue(FString::Printf(TEXT("%s : zone a moitie allumee, moitie de la poussee"), ShapeName),
				Partial.Equals((Point + Full) * 0.5, 0.01));
		}
	}

	// 4. The soft band: continuous at its edge, never negative inside.
	TestTrue(TEXT("Marge douce : continue a son bord"), FMath::IsNearlyEqual(FVibH2OSteeringMath::SoftOutside(0.5f - 1.0e-4f, 0.5f), 0.5f, 1.0e-3f));
	TestTrue(TEXT("Marge douce : toujours dehors, meme au centre"), FVibH2OSteeringMath::SoftOutside(-1.0f, 0.5f) > 0.0f);
	TestEqual(TEXT("Marge douce : identite au-dela"), FVibH2OSteeringMath::SoftOutside(2.0f, 0.5f), 2.0f);

	return true;
}

// ---------------------------------------------------------------------------
// The remote: command lines, HTTP heads, and a real request over TCP.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2ORemoteServerTest,
	"VibH2O.Remote.Telecommande",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FVibH2ORemoteServerTest::RunTest(const FString& Parameters)
{
	using namespace VibH2OSteeringTestUtils;

	// 1. Command lines.
	{
		FVibH2OOscMessage Message;
		TestTrue(TEXT("Commande lue"), FVibH2ORemoteServer::ParseCommandLine(TEXT("  /Exclusion/Tissu/Position/ 0.25 -1 Gauche "), Message));
		TestEqual(TEXT("Adresse"), Message.Address, FString(TEXT("/Exclusion/Tissu/Position/")));
		TestEqual(TEXT("Trois arguments"), Message.Args.Num(), 3);
		if (Message.Args.Num() == 3)
		{
			TestTrue(TEXT("0.25 est un flottant"), Message.Args[0].Type == EVibH2OOscType::Float32 && FMath::IsNearlyEqual(Message.Args[0].FloatValue, 0.25f));
			TestTrue(TEXT("-1 est un entier"), Message.Args[1].Type == EVibH2OOscType::Int32 && Message.Args[1].IntValue == -1);
			TestTrue(TEXT("Gauche est une chaine"), Message.Args[2].Type == EVibH2OOscType::String && Message.Args[2].StringValue == TEXT("Gauche"));
		}
		TestFalse(TEXT("Ligne vide refusee"), FVibH2ORemoteServer::ParseCommandLine(TEXT("   "), Message));
		TestFalse(TEXT("Ligne sans adresse refusee"), FVibH2ORemoteServer::ParseCommandLine(TEXT("Attractor 1"), Message));
	}

	// 2. HTTP heads.
	{
		FString Method;
		FString Path;
		int32 Length = -1;
		TestTrue(TEXT("Entete lue"), FVibH2ORemoteServer::ParseRequestHead(
			TEXT("post /api/cmd?t=123 HTTP/1.1\r\nHost: x\r\ncontent-length:  42\r\n"), Method, Path, Length));
		TestEqual(TEXT("Methode"), Method, FString(TEXT("POST")));
		TestEqual(TEXT("Chemin sans la requete"), Path, FString(TEXT("/api/cmd")));
		TestEqual(TEXT("Longueur du corps"), Length, 42);
		TestFalse(TEXT("Entete illisible refusee"), FVibH2ORemoteServer::ParseRequestHead(TEXT("n'importe quoi"), Method, Path, Length));
	}

	// 3. A real round trip on the loopback.
	FVibH2ORemoteServer Server;
	const FString EmptyRoot = FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("VibH2ORemoteTestRoot"));
	if (!Server.Start(TEXT("127.0.0.1"), RemoteTestPort, EmptyRoot))
	{
		AddWarning(FString::Printf(TEXT("Port %d indisponible, test HTTP ignore : %s"), RemoteTestPort, *Server.GetLastError()));
		return true;
	}
	Server.SetStateJson(TEXT("{\"v\":1,\"attracteur\":\"Gauche\"}"));

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	// Sends one request, returns the whole response.
	auto Request = [SocketSubsystem](const FString& Text) -> FString
	{
		FSocket* Socket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("VibH2ORemoteTestClient"), false);
		if (Socket == nullptr)
		{
			return FString();
		}
		TSharedRef<FInternetAddr> Address = SocketSubsystem->CreateInternetAddr();
		bool bValid = false;
		Address->SetIp(TEXT("127.0.0.1"), bValid);
		Address->SetPort(RemoteTestPort);

		FString Response;
		if (Socket->Connect(*Address))
		{
			const FTCHARToUTF8 Utf8(*Text);
			int32 Sent = 0;
			Socket->Send(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length(), Sent);

			TArray<uint8> Bytes;
			const double Deadline = FPlatformTime::Seconds() + 3.0;
			while (FPlatformTime::Seconds() < Deadline)
			{
				if (Socket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromMilliseconds(100)))
				{
					uint8 Chunk[4096];
					int32 Read = 0;
					if (!Socket->Recv(Chunk, sizeof(Chunk), Read) || Read <= 0)
					{
						break;
					}
					Bytes.Append(Chunk, Read);
				}
			}
			Bytes.Add(0);
			Response = UTF8_TO_TCHAR(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()));
		}
		Socket->Close();
		SocketSubsystem->DestroySocket(Socket);
		return Response;
	};

	const FString Body = TEXT("/Attractor/Gauche/ 1\n/Attractors/FadeScale/ 2.5\n\nignoree\n");

	// Without the remote's own header, as a page of another site would send it.
	const FString Foreign = Request(FString::Printf(
		TEXT("POST /api/cmd HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Length: %d\r\n\r\n%s"),
		FTCHARToUTF8(*Body).Length(), *Body));
	TestTrue(FString::Printf(TEXT("Sans en-tete X-VibH2O, POST /api/cmd est refuse (%s)"), *Foreign.Left(40)), Foreign.StartsWith(TEXT("HTTP/1.1 403")));

	const FString Posted = Request(FString::Printf(
		TEXT("POST /api/cmd HTTP/1.1\r\nHost: 127.0.0.1\r\nx-vibh2o: 1\r\nContent-Length: %d\r\n\r\n%s"),
		FTCHARToUTF8(*Body).Length(), *Body));
	TestTrue(FString::Printf(TEXT("POST /api/cmd repond 200 (%s)"), *Posted.Left(40)), Posted.StartsWith(TEXT("HTTP/1.1 200")));
	TestTrue(TEXT("Deux commandes acceptees, la ligne sans adresse ignoree"), Posted.EndsWith(TEXT("ok 2")));

	FVibH2OOscMessage First;
	FVibH2OOscMessage Second;
	FVibH2OOscMessage None;
	TestTrue(TEXT("Premiere commande dans la file"), Server.Dequeue(First));
	TestTrue(TEXT("Seconde commande dans la file"), Server.Dequeue(Second));
	TestFalse(TEXT("Rien de plus dans la file"), Server.Dequeue(None));
	TestEqual(TEXT("Premiere commande intacte"), First.Address, FString(TEXT("/Attractor/Gauche/")));
	TestTrue(TEXT("Seconde commande intacte"), Second.Address == TEXT("/Attractors/FadeScale/") && FMath::IsNearlyEqual(Second.GetFloatArg(0), 2.5f));

	const FString State = Request(TEXT("GET /api/state HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n"));
	TestTrue(TEXT("GET /api/state renvoie l'etat publie"), State.StartsWith(TEXT("HTTP/1.1 200")) && State.EndsWith(TEXT("{\"v\":1,\"attracteur\":\"Gauche\"}")));

	// No page shipped in this test root: the server says so instead of failing.
	const FString Page = Request(TEXT("GET / HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n"));
	TestTrue(TEXT("GET / sert une page meme sans fichiers"), Page.StartsWith(TEXT("HTTP/1.1 200")) && Page.Contains(TEXT("VibH2O")));

	const FString Missing = Request(TEXT("GET /../../secret.txt HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n"));
	TestTrue(TEXT("Aucun fichier hors de la liste n'est servi"), Missing.StartsWith(TEXT("HTTP/1.1 404")));

	Server.Shutdown();
	TestFalse(TEXT("Serveur arrete"), Server.IsRunning());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
