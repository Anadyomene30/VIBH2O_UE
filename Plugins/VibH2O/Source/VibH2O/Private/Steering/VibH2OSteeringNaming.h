// Control names for hand-placed steering actors.
#pragma once

#include "CoreMinimal.h"

namespace VibH2OSteeringNaming
{
	/**
	 * Turns an actor label into a name an OSC address can carry: letters,
	 * digits, '_' and '-' only, spaces turned into '_'. Accents are kept - they
	 * are letters - since the iPad sends UTF-8.
	 */
	inline FName SanitizeControlName(const FString& Raw)
	{
		FString Clean;
		Clean.Reserve(Raw.Len());
		for (const TCHAR Character : Raw)
		{
			if (FChar::IsAlnum(Character) || Character == TEXT('_') || Character == TEXT('-'))
			{
				Clean.AppendChar(Character);
			}
			else if (FChar::IsWhitespace(Character))
			{
				Clean.AppendChar(TEXT('_'));
			}
		}
		return Clean.IsEmpty() ? NAME_None : FName(*Clean);
	}
}
