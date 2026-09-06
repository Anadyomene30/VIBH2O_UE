// Integration tests: the whole chain, from UDP datagram to averages.
//
// They cover what no unit test can reach: that the network thread, the queue,
// the game thread and the model really do agree. The completion criterion of
// step 3 of the roadmap is verified here, automatically, rather than by eye on
// a debug overlay.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "Common/UdpSocketBuilder.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Misc/ScopeExit.h"
#include "Osc/VibH2OOscReceiver.h"
#include "SocketSubsystem.h"
#include "Sockets.h"
#include "UObject/StrongObjectPtr.h"
#include "VibH2OSettings.h"
#include "VibH2OSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace VibH2OIntegrationUtils
{
	/** Test port, away from the production port so nothing is disturbed. */
	static constexpr int32 TestPort = 9187;

	static void PadTo4(TArray<uint8>& Bytes)
	{
		while (Bytes.Num() % 4 != 0)
		{
			Bytes.Add(0);
		}
	}

	static void WriteString(TArray<uint8>& Bytes, const char* Text)
	{
		for (const char* P = Text; *P != 0; ++P)
		{
			Bytes.Add(static_cast<uint8>(*P));
		}
		Bytes.Add(0);
		PadTo4(Bytes);
	}

	static void WriteInt32(TArray<uint8>& Bytes, int32 Value)
	{
		const uint32 Raw = static_cast<uint32>(Value);
		Bytes.Add(static_cast<uint8>((Raw >> 24) & 0xFF));
		Bytes.Add(static_cast<uint8>((Raw >> 16) & 0xFF));
		Bytes.Add(static_cast<uint8>((Raw >> 8) & 0xFF));
		Bytes.Add(static_cast<uint8>(Raw & 0xFF));
	}

	static TArray<uint8> MakeIntMessage(const FString& Address, int32 Value)
	{
		TArray<uint8> Bytes;
		WriteString(Bytes, TCHAR_TO_UTF8(*Address));
		WriteString(Bytes, ",i");
		WriteInt32(Bytes, Value);
		return Bytes;
	}

	/** Float message, hand-built so it stays independent of the plugin. */
	static TArray<uint8> MakeFloatMessage(const FString& Address, float Value)
	{
		TArray<uint8> Bytes;
		WriteString(Bytes, TCHAR_TO_UTF8(*Address));
		WriteString(Bytes, ",f");
		int32 Raw = 0;
		FMemory::Memcpy(&Raw, &Value, sizeof(int32));
		WriteInt32(Bytes, Raw);
		return Bytes;
	}

	/** Builds an OSC message in memory, bypassing the network. */
	static FVibH2OOscMessage MakeMessage(const FString& Address, float Value)
	{
		FVibH2OOscMessage Message(Address);
		Message.Args.Add(FVibH2OOscValue::MakeFloat(Value));
		return Message;
	}

	static FVibH2OOscMessage MakeMessageInt(const FString& Address, int32 Value)
	{
		FVibH2OOscMessage Message(Address);
		Message.Args.Add(FVibH2OOscValue::MakeInt(Value));
		return Message;
	}

}

