// Automation tests for the OSC parser.
//
//   UnrealEditor-Cmd.exe <projet>.uproject -ExecCmds="Automation RunTests VibH2O" -unattended -nullrhi
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "Osc/VibH2OOscParser.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace VibH2OOscTestUtils
{
	/**
	 * OSC frame builder, written independently of the parser.
	 *
	 * It serves to produce a variety of test cases; but the first test compares
	 * the parser against a frame written out byte by byte by hand, without
	 * going through this builder. Without that safeguard, a builder sharing a
	 * mistake with the parser would validate its own bugs.
	 */
	struct FFrameBuilder
	{
		TArray<uint8> Bytes;

		void PadTo4()
		{
			while (Bytes.Num() % 4 != 0)
			{
				Bytes.Add(0);
			}
		}

		void WriteString(const char* Text)
		{
			for (const char* P = Text; *P != 0; ++P)
			{
				Bytes.Add(static_cast<uint8>(*P));
			}
			Bytes.Add(0);
			PadTo4();
		}

		void WriteInt32(int32 Value)
		{
			const uint32 Raw = static_cast<uint32>(Value);
			Bytes.Add(static_cast<uint8>((Raw >> 24) & 0xFF));
			Bytes.Add(static_cast<uint8>((Raw >> 16) & 0xFF));
			Bytes.Add(static_cast<uint8>((Raw >> 8) & 0xFF));
			Bytes.Add(static_cast<uint8>(Raw & 0xFF));
		}

		void WriteFloat32(float Value)
		{
			int32 Raw = 0;
			FMemory::Memcpy(&Raw, &Value, sizeof(int32));
			WriteInt32(Raw);
		}

		void WriteUInt64(uint64 Value)
		{
			for (int32 Shift = 56; Shift >= 0; Shift -= 8)
			{
				Bytes.Add(static_cast<uint8>((Value >> Shift) & 0xFF));
			}
		}
	};

	/** Message with a single integer argument. */
	static TArray<uint8> MakeIntMessage(const char* Address, int32 Value)
	{
		FFrameBuilder Builder;
		Builder.WriteString(Address);
		Builder.WriteString(",i");
		Builder.WriteInt32(Value);
		return Builder.Bytes;
	}

	/** Message with a single float argument. */
	static TArray<uint8> MakeFloatMessage(const char* Address, float Value)
	{
		FFrameBuilder Builder;
		Builder.WriteString(Address);
		Builder.WriteString(",f");
		Builder.WriteFloat32(Value);
		return Builder.Bytes;
	}
}

// ---------------------------------------------------------------------------
// Reference frame, written out by hand.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2OOscKnownFrameTest,
	"VibH2O.Osc.TrameDeReference",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FVibH2OOscKnownFrameTest::RunTest(const FString& Parameters)
{
	// "/RoomMapping/columns/" carrying the integer 5 - exactly what the Max
	// patch emits, trailing slash included. 21 characters, one null, two
	// padding bytes, the type-tag string ",i" padded to 4, then 5 big-endian.
	const uint8 Frame[] = {
		'/', 'R', 'o', 'o', 'm', 'M', 'a', 'p', 'p', 'i', 'n', 'g',
		'/', 'c', 'o', 'l', 'u', 'm', 'n', 's', '/', 0, 0, 0,
		',', 'i', 0, 0,
		0, 0, 0, 5
	};
	static_assert(sizeof(Frame) == 32, "La trame de reference doit faire 32 octets.");

	TArray<FVibH2OOscMessage> Messages;
	TestTrue(TEXT("La trame de reference se decode"), FVibH2OOscParser::ParsePacket(Frame, sizeof(Frame), Messages));
	TestEqual(TEXT("Un seul message"), Messages.Num(), 1);

	if (Messages.Num() != 1)
	{
		return false;
	}

	TestEqual(TEXT("Adresse conservee telle quelle, slash final compris"), Messages[0].Address, FString(TEXT("/RoomMapping/columns/")));
	TestEqual(TEXT("Un argument"), Messages[0].Args.Num(), 1);
	TestEqual(TEXT("Valeur 5"), Messages[0].GetIntArg(0), 5);

	return true;
}

