#include "Osc/VibH2OOscTypes.h"

#include "Misc/DefaultValueHelper.h"

FVibH2OOscValue FVibH2OOscValue::MakeInt(int32 In)
{
	FVibH2OOscValue Value;
	Value.Type = EVibH2OOscType::Int32;
	Value.IntValue = In;
	Value.FloatValue = static_cast<float>(In);
	return Value;
}

FVibH2OOscValue FVibH2OOscValue::MakeFloat(float In)
{
	FVibH2OOscValue Value;
	Value.Type = EVibH2OOscType::Float32;
	Value.FloatValue = In;
	Value.IntValue = static_cast<int32>(In);
	return Value;
}

FVibH2OOscValue FVibH2OOscValue::MakeString(const FString& In)
{
	FVibH2OOscValue Value;
	Value.Type = EVibH2OOscType::String;
	Value.StringValue = In;
	return Value;
}

bool FVibH2OOscValue::IsNumeric() const
{
	switch (Type)
	{
	case EVibH2OOscType::Int32:
	case EVibH2OOscType::Float32:
	case EVibH2OOscType::True:
	case EVibH2OOscType::False:
		return true;
	case EVibH2OOscType::String:
		return FCString::IsNumeric(*StringValue);
	default:
		return false;
	}
}

float FVibH2OOscValue::AsFloat(float Fallback) const
{
	switch (Type)
	{
	case EVibH2OOscType::Int32:
		return static_cast<float>(IntValue);
	case EVibH2OOscType::Float32:
		return FloatValue;
	case EVibH2OOscType::True:
		return 1.0f;
	case EVibH2OOscType::False:
	case EVibH2OOscType::Nil:
		return 0.0f;
	case EVibH2OOscType::Impulse:
		return 1.0f;
	case EVibH2OOscType::String:
	{
		float Parsed = 0.0f;
		if (FDefaultValueHelper::ParseFloat(StringValue, Parsed))
		{
			return Parsed;
		}
		return Fallback;
	}
	default:
		return Fallback;
	}
}

int32 FVibH2OOscValue::AsInt(int32 Fallback) const
{
	switch (Type)
	{
	case EVibH2OOscType::Int32:
		return IntValue;
	case EVibH2OOscType::Float32:
		return FMath::RoundToInt(FloatValue);
	case EVibH2OOscType::True:
	case EVibH2OOscType::Impulse:
		return 1;
	case EVibH2OOscType::False:
	case EVibH2OOscType::Nil:
		return 0;
	case EVibH2OOscType::String:
	{
		int32 Parsed = 0;
		if (FDefaultValueHelper::ParseInt(StringValue, Parsed))
		{
			return Parsed;
		}
		float ParsedFloat = 0.0f;
		if (FDefaultValueHelper::ParseFloat(StringValue, ParsedFloat))
		{
			return FMath::RoundToInt(ParsedFloat);
		}
		return Fallback;
	}
	default:
		return Fallback;
	}
}

bool FVibH2OOscValue::AsBool(bool bFallback) const
{
	switch (Type)
	{
	case EVibH2OOscType::True:
	case EVibH2OOscType::Impulse:
		return true;
	case EVibH2OOscType::False:
	case EVibH2OOscType::Nil:
		return false;
	case EVibH2OOscType::Int32:
		return IntValue != 0;
	case EVibH2OOscType::Float32:
		return !FMath::IsNearlyZero(FloatValue);
	case EVibH2OOscType::String:
		if (StringValue.Equals(TEXT("true"), ESearchCase::IgnoreCase) || StringValue.Equals(TEXT("on"), ESearchCase::IgnoreCase))
		{
			return true;
		}
		if (StringValue.Equals(TEXT("false"), ESearchCase::IgnoreCase) || StringValue.Equals(TEXT("off"), ESearchCase::IgnoreCase))
		{
			return false;
		}
		return AsInt(bFallback ? 1 : 0) != 0;
	default:
		return bFallback;
	}
}