// ---------------------------------------------------------------------------
// The real network path: a UDP datagram in, a message out of the queue.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2OReceiverLoopbackTest,
	"VibH2O.Integration.BoucleReseau",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FVibH2OReceiverLoopbackTest::RunTest(const FString& Parameters)
{
	using namespace VibH2OIntegrationUtils;

	FVibH2OOscReceiver Receiver;
	if (!Receiver.Start(TEXT("127.0.0.1"), TestPort, 1 << 16, 4096))
	{
		// A busy port is not a plugin defect. Report it without failing the
		// suite.
		AddWarning(FString::Printf(
			TEXT("Port %d indisponible, test de boucle reseau ignore : %s"), TestPort, *Receiver.GetLastError()));
		return true;
	}

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (SocketSubsystem == nullptr)
	{
		AddWarning(TEXT("Aucun sous-systeme de socket, test ignore."));
		return true;
	}

	FSocket* Sender = FUdpSocketBuilder(TEXT("VibH2OTestSender")).AsReusable().Build();
	if (Sender == nullptr)
	{
		AddWarning(TEXT("Socket emetteur indisponible, test ignore."));
		return true;
	}

	TSharedRef<FInternetAddr> Destination = SocketSubsystem->CreateInternetAddr();
	bool bAddressValid = false;
	Destination->SetIp(TEXT("127.0.0.1"), bAddressValid);
	Destination->SetPort(TestPort);

	// A 7 x 3 room-plan burst plus one data message, just as Max would send.
	TArray<TArray<uint8>> Packets;
	Packets.Add(MakeIntMessage(TEXT("/RoomMapping/columns/"), 7));
	Packets.Add(MakeIntMessage(TEXT("/RoomMapping/rows/"), 3));
	for (int32 SeatIndex = 1; SeatIndex <= 21; ++SeatIndex)
	{
		Packets.Add(MakeIntMessage(FString::Printf(TEXT("/RoomMapping/%d/"), SeatIndex), SeatIndex));
	}
	Packets.Add(MakeFloatMessage(TEXT("/BPM/1/"), 72.5f));

	for (const TArray<uint8>& Packet : Packets)
	{
		int32 Sent = 0;
		Sender->SendTo(Packet.GetData(), Packet.Num(), Sent, *Destination);
	}

	// Wait for the network thread to fill the queue. UDP over loopback is near
	// instant, but it remains asynchronous.
	TArray<FVibH2OOscMessage> Received;
	const double Deadline = FPlatformTime::Seconds() + 3.0;
	while (FPlatformTime::Seconds() < Deadline && Received.Num() < Packets.Num())
	{
		FVibH2OOscMessage Message;
		while (Receiver.Dequeue(Message))
		{
			Received.Add(MoveTemp(Message));
		}
		FPlatformProcess::Sleep(0.01f);
	}

	Receiver.Shutdown();
	Sender->Close();
	SocketSubsystem->DestroySocket(Sender);

	TestEqual(TEXT("Tous les messages ont traverse le reseau et la file"), Received.Num(), Packets.Num());
	TestEqual(TEXT("Aucun message jete"), Receiver.GetMessagesDropped(), 0);
	TestEqual(TEXT("Aucun paquet malforme"), Receiver.GetMalformedPackets(), 0);
	TestEqual(TEXT("La file est vide a la fin"), Receiver.GetQueueDepth(), 0);

	if (Received.Num() >= 2)
	{
		TestEqual(TEXT("Premier message"), Received[0].Address, FString(TEXT("/RoomMapping/columns/")));
		TestEqual(TEXT("Valeur du premier"), Received[0].GetIntArg(0), 7);
	}
	if (Received.Num() == Packets.Num())
	{
		TestEqual(TEXT("Dernier message"), Received.Last().Address, FString(TEXT("/BPM/1/")));
		TestEqual(TEXT("Valeur du dernier"), Received.Last().GetFloatArg(0), 72.5f);
	}

	return true;
}

