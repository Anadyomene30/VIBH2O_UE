#include "Cues/VibH2OCueTypes.h"

FString FVibH2OCueInput::Describe() const
{
	switch (Source)
	{
	case EVibH2OCueSource::Osc:
		return bHasValue
			? FString::Printf(TEXT("OSC %s %g"), *Address, Value)
			: FString::Printf(TEXT("OSC %s"), *Address);

	case EVibH2OCueSource::MidiNote:
		return FString::Printf(TEXT("MIDI note %d, canal %d (velocite %d)"), Number, Channel, FMath::RoundToInt(Value * 127.0f));

	case EVibH2OCueSource::MidiControlChange:
		return FString::Printf(TEXT("MIDI CC %d, canal %d (valeur %d)"), Number, Channel, FMath::RoundToInt(Value * 127.0f));

	case EVibH2OCueSource::MidiProgramChange:
		return FString::Printf(TEXT("MIDI program change %d, canal %d"), Number, Channel);

	case EVibH2OCueSource::Key:
	{
		FString Text;
		if (bCtrl)
		{
			Text += TEXT("Ctrl+");
		}
		if (bShift)
		{
			Text += TEXT("Maj+");
		}
		if (bAlt)
		{
			Text += TEXT("Alt+");
		}
		return FString::Printf(TEXT("Touche %s%s"), *Text, *Key.GetFName().ToString());
	}
	}
	return FString();
}

namespace VibH2OCues
{
	bool OscAddressesEqual(const FString& A, const FString& B)
	{
		TArray<FString> SegmentsA;
		TArray<FString> SegmentsB;
		A.ParseIntoArray(SegmentsA, TEXT("/"), /*InCullEmpty=*/true);
		B.ParseIntoArray(SegmentsB, TEXT("/"), /*InCullEmpty=*/true);

		// An empty address is never a trigger: matching it against everything
		// would fire the cue on the whole control stream.
		if (SegmentsA.Num() == 0 || SegmentsA.Num() != SegmentsB.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < SegmentsA.Num(); ++Index)
		{
			if (!SegmentsA[Index].Equals(SegmentsB[Index], ESearchCase::IgnoreCase))
			{
				return false;
			}
		}
		return true;
	}

	bool MatchesTrigger(const FVibH2OCue& Cue, const FVibH2OCueInput& Input)
	{
		if (!Cue.bEnabled || Cue.Source != Input.Source)
		{
			return false;
		}

		switch (Cue.Source)
		{
		case EVibH2OCueSource::Osc:
			return OscAddressesEqual(Cue.OscAddress, Input.Address);

		case EVibH2OCueSource::MidiNote:
		case EVibH2OCueSource::MidiControlChange:
		case EVibH2OCueSource::MidiProgramChange:
			return Cue.MidiNumber == Input.Number
				&& (Cue.MidiChannel == 0 || Cue.MidiChannel == Input.Channel);

		case EVibH2OCueSource::Key:
			// Required modifiers must be held, extra ones are tolerated. A cue
			// with no modifier at all wants none held: Ctrl+1 must not also
			// fire a bare "1" cue.
			if (!Cue.Key.IsValid() || Cue.Key != Input.Key)
			{
				return false;
			}
			if (!Cue.bRequireCtrl && !Cue.bRequireShift && !Cue.bRequireAlt)
			{
				return !Input.bCtrl && !Input.bShift && !Input.bAlt;
			}
			return (!Cue.bRequireCtrl || Input.bCtrl)
				&& (!Cue.bRequireShift || Input.bShift)
				&& (!Cue.bRequireAlt || Input.bAlt);
		}
		return false;
	}

	bool PassesThreshold(const FVibH2OCue& Cue, const FVibH2OCueInput& Input)
	{
		if (Cue.bContinuous || !Input.bHasValue)
		{
			return true;
		}
		return Input.Value >= Cue.Threshold;
	}

	void ApplyLearnedInput(FVibH2OCue& Cue, const FVibH2OCueInput& Input)
	{
		Cue.Source = Input.Source;
		switch (Input.Source)
		{
		case EVibH2OCueSource::Osc:
			Cue.OscAddress = Input.Address;
			break;

		case EVibH2OCueSource::MidiNote:
		case EVibH2OCueSource::MidiControlChange:
		case EVibH2OCueSource::MidiProgramChange:
			Cue.MidiChannel = Input.Channel;
			Cue.MidiNumber = Input.Number;
			break;

		case EVibH2OCueSource::Key:
			Cue.Key = Input.Key;
			Cue.bRequireCtrl = Input.bCtrl;
			Cue.bRequireShift = Input.bShift;
			Cue.bRequireAlt = Input.bAlt;
			break;
		}
	}

	bool DecodeMidiShortMessage(uint32 Packed, FVibH2OCueInput& OutInput)
	{
		const uint8 Status = static_cast<uint8>(Packed & 0xFF);
		const uint8 Data1 = static_cast<uint8>((Packed >> 8) & 0x7F);
		const uint8 Data2 = static_cast<uint8>((Packed >> 16) & 0x7F);

		// Running status never reaches us - WinMM expands it - so a byte
		// without its high bit is garbage.
		if ((Status & 0x80) == 0)
		{
			return false;
		}

		OutInput = FVibH2OCueInput();
		OutInput.Channel = (Status & 0x0F) + 1;
		OutInput.Number = Data1;

		switch (Status & 0xF0)
		{
		case 0x90:
			if (Data2 == 0)
			{
				return false;
			}
			OutInput.Source = EVibH2OCueSource::MidiNote;
			OutInput.Value = Data2 / 127.0f;
			OutInput.bHasValue = true;
			return true;

		case 0xB0:
			OutInput.Source = EVibH2OCueSource::MidiControlChange;
			OutInput.Value = Data2 / 127.0f;
			OutInput.bHasValue = true;
			return true;

		case 0xC0:
			OutInput.Source = EVibH2OCueSource::MidiProgramChange;
			return true;

		default:
			return false;
		}
	}
}