// ---------------------------------------------------------------------------
// Pitfall 1 - the trailing slash produces an empty segment.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2OOscTrailingSlashTest,
	"VibH2O.Osc.SlashFinal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FVibH2OOscTrailingSlashTest::RunTest(const FString& Parameters)
{
	using namespace VibH2OOscTestUtils;

	const TArray<uint8> Frame = MakeIntMessage("/RoomMapping/12/", 42);

	TArray<FVibH2OOscMessage> Messages;
	TestTrue(TEXT("Decodage"), FVibH2OOscParser::ParsePacket(Frame, Messages));
	if (Messages.Num() != 1)
	{
		return false;
	}

	TArray<FString> Segments;
	Messages[0].GetSegments(Segments);
	TestEqual(TEXT("Le segment vide du slash final est ignore"), Segments.Num(), 2);
	TestEqual(TEXT("Premier segment"), Segments[0], FString(TEXT("RoomMapping")));
	TestEqual(TEXT("Deuxieme segment"), Segments[1], FString(TEXT("12")));

	// Multiple slashes must not produce empty segments either.
	const TArray<uint8> Doubled = MakeIntMessage("/RoomMapping//12//", 42);
	Messages.Reset();
	FVibH2OOscParser::ParsePacket(Doubled, Messages);
	if (Messages.Num() == 1)
	{
		Messages[0].GetSegments(Segments);
		TestEqual(TEXT("Slashs multiples : toujours deux segments"), Segments.Num(), 2);
	}

	// The prefix must match on a segment boundary, not character by character:
	// "/BPM" must not catch "/BPMX".
	TArray<uint8> Bpm = MakeFloatMessage("/BPM/7/", 72.5f);
	Messages.Reset();
	FVibH2OOscParser::ParsePacket(Bpm, Messages);
	if (Messages.Num() == 1)
	{
		TestTrue(TEXT("/BPM/7/ correspond au prefixe /BPM"), Messages[0].MatchesPrefix(TEXT("/BPM")));

		TArray<FString> After;
		TestTrue(TEXT("Segments apres prefixe"), Messages[0].GetSegmentsAfterPrefix(TEXT("/BPM"), After));
		TestEqual(TEXT("Un seul segment restant"), After.Num(), 1);
		if (After.Num() == 1)
		{
			TestEqual(TEXT("L'identifiant"), After[0], FString(TEXT("7")));
		}
	}

	TArray<uint8> BpmX = MakeFloatMessage("/BPMX/7/", 72.5f);
	Messages.Reset();
	FVibH2OOscParser::ParsePacket(BpmX, Messages);
	if (Messages.Num() == 1)
	{
		TestFalse(TEXT("/BPMX/7/ ne correspond pas au prefixe /BPM"), Messages[0].MatchesPrefix(TEXT("/BPM")));
	}

	return true;
}