// ---------------------------------------------------------------------------
// The model chain: burst, silence, build, averages, quiet sensor.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2OSubsystemPipelineTest,
	"VibH2O.Integration.ChaineComplete",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FVibH2OSubsystemPipelineTest::RunTest(const FString& Parameters)
{
	using namespace VibH2OIntegrationUtils;

	UVibH2OSettings* Settings = GetMutableDefault<UVibH2OSettings>();

	// Saved for restoration: the CDO is shared by the whole editor.
	const float SavedSettle = Settings->RoomMappingSettleSeconds;
	const float SavedStale = Settings->StaleTimeoutSeconds;
	const float SavedSmoothing = Settings->ValueSmoothingSeconds;
	const EVibH2OSeatOrder SavedOrder = Settings->SeatOrder;
	const bool bSavedMissing = Settings->bTreatMissingSeatsAsEmpty;

	Settings->RoomMappingSettleSeconds = 0.0f;
	Settings->StaleTimeoutSeconds = 0.5f;
	Settings->ValueSmoothingSeconds = 0.0f; // no smoothing: exact values
	Settings->SeatOrder = EVibH2OSeatOrder::ColumnMajor;
	// Explicit, so the test does not depend on the configuration file.
	Settings->bTreatMissingSeatsAsEmpty = false;

	ON_SCOPE_EXIT
	{
		Settings->RoomMappingSettleSeconds = SavedSettle;
		Settings->StaleTimeoutSeconds = SavedStale;
		Settings->ValueSmoothingSeconds = SavedSmoothing;
		Settings->SeatOrder = SavedOrder;
		Settings->bTreatMissingSeatsAsEmpty = bSavedMissing;
	};

	// A UGameInstanceSubsystem declares ClassWithin = UGameInstance, so it
	// cannot be created in the transient package. We build it a bare
	// GameInstance, which is enough here - the subsystem never uses it.
	TStrongObjectPtr<UGameInstance> GameInstance(NewObject<UGameInstance>(GEngine));
	TStrongObjectPtr<UVibH2OSubsystem> Subsystem(NewObject<UVibH2OSubsystem>(GameInstance.Get()));
	if (!Subsystem.IsValid())
	{
		AddError(TEXT("Impossible d'instancier le subsystem."));
		return false;
	}

	// --- A 7 x 3 room-plan burst, with two empty seats.
	//
	// Non-square room: the transposition shows. Empty seats: pitfall 3 is
	// exercised.
	const int32 Columns = 7;
	const int32 Rows = 3;
	const int32 EmptySeatA = 5;   // colonne 1, rangee 1 en column-major
	const int32 EmptySeatB = 20;  // colonne 6, rangee 1

	Subsystem->InjectOscMessage(MakeMessageInt(TEXT("/RoomMapping/columns/"), Columns));
	Subsystem->InjectOscMessage(MakeMessageInt(TEXT("/RoomMapping/rows/"), Rows));
	for (int32 SeatIndex = 1; SeatIndex <= Columns * Rows; ++SeatIndex)
	{
		const int32 Id = (SeatIndex == EmptySeatA || SeatIndex == EmptySeatB) ? 0 : (100 + SeatIndex);
		Subsystem->InjectOscMessage(MakeMessageInt(FString::Printf(TEXT("/RoomMapping/%d/"), SeatIndex), Id));
	}

	// Nothing may have been built before the tick: the burst has no end marker,
	// silence stands in for one.
	TestFalse(TEXT("Aucune salle avant le tick"), Subsystem->HasRoom());

	Subsystem->Tick(1.0f / 60.0f);

	TestTrue(TEXT("La salle est construite apres le silence"), Subsystem->HasRoom());

	const FVibH2ORoomSnapshot& Room = Subsystem->GetRoomRef();
	TestEqual(TEXT("Colonnes"), Room.Columns, Columns);
	TestEqual(TEXT("Rangees"), Room.Rows, Rows);
	TestEqual(TEXT("21 places"), Room.Seats.Num(), Columns * Rows);
	TestEqual(TEXT("19 sieges occupes, 2 vides"), Room.GetOccupiedCount(), Columns * Rows - 2);

	// Pitfall 4, checked on the room actually built: index 4 must land on
	// column 1, row 0.
	if (const FVibH2OSeat* Seat = Room.FindSeatByIndex(4))
	{
		TestEqual(TEXT("Index 4 -> colonne 1"), Seat->Column, 1);
		TestEqual(TEXT("Index 4 -> rangee 0"), Seat->Row, 0);
	}

	// Pitfall 3: a seat at 0 is not occupied.
	if (const FVibH2OSeat* Seat = Room.FindSeatByIndex(EmptySeatA))
	{
		TestFalse(TEXT("Le siege a 0 est vide"), Seat->bOccupied);
		TestEqual(TEXT("Aucun individu sur un siege vide"), Seat->IndividualId, 0);
	}

	// --- Rebuild by difference: resending the same plan must change nothing
	// and raise no event.
	int32 RoomChangedCount = 0;
	const int32 GenerationBefore = Room.Generation;

	for (int32 SeatIndex = 1; SeatIndex <= Columns * Rows; ++SeatIndex)
	{
		const int32 Id = (SeatIndex == EmptySeatA || SeatIndex == EmptySeatB) ? 0 : (100 + SeatIndex);
		Subsystem->InjectOscMessage(MakeMessageInt(FString::Printf(TEXT("/RoomMapping/%d/"), SeatIndex), Id));
	}
	Subsystem->InjectOscMessage(MakeMessageInt(TEXT("/RoomMapping/columns/"), Columns));
	Subsystem->InjectOscMessage(MakeMessageInt(TEXT("/RoomMapping/rows/"), Rows));
	Subsystem->Tick(1.0f / 60.0f);

	TestEqual(
		TEXT("Un plan identique ne reconstruit pas la salle"),
		Subsystem->GetRoomRef().Generation, GenerationBefore);

	// --- The data.
	//
	// Two individuals filled in with known synchronies, plus one message
	// addressed to individual 0: it must go nowhere.
	Subsystem->InjectOscMessage(MakeMessage(TEXT("/BPM/101/"), 60.0f));
	Subsystem->InjectOscMessage(MakeMessage(TEXT("/Synchronie/101/"), 0.9f));
	Subsystem->InjectOscMessage(MakeMessage(TEXT("/SD/101/"), 0.5f));

	Subsystem->InjectOscMessage(MakeMessage(TEXT("/BPM/102/"), 120.0f));
	Subsystem->InjectOscMessage(MakeMessage(TEXT("/Synchronie/102/"), 0.3f));
	Subsystem->InjectOscMessage(MakeMessage(TEXT("/SD/102/"), 0.1f));

	// Pitfall 3: individual 0 does not exist, no data is routed to it.
	Subsystem->InjectOscMessage(MakeMessage(TEXT("/BPM/0/"), 99.0f));
	Subsystem->InjectOscMessage(MakeMessage(TEXT("/Synchronie/0/"), 0.0f));

	Subsystem->Tick(1.0f / 60.0f);

	TestEqual(TEXT("Deux individus connus, pas trois"), Subsystem->GetIndividualCount(), 2);
	TestEqual(TEXT("Deux individus actifs"), Subsystem->GetActiveIndividualCount(), 2);

	// Collective synchrony is the mean of those two and of nothing else. Were
	// an empty seat counted, it would come out far lower.
	TestTrue(
		FString::Printf(TEXT("Synchronie collective = 0.6 (obtenu %f)"), Subsystem->GetCollectiveSynchrony()),
		FMath::IsNearlyEqual(Subsystem->GetCollectiveSynchrony(), 0.6f, 0.001f));

	TestTrue(
		FString::Printf(TEXT("BPM moyen = 90 (obtenu %f)"), Subsystem->GetAverageBpm()),
		FMath::IsNearlyEqual(Subsystem->GetAverageBpm(), 90.0f, 0.01f));

	// --- The beat.
	FVibH2OIndividualState State;
	TestTrue(TEXT("L'individu 101 existe"), Subsystem->GetIndividual(101, State));
	const float PhaseBefore = State.BeatPhase;

	// Half a second at 60 BPM advances the phase by half a beat.
	Subsystem->Tick(0.5f);
	Subsystem->GetIndividual(101, State);
	const float Advance = FMath::Frac(State.BeatPhase - PhaseBefore + 1.0f);
	TestTrue(
		FString::Printf(TEXT("La phase a avance d'un demi-battement (%f)"), Advance),
		FMath::IsNearlyEqual(Advance, 0.5f, 0.01f));

	// One full second at 60 BPM crosses exactly one beat, whatever the current
	// phase. This pins the beat *count* down - the deferred-broadcast rework
	// must not change it.
	const int32 BeatCountBefore = State.BeatCount;
	Subsystem->Tick(1.0f);
	Subsystem->GetIndividual(101, State);
	TestEqual(TEXT("Une seconde a 60 BPM ajoute exactement un battement"), State.BeatCount, BeatCountBefore + 1);

	// --- The quiet sensor.
	//
	// The timeout was lowered to 0.5 s at the top of the test. We wait, we
	// tick, and the individual must drop out of the averages.
	FPlatformProcess::Sleep(0.6f);
	Subsystem->Tick(1.0f / 60.0f);

	Subsystem->GetIndividual(101, State);
	TestTrue(TEXT("Le capteur est declare muet"), State.bStale);
	TestEqual(TEXT("Plus aucun individu ne compte dans les moyennes"), Subsystem->GetActiveIndividualCount(), 0);
	TestEqual(TEXT("La synchronie collective retombe a zero"), Subsystem->GetCollectiveSynchrony(), 0.0f);

	// A quiet sensor's phase is frozen, NOT reset: when the sensor comes back
	// it resumes where it was, with no jump.
	const float FrozenPhase = State.BeatPhase;
	Subsystem->Tick(0.5f);
	Subsystem->GetIndividual(101, State);
	TestEqual(TEXT("La phase d'un capteur muet est gelee, pas remise a zero"), State.BeatPhase, FrozenPhase);
	TestTrue(TEXT("La phase gelee n'est pas nulle"), FrozenPhase > 0.0f);

	// --- Effects and control.
	TestTrue(TEXT("Un effet inconnu est considere actif"), Subsystem->IsEffectEnabled(TEXT("EffetInexistant")));

	Subsystem->InjectOscMessage(MakeMessageInt(TEXT("/Effect/Drift/"), 0));
	TestFalse(TEXT("/Effect/Drift/ 0 desactive l'effet"), Subsystem->IsEffectEnabled(VibH2OEffects::Drift));

	Subsystem->InjectOscMessage(MakeMessage(TEXT("/Blend/"), 0.75f));
	TestEqual(TEXT("/Blend/ met a jour le morph"), Subsystem->GetOscBlendAlpha(), 0.75f);

	// bIgnoreOscControl cuts CONTROL, not data: exactly what is wanted during a
	// Sequencer render.
	Subsystem->bIgnoreOscControl = true;
	Subsystem->InjectOscMessage(MakeMessage(TEXT("/Blend/"), 0.10f));
	TestEqual(TEXT("bIgnoreOscControl ignore le controle"), Subsystem->GetOscBlendAlpha(), 0.75f);

	Subsystem->InjectOscMessage(MakeMessage(TEXT("/BPM/102/"), 111.0f));
	Subsystem->Tick(1.0f / 60.0f);
	Subsystem->GetIndividual(102, State);
	TestEqual(TEXT("Mais les donnees des capteurs continuent d'arriver"), State.Bpm, 111.0f);

	// --- A seat going to 0 mid-run: the bubble must disappear.
	// A single message is resent, without the rest of the burst. With
	// bTreatMissingSeatsAsEmpty false, the other seats keep their occupant:
	// exactly the behaviour that protects against a lost datagram.
	Subsystem->bIgnoreOscControl = false;
	Subsystem->InjectOscMessage(MakeMessageInt(TEXT("/RoomMapping/1/"), 0));
	Subsystem->Tick(1.0f / 60.0f);

	if (const FVibH2OSeat* Seat = Subsystem->GetRoomRef().FindSeatByIndex(1))
	{
		TestFalse(TEXT("Un siege passe a 0 devient vide"), Seat->bOccupied);
	}
	TestEqual(TEXT("Un siege de moins est occupe"), Subsystem->GetRoomRef().GetOccupiedCount(), Columns * Rows - 3);

	// --- Degenerate normalisation range: no NaN may escape.
	//
	// With min == max, GetRangePct divides by zero and Clamp does not stop a
	// NaN. The guard must fall back to clamping the raw value.
	{
		const float SavedExcMin = Settings->ExcitationInputMin;
		const float SavedExcMax = Settings->ExcitationInputMax;
		Settings->ExcitationInputMin = 0.5f;
		Settings->ExcitationInputMax = 0.5f;

		Subsystem->InjectOscMessage(MakeMessage(TEXT("/SD/102/"), 0.75f));
		Subsystem->Tick(1.0f / 60.0f);

		Settings->ExcitationInputMin = SavedExcMin;
		Settings->ExcitationInputMax = SavedExcMax;

		Subsystem->GetIndividual(102, State);
		TestTrue(TEXT("Plage degeneree : l'excitation reste finie"), FMath::IsFinite(State.Excitation));
		TestTrue(TEXT("Plage degeneree : l'excitation reste dans [0,1]"),
			State.Excitation >= 0.0f && State.Excitation <= 1.0f);
		TestEqual(TEXT("Plage degeneree : repli sur la valeur brute bornee"), State.ExcitationTarget, 0.75f);
	}

	Subsystem->StopListening();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
