// OSC 1.0 value and message types. Deliberately engine-free (no UObject) so
// they stay testable against raw byte frames.
#pragma once

#include "CoreMinimal.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"

/** OSC argument types we recognise. */
enum class EVibH2OOscType : uint8
{
	Int32,
	Float32,
	String,
	Blob,
	/** 'T' - true, carries no payload in the message body. */
	True,
	/** 'F' - false, no payload. */
	False,
	/** 'N' - nil. */
	Nil,
	/** 'I' - impulse / bang. */
	Impulse,
	/** A tag recognised as valid but left uninterpreted (e.g. 'h', 'd', 't'). */
	Unsupported
};

/**
 * A single OSC argument.
 *
 * AsFloat / AsInt convert between numeric types, and even accept a numeric
 * string. Max may well send an integer where we expect a float, or the other
 * way round; refusing the conversion would drop live data for a purely formal
 * reason.
 */
struct VIBH2O_API FVibH2OOscValue
{
	EVibH2OOscType Type = EVibH2OOscType::Nil;

	int32 IntValue = 0;
	float FloatValue = 0.0f;
	FString StringValue;
	TArray<uint8> BlobValue;

	FVibH2OOscValue() = default;

	static FVibH2OOscValue MakeInt(int32 In);
	static FVibH2OOscValue MakeFloat(float In);
	static FVibH2OOscValue MakeString(const FString& In);

	bool IsNumeric() const;

	float AsFloat(float Fallback = 0.0f) const;
	int32 AsInt(int32 Fallback = 0) const;
	bool AsBool(bool bFallback = false) const;
	FString AsString() const;

	FString ToDebugString() const;
};

/**
 * An OSC message: an address plus typed arguments.
 *
 * The address is kept exactly as received, trailing slash included. Splitting
 * happens in GetSegments(), which drops empty segments - see pitfall 1 in
 * docs/OSC_PROTOCOL.md: Max emits "/RoomMapping/columns/", not
 * "/RoomMapping/columns".
 */
struct VIBH2O_API FVibH2OOscMessage
{
	FString Address;
	TArray<FVibH2OOscValue> Args;

	/** Time tag of the enclosing bundle, or 0 for "immediate". Unused for now. */
	uint64 TimeTag = 0;

	FVibH2OOscMessage() = default;
	explicit FVibH2OOscMessage(const FString& InAddress) : Address(InAddress) {}

	/** Splits the address into non-empty segments. "/A//B/" -> ["A", "B"]. */
	void GetSegments(TArray<FString>& OutSegments) const;
	TArray<FString> GetSegments() const;

	/**
	 * True when the address starts with the given prefix on a segment
	 * boundary. "/BPM" matches "/BPM/12/" but not "/BPMX/12/". Stray slashes
	 * on either side are ignored.
	 */
	bool MatchesPrefix(const FString& Prefix) const;

	/**
	 * Segments left after the prefix. For "/BPM/12/" with prefix "/BPM" this
	 * yields ["12"]. Returns false when the prefix does not match.
	 */
	bool GetSegmentsAfterPrefix(const FString& Prefix, TArray<FString>& OutSegments) const;

	const FVibH2OOscValue* GetArg(int32 Index) const;
	float GetFloatArg(int32 Index, float Fallback = 0.0f) const;
	int32 GetIntArg(int32 Index, int32 Fallback = 0) const;
	FString GetStringArg(int32 Index, const FString& Fallback = FString()) const;

	FString ToDebugString() const;
};
