#include "Osc/VibH2OOscParser.h"

#include "VibH2OModule.h"

namespace VibH2OOscParserInternal
{
	/**
	 * Bounds-checked read cursor. Every read verifies it fits inside the buffer
	 * before touching a byte: that is what makes the parser immune to truncated
	 * frames.
	 */
	struct FReader
	{
		const uint8* Data = nullptr;
		int32 Size = 0;
		int32 Offset = 0;

		FReader(const uint8* InData, int32 InSize) : Data(InData), Size(InSize) {}

		int32 Remaining() const { return Size - Offset; }
		bool CanRead(int32 Count) const { return Count >= 0 && Remaining() >= Count; }

		/** Advances the cursor to the next 4-byte boundary. */
		bool Align4()
		{
			const int32 Padding = (4 - (Offset % 4)) % 4;
			if (!CanRead(Padding))
			{
				return false;
			}
			Offset += Padding;
			return true;
		}

		bool ReadInt32(int32& Out)
		{
			if (!CanRead(4))
			{
				return false;
			}
			// OSC is big-endian. Assembling by hand rather than calling a
			// conditional ByteSwap keeps this correct on every platform without
			// depending on an endianness macro.
			const uint8* P = Data + Offset;
			const uint32 Raw = (static_cast<uint32>(P[0]) << 24)
				| (static_cast<uint32>(P[1]) << 16)
				| (static_cast<uint32>(P[2]) << 8)
				| static_cast<uint32>(P[3]);
			Offset += 4;
			Out = static_cast<int32>(Raw);
			return true;
		}

		bool ReadUInt64(uint64& Out)
		{
			if (!CanRead(8))
			{
				return false;
			}
			const uint8* P = Data + Offset;
			uint64 Raw = 0;
			for (int32 Index = 0; Index < 8; ++Index)
			{
				Raw = (Raw << 8) | static_cast<uint64>(P[Index]);
			}
			Offset += 8;
			Out = Raw;
			return true;
		}

		bool ReadFloat32(float& Out)
		{
			int32 Raw = 0;
			if (!ReadInt32(Raw))
			{
				return false;
			}
			// Bit reinterpretation through memcpy: the only standard-defined way
			// to read 4 bytes back as a float without breaking strict aliasing.
			float Value = 0.0f;
			FMemory::Memcpy(&Value, &Raw, sizeof(float));
			Out = Value;
			return true;
		}

		/** OSC string: null-terminated, then padded to a multiple of 4 bytes. */
		bool ReadString(FString& Out)
		{
			const int32 Start = Offset;
			int32 Cursor = Offset;
			while (Cursor < Size && Data[Cursor] != 0)
			{
				++Cursor;
			}
			if (Cursor >= Size)
			{
				// No terminating null inside the buffer: the frame is truncated.
				return false;
			}

			const int32 Length = Cursor - Start;
			if (Length == 0)
			{
				Out.Reset();
			}
			else
			{
				// Data[Cursor] is 0 and lies within bounds, so the pointer is a
				// genuinely valid C string.
				Out = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Data + Start)));
			}

			// Skip the content plus the null, then align to 4.
			Offset = Cursor + 1;
			return Align4();
		}

		bool ReadBlob(TArray<uint8>& Out)
		{
			int32 BlobSize = 0;
			if (!ReadInt32(BlobSize))
			{
				return false;
			}
			if (BlobSize < 0 || !CanRead(BlobSize))
			{
				return false;
			}
			Out.Reset(BlobSize);
			Out.Append(Data + Offset, BlobSize);
			Offset += BlobSize;
			return Align4();
		}
	};

	bool IsBundleHeader(const uint8* Data, int32 Size)
	{
		static const char Marker[] = "#bundle";
		if (Size < 8)
		{
			return false;
		}
		for (int32 Index = 0; Index < 7; ++Index)
		{
			if (Data[Index] != static_cast<uint8>(Marker[Index]))
			{
				return false;
			}
		}
		return Data[7] == 0;
	}
}

bool FVibH2OOscParser::ParsePacket(const uint8* Data, int32 Size, TArray<FVibH2OOscMessage>& OutMessages)
{
	if (Data == nullptr || Size <= 0)
	{
		return false;
	}
	return ParseElement(Data, Size, /*Depth=*/0, /*TimeTag=*/0, OutMessages);
}

bool FVibH2OOscParser::ParsePacket(const TArray<uint8>& Data, TArray<FVibH2OOscMessage>& OutMessages)
{
	return ParsePacket(Data.GetData(), Data.Num(), OutMessages);
}

bool FVibH2OOscParser::ParseElement(const uint8* Data, int32 Size, int32 Depth, uint64 TimeTag, TArray<FVibH2OOscMessage>& OutMessages)
{
	if (Size <= 0)
	{
		return false;
	}

	if (VibH2OOscParserInternal::IsBundleHeader(Data, Size))
	{
		if (Depth >= MaxBundleDepth)
		{
			// Endlessly nested bundles: refuse rather than unwind the call
			// stack.
			return false;
		}
		return ParseBundle(Data, Size, Depth, OutMessages);
	}

	// An OSC message necessarily starts with '/'. Anything else is network
	// noise, silently discarded.
	if (Data[0] != static_cast<uint8>('/'))
	{
		return false;
	}

	if (OutMessages.Num() >= MaxMessagesPerPacket)
	{
		return false;
	}

	FVibH2OOscMessage Message;
	if (!ParseMessage(Data, Size, TimeTag, Message))
	{
		return false;
	}

	OutMessages.Add(MoveTemp(Message));
	return true;
}

