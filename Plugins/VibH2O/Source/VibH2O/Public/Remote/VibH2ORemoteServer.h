// The iPad remote: a minimal HTTP server, on its own thread.
//
// Written here for the same reason as the OSC parser: Epic's HTTP server and
// WebSocket modules shift between engine versions, and the plugin must build
// unchanged on 5.5 and 5.8. What the remote needs is small - serve a handful
// of files, hand back a state snapshot, accept a few lines of commands - and a
// plain TCP socket does it.
//
// Commands arrive as OSC-style lines ("/Attractor/Gauche/ 1") and leave as
// FVibH2OOscMessage through a queue, so the iPad, Max and TouchOSC all reach
// the very same control path on the game thread.
#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "HAL/Runnable.h"
#include "HAL/ThreadSafeCounter.h"
#include "Osc/VibH2OOscTypes.h"

class FRunnableThread;
class FSocket;
class ISocketSubsystem;

/**
 * Serves the remote's page and its two API routes:
 *
 *   GET  /              the page and its files, from the web root
 *   GET  /api/state     the latest state snapshot, as JSON
 *   POST /api/cmd       one command per line, OSC style
 *
 * Like the OSC receiver, this class touches NO UObject. The game thread pushes
 * the state in through SetStateJson and pulls the commands out with Dequeue;
 * nothing else crosses over.
 */
class VIBH2O_API FVibH2ORemoteServer : public FRunnable
{
public:
	FVibH2ORemoteServer();
	virtual ~FVibH2ORemoteServer() override;

	/**
	 * Loads the web root and starts listening. Every file is read into memory
	 * here, on the calling thread: the server thread never touches the disk,
	 * and a request can only ever reach a file that was there at start-up.
	 */
	bool Start(const FString& ListenAddress, int32 Port, const FString& WebRoot);

	/** Stops the thread and closes every socket. Safe to call more than once. */
	void Shutdown();

	bool IsRunning() const { return bRunning; }
	int32 GetPort() const { return BoundPort; }
	FString GetLastError() const;
	int32 GetRequestCount() const { return RequestCounter.GetValue(); }
	int32 GetCommandCount() const { return CommandCounter.GetValue(); }
	int32 GetFileCount() const { return Files.Num(); }

	/** Pops a command. Game thread only. */
	bool Dequeue(FVibH2OOscMessage& OutMessage);

	/** Replaces the snapshot GET /api/state returns. Game thread. */
	void SetStateJson(const FString& Json);

	// FRunnable
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

	// -------------------------------------------------- Pure, for the tests

	/**
	 * Reads one command line: an address, then arguments separated by spaces.
	 * Integers become 'i', other numbers 'f', anything else 's'. False for a
	 * blank line or one that does not start with '/'.
	 */
	static bool ParseCommandLine(const FString& Line, FVibH2OOscMessage& OutMessage);

	/**
	 * Reads the request line and the Content-Length of an HTTP head (the text
	 * before the blank line). The path loses its query string.
	 */
	static bool ParseRequestHead(const FString& Head, FString& OutMethod, FString& OutPath, int32& OutContentLength);

	/** The MIME type the remote's files are served with. */
	static FString ContentTypeFor(const FString& FileName);

private:
	struct FClient
	{
		FSocket* Socket = nullptr;
		TArray<uint8> Buffer;
		double StartTime = 0.0;
	};

	struct FServedFile
	{
		TArray<uint8> Bytes;
		FString ContentType;
	};

	void AcceptClients();

	/** Returns true when the client is finished with and may be closed. */
	bool ServiceClient(FClient& Client);

	void HandleRequest(FClient& Client, const FString& Method, const FString& Path, const TArray<uint8>& Body);
	void SendResponse(FSocket* Socket, int32 Status, const FString& ContentType, const TArray<uint8>& Body, bool bNoCache);
	void SendText(FSocket* Socket, int32 Status, const FString& Text);
	void CloseClient(FClient& Client);
	void CloseAll();

	void SetError(const FString& Message);

	FSocket* ListenSocket = nullptr;
	ISocketSubsystem* SocketSubsystem = nullptr;
	FRunnableThread* Thread = nullptr;

	/** Server thread only. */
	TArray<FClient> Clients;

	/** Read-only once Start() has returned. */
	TMap<FString, FServedFile> Files;

	TQueue<FVibH2OOscMessage, EQueueMode::Spsc> Queue;
	FThreadSafeCounter QueueDepth;

	mutable FCriticalSection StateLock;
	TArray<uint8> StateUtf8;

	FThreadSafeCounter StopCounter;
	FThreadSafeCounter RequestCounter;
	FThreadSafeCounter CommandCounter;

	bool bRunning = false;
	int32 BoundPort = 0;

	mutable FCriticalSection ErrorLock;
	FString LastError;
};