// ---------------------------------------------------------------------------
// The three required types, and the 4-byte padding.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2OOscTypesTest,
	"VibH2O.Osc.Types",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FVibH2OOscTypesTest::RunTest(const FString& Parameters)
{
	using namespace VibH2OOscTestUtils;

	FFrameBuilder Builder;
	Builder.WriteString("/Mix/1/");
	Builder.WriteString(",ifs");
	Builder.WriteInt32(-17);
	Builder.WriteFloat32(72.5f);
	Builder.WriteString("HautGauche");

	TestEqual(TEXT("La trame est alignee sur 4 octets"), Builder.Bytes.Num() % 4, 0);

	TArray<FVibH2OOscMessage> Messages;
	TestTrue(TEXT("Decodage"), FVibH2OOscParser::ParsePacket(Builder.Bytes, Messages));
	if (Messages.Num() != 1 || Messages[0].Args.Num() != 3)
	{
		AddError(TEXT("Les trois arguments n'ont pas ete decodes."));
		return false;
	}

	TestEqual(TEXT("Entier"), Messages[0].GetIntArg(0), -17);
	TestEqual(TEXT("Flottant"), Messages[0].GetFloatArg(1), 72.5f);
	TestEqual(TEXT("Chaine"), Messages[0].GetStringArg(2), FString(TEXT("HautGauche")));

	// Cross conversion: Max may send an integer where a float is expected.
	// Losing the data for that reason would be absurd.
	TestEqual(TEXT("Un entier se relit en flottant"), Messages[0].GetFloatArg(0), -17.0f);
	TestEqual(TEXT("Un flottant se relit en entier"), Messages[0].GetIntArg(1), 73);

	// A string whose length is already a multiple of 4: the standard then
	// requires four padding bytes, not zero.
	FFrameBuilder Aligned;
	Aligned.WriteString("/ab/");
	Aligned.WriteString(",s");
	Aligned.WriteString("abcd");
	Messages.Reset();
	TestTrue(TEXT("Chaine de longueur multiple de 4"), FVibH2OOscParser::ParsePacket(Aligned.Bytes, Messages));
	if (Messages.Num() == 1)
	{
		TestEqual(TEXT("Chaine relue"), Messages[0].GetStringArg(0), FString(TEXT("abcd")));
	}

	// Boolean types with no payload.
	FFrameBuilder Flags;
	Flags.WriteString("/Effect/Drift/");
	Flags.WriteString(",TF");
	Messages.Reset();
	TestTrue(TEXT("Types T et F"), FVibH2OOscParser::ParsePacket(Flags.Bytes, Messages));
	if (Messages.Num() == 1 && Messages[0].Args.Num() == 2)
	{
		TestTrue(TEXT("T vaut vrai"), Messages[0].Args[0].AsBool());
		TestFalse(TEXT("F vaut faux"), Messages[0].Args[1].AsBool());
	}

	return true;
}

// ---------------------------------------------------------------------------
// Bundles.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2OOscBundleTest,
	"VibH2O.Osc.Bundle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FVibH2OOscBundleTest::RunTest(const FString& Parameters)
{
	using namespace VibH2OOscTestUtils;

	const TArray<uint8> First = MakeIntMessage("/RoomMapping/1/", 11);
	const TArray<uint8> Second = MakeFloatMessage("/BPM/11/", 68.25f);

	FFrameBuilder Bundle;
	Bundle.WriteString("#bundle");
	Bundle.WriteUInt64(1);
	Bundle.WriteInt32(First.Num());
	Bundle.Bytes.Append(First);
	Bundle.WriteInt32(Second.Num());
	Bundle.Bytes.Append(Second);

	TArray<FVibH2OOscMessage> Messages;
	TestTrue(TEXT("Decodage du bundle"), FVibH2OOscParser::ParsePacket(Bundle.Bytes, Messages));
	TestEqual(TEXT("Deux messages extraits"), Messages.Num(), 2);
	if (Messages.Num() == 2)
	{
		TestEqual(TEXT("Premier"), Messages[0].Address, FString(TEXT("/RoomMapping/1/")));
		TestEqual(TEXT("Deuxieme"), Messages[1].Address, FString(TEXT("/BPM/11/")));
		TestEqual(TEXT("Le timetag est propage"), static_cast<int32>(Messages[1].TimeTag), 1);
	}

	// A bundle nested inside a bundle: the standard allows it.
	FFrameBuilder Nested;
	Nested.WriteString("#bundle");
	Nested.WriteUInt64(0);
	Nested.WriteInt32(Bundle.Bytes.Num());
	Nested.Bytes.Append(Bundle.Bytes);

	Messages.Reset();
	TestTrue(TEXT("Bundle imbrique"), FVibH2OOscParser::ParsePacket(Nested.Bytes, Messages));
	TestEqual(TEXT("Toujours deux messages"), Messages.Num(), 2);

	return true;
}