bool FVibH2OOscParser::ParseBundle(const uint8* Data, int32 Size, int32 Depth, TArray<FVibH2OOscMessage>& OutMessages)
{
	using namespace VibH2OOscParserInternal;

	FReader Reader(Data, Size);

	FString Marker;
	if (!Reader.ReadString(Marker))
	{
		return false;
	}

	uint64 TimeTag = 0;
	if (!Reader.ReadUInt64(TimeTag))
	{
		return false;
	}

	bool bAllOk = true;
	while (Reader.Remaining() > 0)
	{
		int32 ElementSize = 0;
		if (!Reader.ReadInt32(ElementSize))
		{
			return false;
		}
		if (ElementSize <= 0 || !Reader.CanRead(ElementSize))
		{
			// Declared size inconsistent with what remains: truncated bundle.
			return false;
		}

		// Messages already extracted stay in OutMessages: in a bundle cut short
		// in flight we may as well keep what was readable.
		if (!ParseElement(Data + Reader.Offset, ElementSize, Depth + 1, TimeTag, OutMessages))
		{
			bAllOk = false;
		}
		Reader.Offset += ElementSize;
	}

	return bAllOk;
}

bool FVibH2OOscParser::ParseMessage(const uint8* Data, int32 Size, uint64 TimeTag, FVibH2OOscMessage& OutMessage)
{
	using namespace VibH2OOscParserInternal;

	FReader Reader(Data, Size);

	if (!Reader.ReadString(OutMessage.Address))
	{
		return false;
	}
	if (OutMessage.Address.IsEmpty() || OutMessage.Address[0] != TEXT('/'))
	{
		return false;
	}
	OutMessage.TimeTag = TimeTag;

	if (Reader.Remaining() <= 0)
	{
		// Message with no type-tag string. The standard makes it mandatory, but
		// a few senders omit it when there are no arguments. An argument-less
		// message is still usable, so accept it.
		return true;
	}

	FString TypeTags;
	if (!Reader.ReadString(TypeTags))
	{
		return false;
	}
	if (TypeTags.IsEmpty() || TypeTags[0] != TEXT(','))
	{
		// Type-tag string missing or malformed: return the message without
		// arguments rather than guessing how they are laid out.
		return true;
	}

	const int32 TagCount = TypeTags.Len() - 1;
	OutMessage.Args.Reserve(TagCount);

	for (int32 TagIndex = 1; TagIndex <= TagCount; ++TagIndex)
	{
		const TCHAR Tag = TypeTags[TagIndex];
		FVibH2OOscValue Value;

		switch (Tag)
		{
		case TEXT('i'):
		{
			int32 Raw = 0;
			if (!Reader.ReadInt32(Raw))
			{
				return false;
			}
			Value = FVibH2OOscValue::MakeInt(Raw);
			break;
		}
		case TEXT('f'):
		{
			float Raw = 0.0f;
			if (!Reader.ReadFloat32(Raw))
			{
				return false;
			}
			Value = FVibH2OOscValue::MakeFloat(Raw);
			break;
		}
		case TEXT('s'):
		case TEXT('S'):
		{
			FString Raw;
			if (!Reader.ReadString(Raw))
			{
				return false;
			}
			Value = FVibH2OOscValue::MakeString(Raw);
			break;
		}
		case TEXT('b'):
		{
			Value.Type = EVibH2OOscType::Blob;
			if (!Reader.ReadBlob(Value.BlobValue))
			{
				return false;
			}
			break;
		}
		case TEXT('T'):
			Value.Type = EVibH2OOscType::True;
			Value.IntValue = 1;
			Value.FloatValue = 1.0f;
			break;
		case TEXT('F'):
			Value.Type = EVibH2OOscType::False;
			break;
		case TEXT('N'):
			Value.Type = EVibH2OOscType::Nil;
			break;
		case TEXT('I'):
			Value.Type = EVibH2OOscType::Impulse;
			Value.IntValue = 1;
			Value.FloatValue = 1.0f;
			break;
		case TEXT('h'):
		case TEXT('t'):
		case TEXT('d'):
		{
			// 64-bit standard types. We do not interpret them, but their 8 bytes
			// must be consumed or everything after them misaligns.
			uint64 Raw = 0;
			if (!Reader.ReadUInt64(Raw))
			{
				return false;
			}
			Value.Type = EVibH2OOscType::Unsupported;
			break;
		}
		case TEXT('c'):
		case TEXT('r'):
		case TEXT('m'):
		{
			int32 Raw = 0;
			if (!Reader.ReadInt32(Raw))
			{
				return false;
			}
			Value.Type = EVibH2OOscType::Unsupported;
			Value.IntValue = Raw;
			break;
		}
		case TEXT('['):
		case TEXT(']'):
			// Array markers: no payload in the message body.
			continue;
		default:
			// Unknown tag: there is no way to know how many bytes it consumes,
			// so no way to read on with confidence. Stop here, keeping the
			// arguments decoded so far.
			UE_LOG(LogVibH2O, Verbose, TEXT("OSC : tag de type inconnu '%c' dans %s"), Tag, *OutMessage.Address);
			return true;
		}

		OutMessage.Args.Add(MoveTemp(Value));
	}

	return true;
}
