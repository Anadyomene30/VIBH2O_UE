// OSC 1.0 parser. Engine-free, so it can be tested against raw byte frames.
// See docs/OSC_PROTOCOL.md, section 4.
#pragma once

#include "CoreMinimal.h"
#include "Osc/VibH2OOscTypes.h"

/**
 * OSC 1.0 packet decoder.
 *
 * The contract, and it is not negotiable: no frame whatsoever may cause a
 * crash or an out-of-bounds read. During a performance a lost or truncated
 * packet must not take the show down with it. The parser therefore returns
 * false and drops the frame rather than assuming anything.
 */
class VIBH2O_API FVibH2OOscParser
{
public:
	/** Maximum bundle nesting depth. Guards against runaway recursion. */
	static constexpr int32 MaxBundleDepth = 8;

	/** Maximum number of messages extracted from a single packet. */
	static constexpr int32 MaxMessagesPerPacket = 8192;

	/**
	 * Decodes a whole UDP packet (single message or bundle) and appends the
	 * messages it contains to OutMessages.
	 *
	 * @return true when the packet was decoded in full. false when the frame is
	 *         truncated or malformed - OutMessages may then still hold the
	 *         valid messages already extracted, and that is deliberate: in a
	 *         partially received bundle we may as well keep what is readable.
	 */
	static bool ParsePacket(const uint8* Data, int32 Size, TArray<FVibH2OOscMessage>& OutMessages);

	/** Convenience overload. */
	static bool ParsePacket(const TArray<uint8>& Data, TArray<FVibH2OOscMessage>& OutMessages);

private:
	static bool ParseElement(const uint8* Data, int32 Size, int32 Depth, uint64 TimeTag, TArray<FVibH2OOscMessage>& OutMessages);
	static bool ParseBundle(const uint8* Data, int32 Size, int32 Depth, TArray<FVibH2OOscMessage>& OutMessages);
	static bool ParseMessage(const uint8* Data, int32 Size, uint64 TimeTag, FVibH2OOscMessage& OutMessage);
};