// ---------------------------------------------------------------------------
// Robustness: nothing may ever crash.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVibH2OOscMalformedTest,
	"VibH2O.Osc.TramesMalformees",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FVibH2OOscMalformedTest::RunTest(const FString& Parameters)
{
	using namespace VibH2OOscTestUtils;

	TArray<FVibH2OOscMessage> Messages;

	// Empty frame and null pointer.
	TestFalse(TEXT("Pointeur nul"), FVibH2OOscParser::ParsePacket(nullptr, 0, Messages));
	TestFalse(TEXT("Taille nulle"), FVibH2OOscParser::ParsePacket(reinterpret_cast<const uint8*>("x"), 0, Messages));

	// Every possible prefix of a valid frame. That is exactly what a datagram
	// truncated in flight produces.
	FFrameBuilder Full;
	Full.WriteString("/RoomMapping/25/");
	Full.WriteString(",ifs");
	Full.WriteInt32(25);
	Full.WriteFloat32(1.5f);
	Full.WriteString("fin");

	for (int32 Length = 1; Length < Full.Bytes.Num(); ++Length)
	{
		Messages.Reset();
		// The contract is not "returns false" but "does not crash and does not
		// read out of bounds". A prefix may well remain decodable.
		FVibH2OOscParser::ParsePacket(Full.Bytes.GetData(), Length, Messages);
	}

	// A bundle announcing an absurd element size.
	FFrameBuilder BadBundle;
	BadBundle.WriteString("#bundle");
	BadBundle.WriteUInt64(0);
	BadBundle.WriteInt32(0x7FFFFFFF);
	BadBundle.WriteInt32(0);
	Messages.Reset();
	TestFalse(TEXT("Taille d'element aberrante rejetee"), FVibH2OOscParser::ParsePacket(BadBundle.Bytes, Messages));

	// A bundle announcing a negative size.
	FFrameBuilder NegBundle;
	NegBundle.WriteString("#bundle");
	NegBundle.WriteUInt64(0);
	NegBundle.WriteInt32(-8);
	Messages.Reset();
	TestFalse(TEXT("Taille negative rejetee"), FVibH2OOscParser::ParsePacket(NegBundle.Bytes, Messages));

	// Address with no terminating null.
	const uint8 NoTerminator[] = { '/', 'a', 'b', 'c' };
	Messages.Reset();
	TestFalse(TEXT("Adresse non terminee rejetee"), FVibH2OOscParser::ParsePacket(NoTerminator, sizeof(NoTerminator), Messages));

	// Message not starting with a slash.
	const uint8 NoSlash[] = { 'a', 'b', 'c', 0 };
	Messages.Reset();
	TestFalse(TEXT("Adresse sans slash initial rejetee"), FVibH2OOscParser::ParsePacket(NoSlash, sizeof(NoSlash), Messages));

	// Type-tag string announcing more arguments than the frame contains.
	FFrameBuilder Lying;
	Lying.WriteString("/x/");
	Lying.WriteString(",iiii");
	Lying.WriteInt32(1);
	Messages.Reset();
	TestFalse(TEXT("Chaine de types mensongere rejetee"), FVibH2OOscParser::ParsePacket(Lying.Bytes, Messages));

	// Pseudo-random noise: no frame may bring the parser down.
	uint32 State = 0x12345678u;
	TArray<uint8> Noise;
	for (int32 Iteration = 0; Iteration < 2000; ++Iteration)
	{
		State = State * 1664525u + 1013904223u;
		const int32 Size = static_cast<int32>(State % 96u) + 1;
		Noise.Reset(Size);
		for (int32 Index = 0; Index < Size; ++Index)
		{
			State = State * 1664525u + 1013904223u;
			Noise.Add(static_cast<uint8>((State >> 16) & 0xFF));
		}
		// One frame in three starts with a slash, so the argument decoding is
		// reached and not just the initial rejection.
		if (Iteration % 3 == 0 && Noise.Num() > 0)
		{
			Noise[0] = static_cast<uint8>('/');
		}
		Messages.Reset();
		FVibH2OOscParser::ParsePacket(Noise, Messages);
	}

	AddInfo(TEXT("2000 trames aleatoires et tous les prefixes tronques traites sans crash."));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