FString FVibH2OOscValue::AsString() const
{
	switch (Type)
	{
	case EVibH2OOscType::String:
		return StringValue;
	case EVibH2OOscType::Int32:
		return FString::FromInt(IntValue);
	case EVibH2OOscType::Float32:
		return FString::SanitizeFloat(FloatValue);
	case EVibH2OOscType::True:
		return TEXT("true");
	case EVibH2OOscType::False:
		return TEXT("false");
	case EVibH2OOscType::Nil:
		return TEXT("nil");
	case EVibH2OOscType::Impulse:
		return TEXT("impulse");
	case EVibH2OOscType::Blob:
		return FString::Printf(TEXT("<blob %d>"), BlobValue.Num());
	default:
		return TEXT("<unsupported>");
	}
}

FString FVibH2OOscValue::ToDebugString() const
{
	return AsString();
}

// ---------------------------------------------------------------------------

void FVibH2OOscMessage::GetSegments(TArray<FString>& OutSegments) const
{
	OutSegments.Reset();
	// ParseIntoArray with bCullEmpty = true settles pitfall 1 on its own: the
	// trailing slash yields an empty segment, which is simply discarded.
	Address.ParseIntoArray(OutSegments, TEXT("/"), /*InCullEmpty=*/true);
}

TArray<FString> FVibH2OOscMessage::GetSegments() const
{
	TArray<FString> Segments;
	GetSegments(Segments);
	return Segments;
}

bool FVibH2OOscMessage::MatchesPrefix(const FString& Prefix) const
{
	TArray<FString> PrefixSegments;
	Prefix.ParseIntoArray(PrefixSegments, TEXT("/"), /*InCullEmpty=*/true);
	if (PrefixSegments.Num() == 0)
	{
		return true;
	}

	TArray<FString> Segments;
	GetSegments(Segments);
	if (Segments.Num() < PrefixSegments.Num())
	{
		return false;
	}

	for (int32 Index = 0; Index < PrefixSegments.Num(); ++Index)
	{
		if (!Segments[Index].Equals(PrefixSegments[Index], ESearchCase::IgnoreCase))
		{
			return false;
		}
	}
	return true;
}

bool FVibH2OOscMessage::GetSegmentsAfterPrefix(const FString& Prefix, TArray<FString>& OutSegments) const
{
	OutSegments.Reset();

	TArray<FString> PrefixSegments;
	Prefix.ParseIntoArray(PrefixSegments, TEXT("/"), /*InCullEmpty=*/true);

	TArray<FString> Segments;
	GetSegments(Segments);
	if (Segments.Num() < PrefixSegments.Num())
	{
		return false;
	}

	for (int32 Index = 0; Index < PrefixSegments.Num(); ++Index)
	{
		if (!Segments[Index].Equals(PrefixSegments[Index], ESearchCase::IgnoreCase))
		{
			return false;
		}
	}

	for (int32 Index = PrefixSegments.Num(); Index < Segments.Num(); ++Index)
	{
		OutSegments.Add(Segments[Index]);
	}
	return true;
}

const FVibH2OOscValue* FVibH2OOscMessage::GetArg(int32 Index) const
{
	return Args.IsValidIndex(Index) ? &Args[Index] : nullptr;
}

float FVibH2OOscMessage::GetFloatArg(int32 Index, float Fallback) const
{
	const FVibH2OOscValue* Value = GetArg(Index);
	return Value ? Value->AsFloat(Fallback) : Fallback;
}

int32 FVibH2OOscMessage::GetIntArg(int32 Index, int32 Fallback) const
{
	const FVibH2OOscValue* Value = GetArg(Index);
	return Value ? Value->AsInt(Fallback) : Fallback;
}

FString FVibH2OOscMessage::GetStringArg(int32 Index, const FString& Fallback) const
{
	const FVibH2OOscValue* Value = GetArg(Index);
	return Value ? Value->AsString() : Fallback;
}

FString FVibH2OOscMessage::ToDebugString() const
{
	FString Result = Address;
	for (const FVibH2OOscValue& Arg : Args)
	{
		Result += TEXT(" ");
		Result += Arg.ToDebugString();
	}
	return Result;
}
