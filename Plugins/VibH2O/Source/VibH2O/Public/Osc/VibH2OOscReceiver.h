// OSC reception on a dedicated thread.
#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/ThreadSafeCounter.h"
#include "Containers/Queue.h"
#include "Osc/VibH2OOscTypes.h"

class FRunnableThread;
class FSocket;
class ISocketSubsystem;

/**
 * Listens on a UDP port, decodes OSC packets, and hands the messages over
 * through a single-producer, single-consumer queue.
 *
 * This class touches NO UObject at all, and that is the most important line in
 * the file. Reaching a UObject from a network thread produces a random crash -
 * which, statistically, means during a performance. The thread writes to the
 * queue, the game thread drains it; there is no other path.
 *
 * Decoding happens here rather than on the game thread: the parser is pure and
 * stateless, so it is safe off the main thread, and this spares the game thread
 * the cost of decoding several hundred messages per second.
 */
class VIBH2O_API FVibH2OOscReceiver : public FRunnable
{
public:
	FVibH2OOscReceiver();
	virtual ~FVibH2OOscReceiver() override;

	/**
	 * Opens the socket and starts the thread.
	 *
	 * @return false when the port is already taken or the address invalid. The
	 *         error is logged: a busy port is the number-one problem during
	 *         get-in, and it must never fail silently.
	 */
	bool Start(const FString& ListenAddress, int32 Port, int32 ReceiveBufferSize, int32 InMaxQueuedMessages);

	/** Stops the thread and closes the socket. Safe to call more than once. */
	void Shutdown();

	bool IsRunning() const { return bRunning; }

	/** Pops a message off the queue. Game thread only. */
	bool Dequeue(FVibH2OOscMessage& OutMessage);

	// Diagnostic counters, all atomic.
	int32 GetPacketsReceived() const { return PacketCounter.GetValue(); }
	int32 GetMessagesReceived() const { return MessageCounter.GetValue(); }
	int32 GetMessagesDropped() const { return DroppedCounter.GetValue(); }
	int32 GetMalformedPackets() const { return MalformedCounter.GetValue(); }
	int32 GetQueueDepth() const { return QueueDepth.GetValue(); }

	FString GetEndpointDescription() const { return EndpointDescription; }
	FString GetLastError() const;

	// FRunnable
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

private:
	void CloseSocket();

	FSocket* Socket = nullptr;
	ISocketSubsystem* SocketSubsystem = nullptr;
	FRunnableThread* Thread = nullptr;

	/** SPSC queue: the network thread writes, the game thread reads. */
	TQueue<FVibH2OOscMessage, EQueueMode::Spsc> Queue;

	FThreadSafeCounter StopCounter;
	FThreadSafeCounter PacketCounter;
	FThreadSafeCounter MessageCounter;
	FThreadSafeCounter DroppedCounter;
	FThreadSafeCounter MalformedCounter;

	/**
	 * Current queue depth. Without this guard, a stalled game thread - a shader
	 * compile, a level load - would let the queue grow without bound and
	 * eventually eat all available memory.
	 */
	FThreadSafeCounter QueueDepth;
	int32 MaxQueuedMessages = 65536;

	TArray<uint8> ReceiveBuffer;
	bool bRunning = false;
	FString EndpointDescription;

	mutable FCriticalSection ErrorLock;
	FString LastError;
};
