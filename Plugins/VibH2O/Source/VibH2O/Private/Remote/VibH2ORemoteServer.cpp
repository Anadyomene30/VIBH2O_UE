#include "Remote/VibH2ORemoteServer.h"

#include "Common/TcpSocketBuilder.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/RunnableThread.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SocketSubsystem.h"
#include "Sockets.h"
#include "VibH2OModule.h"

namespace VibH2ORemoteInternal
{
	/** Wait on the listen socket before re-checking the stop request. */
	static constexpr int32 IdleWaitMilliseconds = 50;

	/** A client that has not finished its request by then is dropped. */
	static constexpr double ClientTimeoutSeconds = 5.0;

	/** Bigger than any request the remote sends; anything above is refused. */
	static constexpr int32 MaxRequestBytes = 256 * 1024;

	/** A bound on simultaneous connections: one iPad opens a handful. */
	static constexpr int32 MaxClients = 32;

	/** Commands waiting for the game thread, at most. */
	static constexpr int32 MaxQueuedCommands = 4096;

	static const TCHAR* StatusText(int32 Status)
	{
		switch (Status)
		{
		case 200: return TEXT("OK");
		case 400: return TEXT("Bad Request");
		case 404: return TEXT("Not Found");
		case 405: return TEXT("Method Not Allowed");
		case 413: return TEXT("Payload Too Large");
		case 503: return TEXT("Service Unavailable");
		default: return TEXT("Error");
		}
	}

	static void AppendUtf8(TArray<uint8>& Out, const FString& Text)
	{
		const FTCHARToUTF8 Converted(*Text);
		Out.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
	}

	static FString Utf8ToString(const uint8* Bytes, int32 Count)
	{
		if (Count <= 0)
		{
			return FString();
		}
		const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Bytes), Count);
		return FString(Converted.Length(), Converted.Get());
	}

	/** Index of the blank line ending an HTTP head, or INDEX_NONE. */
	static int32 FindHeadEnd(const TArray<uint8>& Buffer)
	{
		for (int32 Index = 0; Index + 3 < Buffer.Num(); ++Index)
		{
			if (Buffer[Index] == '\r' && Buffer[Index + 1] == '\n' && Buffer[Index + 2] == '\r' && Buffer[Index + 3] == '\n')
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	/** Shown when the page's files did not ship: says what is missing and where. */
	static const TCHAR* MissingPage = TEXT(
		"<!doctype html><meta charset=utf-8><meta name=viewport content='width=device-width'>"
		"<body style='background:#000;color:#c9a;font:18px system-ui;padding:2em'>"
		"<h1>VibH2O</h1><p>Le serveur tourne, mais la page de la t&eacute;l&eacute;commande est introuvable.</p>"
		"<p>Elle doit se trouver dans <code>Plugins/VibH2O/Resources/Remote/</code>. Voir le log Unreal.</p>");
}

FVibH2ORemoteServer::FVibH2ORemoteServer() = default;

FVibH2ORemoteServer::~FVibH2ORemoteServer()
{
	Shutdown();
}

FString FVibH2ORemoteServer::GetLastError() const
{
	FScopeLock Lock(&ErrorLock);
	return LastError;
}

void FVibH2ORemoteServer::SetError(const FString& Message)
{
	{
		FScopeLock Lock(&ErrorLock);
		LastError = Message;
	}
	UE_LOG(LogVibH2O, Error, TEXT("VibH2O: %s"), *Message);
}

// ------------------------------------------------------------------ Lifetime

bool FVibH2ORemoteServer::Start(const FString& ListenAddress, int32 Port, const FString& WebRoot)
{
	using namespace VibH2ORemoteInternal;

	Shutdown();

	SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (SocketSubsystem == nullptr)
	{
		SetError(TEXT("Aucun sous-systeme de socket disponible pour la telecommande."));
		return false;
	}

	// Every file is read now, once. The page can be edited on site between two
	// runs - VibH2O.Restart reloads it - but the server thread never opens a
	// file, and no path in a request can reach outside this list.
	Files.Reset();
	TArray<FString> Names;
	IFileManager::Get().FindFiles(Names, *FPaths::Combine(WebRoot, TEXT("*")), /*Files=*/true, /*Directories=*/false);
	for (const FString& Name : Names)
	{
		FServedFile File;
		if (FFileHelper::LoadFileToArray(File.Bytes, *FPaths::Combine(WebRoot, Name)))
		{
			File.ContentType = ContentTypeFor(Name);
			Files.Add(Name.ToLower(), MoveTemp(File));
		}
	}
	if (!Files.Contains(TEXT("index.html")))
	{
		UE_LOG(LogVibH2O, Warning,
			TEXT("VibH2O: page de la telecommande introuvable dans '%s' — une page d'erreur sera servie a la place."), *WebRoot);
		FServedFile Fallback;
		AppendUtf8(Fallback.Bytes, MissingPage);
		Fallback.ContentType = ContentTypeFor(TEXT("index.html"));
		Files.Add(TEXT("index.html"), MoveTemp(Fallback));
	}

	FIPv4Address Address;
	if (!FIPv4Address::Parse(ListenAddress, Address))
	{
		UE_LOG(LogVibH2O, Warning,
			TEXT("VibH2O: adresse d'ecoute de la telecommande '%s' invalide, repli sur 0.0.0.0."), *ListenAddress);
		Address = FIPv4Address::Any;
	}
	BoundPort = FMath::Clamp(Port, 1, 65535);
	const FIPv4Endpoint Endpoint(Address, static_cast<uint16>(BoundPort));

	// Windows lets a program listen on 0.0.0.0 while ANOTHER one already holds
	// the same port on 127.0.0.1 - Logitech G HUB does exactly that on 9080.
	// Nothing fails: the iPad reaches us over Wi-Fi, but anything on this PC
	// asking localhost talks to the other program, and the remote looks dead
	// from here. Probe the loopback first, so the conflict is named, not
	// guessed at.
	if (Address == FIPv4Address::Any)
	{
		FSocket* Probe = FTcpSocketBuilder(TEXT("VibH2ORemoteProbe"))
			.BoundToEndpoint(FIPv4Endpoint(FIPv4Address(127, 0, 0, 1), static_cast<uint16>(BoundPort)))
			.Build();
		if (Probe != nullptr)
		{
			Probe->Close();
			SocketSubsystem->DestroySocket(Probe);
		}
		else
		{
			UE_LOG(LogVibH2O, Warning,
				TEXT("VibH2O: un autre logiciel ecoute deja le port %d sur 127.0.0.1. L'iPad joindra la telecommande par le Wi-Fi, ")
				TEXT("mais http://127.0.0.1:%d/ sur ce PC tombera sur l'autre logiciel. Changer le port dans Project Settings > VibH2O."),
				BoundPort, BoundPort);
		}
	}

	// Not reusable, on purpose: on Windows a reusable TCP port can be bound
	// twice, and a second program on the same port would then go unnoticed
	// instead of failing loudly at get-in.
	ListenSocket = FTcpSocketBuilder(TEXT("VibH2ORemote"))
		.AsNonBlocking()
		.BoundToEndpoint(Endpoint)
		.Listening(16)
		.Build();

	if (ListenSocket == nullptr)
	{
		SetError(FString::Printf(
			TEXT("Impossible d'ouvrir la telecommande sur %s. Le port est-il deja utilise par un autre logiciel ?"),
			*Endpoint.ToString()));
		return false;
	}

	{
		FScopeLock Lock(&ErrorLock);
		LastError.Reset();
	}
	StopCounter.Reset();
	RequestCounter.Reset();
	CommandCounter.Reset();
	QueueDepth.Reset();

	Thread = FRunnableThread::Create(this, TEXT("VibH2ORemoteServer"), 0, TPri_Normal);
	if (Thread == nullptr)
	{
		SocketSubsystem->DestroySocket(ListenSocket);
		ListenSocket = nullptr;
		SetError(TEXT("Impossible de creer le thread de la telecommande."));
		return false;
	}

	bRunning = true;
	UE_LOG(LogVibH2O, Log, TEXT("VibH2O: telecommande demarree sur %s (%d fichiers)."), *Endpoint.ToString(), Files.Num());
	return true;
}

void FVibH2ORemoteServer::Shutdown()
{
	if (Thread != nullptr)
	{
		StopCounter.Increment();
		// Waits for Run() to return: no queue write outlives this object.
		Thread->Kill(true);
		delete Thread;
		Thread = nullptr;
	}

	if (ListenSocket != nullptr && SocketSubsystem != nullptr)
	{
		ListenSocket->Close();
		SocketSubsystem->DestroySocket(ListenSocket);
	}
	ListenSocket = nullptr;

	FVibH2OOscMessage Discard;
	while (Queue.Dequeue(Discard))
	{
	}
	QueueDepth.Reset();
	bRunning = false;
}

bool FVibH2ORemoteServer::Init()
{
	return ListenSocket != nullptr;
}

void FVibH2ORemoteServer::Stop()
{
	StopCounter.Increment();
}

void FVibH2ORemoteServer::Exit()
{
}

// ------------------------------------------------------------- Game thread

bool FVibH2ORemoteServer::Dequeue(FVibH2OOscMessage& OutMessage)
{
	if (Queue.Dequeue(OutMessage))
	{
		QueueDepth.Decrement();
		return true;
	}
	return false;
}

void FVibH2ORemoteServer::SetStateJson(const FString& Json)
{
	TArray<uint8> Bytes;
	VibH2ORemoteInternal::AppendUtf8(Bytes, Json);

	FScopeLock Lock(&StateLock);
	StateUtf8 = MoveTemp(Bytes);
}

// ------------------------------------------------------------ Server thread

uint32 FVibH2ORemoteServer::Run()
{
	using namespace VibH2ORemoteInternal;

	while (StopCounter.GetValue() == 0)
	{
		AcceptClients();

		for (int32 Index = Clients.Num() - 1; Index >= 0; --Index)
		{
			if (ServiceClient(Clients[Index]))
			{
				CloseClient(Clients[Index]);
				Clients.RemoveAtSwap(Index);
			}
		}

		if (Clients.Num() == 0)
		{
			// Nothing in flight: sleep on the listen socket itself, so a new
			// connection wakes us at once.
			bool bPending = false;
			ListenSocket->WaitForPendingConnection(bPending, FTimespan::FromMilliseconds(IdleWaitMilliseconds));
		}
		else
		{
			FPlatformProcess::Sleep(0.002f);
		}
	}

	CloseAll();
	return 0;
}

void FVibH2ORemoteServer::AcceptClients()
{
	bool bPending = false;
	while (ListenSocket->HasPendingConnection(bPending) && bPending)
	{
		FSocket* Accepted = ListenSocket->Accept(TEXT("VibH2ORemoteClient"));
		if (Accepted == nullptr)
		{
			break;
		}
		if (Clients.Num() >= VibH2ORemoteInternal::MaxClients)
		{
			Accepted->Close();
			SocketSubsystem->DestroySocket(Accepted);
			continue;
		}

		// Each connection is polled, never waited on: Safari opens spare
		// connections it may never use, and a blocking read on one of those
		// would freeze the remote for everyone else.
		Accepted->SetNonBlocking(true);
		Accepted->SetNoDelay(true);

		FClient Client;
		Client.Socket = Accepted;
		Client.StartTime = FPlatformTime::Seconds();
		Clients.Add(MoveTemp(Client));
	}
}

bool FVibH2ORemoteServer::ServiceClient(FClient& Client)
{
	using namespace VibH2ORemoteInternal;

	uint32 PendingBytes = 0;
	while (Client.Socket->HasPendingData(PendingBytes) && PendingBytes > 0)
	{
		const int32 Offset = Client.Buffer.Num();
		const int32 Chunk = FMath::Min<int32>(static_cast<int32>(PendingBytes), 64 * 1024);
		Client.Buffer.SetNumUninitialized(Offset + Chunk);

		int32 Read = 0;
		if (!Client.Socket->Recv(Client.Buffer.GetData() + Offset, Chunk, Read) || Read <= 0)
		{
			Client.Buffer.SetNum(Offset);
			break;
		}
		Client.Buffer.SetNum(Offset + Read);

		if (Client.Buffer.Num() > MaxRequestBytes)
		{
			SendText(Client.Socket, 413, TEXT("trop long"));
			return true;
		}
	}

	const bool bTimedOut = (FPlatformTime::Seconds() - Client.StartTime) > ClientTimeoutSeconds;

	const int32 HeadEnd = FindHeadEnd(Client.Buffer);
	if (HeadEnd == INDEX_NONE)
	{
		return bTimedOut || Client.Socket->GetConnectionState() == SCS_ConnectionError;
	}

	FString Method;
	FString Path;
	int32 ContentLength = 0;
	const FString Head = Utf8ToString(Client.Buffer.GetData(), HeadEnd);
	if (!ParseRequestHead(Head, Method, Path, ContentLength))
	{
		SendText(Client.Socket, 400, TEXT("requete illisible"));
		return true;
	}
	if (ContentLength > MaxRequestBytes)
	{
		SendText(Client.Socket, 413, TEXT("trop long"));
		return true;
	}

	// Commands need a header of our own. A browser cannot add one to a
	// cross-site request without a preflight this server never answers: a
	// web page open on another machine of the show network cannot drive the
	// room. The remote's own page sends it.
	if (Path == TEXT("/api/cmd") && !Head.Contains(TEXT("\nX-VibH2O:"), ESearchCase::IgnoreCase))
	{
		SendText(Client.Socket, 403, TEXT("en-tete X-VibH2O attendu"));
		return true;
	}

	const int32 BodyStart = HeadEnd + 4;
	if (Client.Buffer.Num() < BodyStart + ContentLength)
	{
		// The body is still on its way.
		return bTimedOut;
	}

	TArray<uint8> Body;
	Body.Append(Client.Buffer.GetData() + BodyStart, ContentLength);

	RequestCounter.Increment();
	HandleRequest(Client, Method, Path, Body);
	return true;
}

void FVibH2ORemoteServer::HandleRequest(FClient& Client, const FString& Method, const FString& Path, const TArray<uint8>& Body)
{
	using namespace VibH2ORemoteInternal;

	const bool bGet = Method == TEXT("GET") || Method == TEXT("HEAD");

	if (Path == TEXT("/api/state"))
	{
		TArray<uint8> Snapshot;
		{
			FScopeLock Lock(&StateLock);
			Snapshot = StateUtf8;
		}
		if (Snapshot.Num() == 0)
		{
			AppendUtf8(Snapshot, TEXT("{}"));
		}
		SendResponse(Client.Socket, 200, TEXT("application/json; charset=utf-8"), Snapshot, true);
		return;
	}

	if (Path == TEXT("/api/cmd"))
	{
		if (Method != TEXT("POST"))
		{
			SendText(Client.Socket, 405, TEXT("POST attendu"));
			return;
		}

		TArray<FString> Lines;
		Utf8ToString(Body.GetData(), Body.Num()).ParseIntoArrayLines(Lines, /*InCullEmpty=*/true);

		int32 Accepted = 0;
		for (const FString& Line : Lines)
		{
			FVibH2OOscMessage Message;
			if (!ParseCommandLine(Line, Message))
			{
				continue;
			}
			if (QueueDepth.GetValue() >= MaxQueuedCommands)
			{
				// The game thread is stalled - a level load, a shader compile.
				// Refuse rather than pile up stale drags.
				SendText(Client.Socket, 503, TEXT("occupe"));
				return;
			}
			Queue.Enqueue(MoveTemp(Message));
			QueueDepth.Increment();
			CommandCounter.Increment();
			++Accepted;
		}
		SendText(Client.Socket, 200, FString::Printf(TEXT("ok %d"), Accepted));
		return;
	}

	if (!bGet)
	{
		SendText(Client.Socket, 405, TEXT("methode non prise en charge"));
		return;
	}

	if (Path == TEXT("/api/ping"))
	{
		SendText(Client.Socket, 200, TEXT("ok"));
		return;
	}

	FString FileName = Path;
	FileName.RemoveFromStart(TEXT("/"));
	if (FileName.IsEmpty())
	{
		FileName = TEXT("index.html");
	}

	if (const FServedFile* File = Files.Find(FileName.ToLower()))
	{
		// The page is never cached: an edit made on site shows up on the iPad
		// at the next reload, not at some later whim of Safari's.
		SendResponse(Client.Socket, 200, File->ContentType, File->Bytes, true);
		return;
	}

	SendText(Client.Socket, 404, TEXT("introuvable"));
}

void FVibH2ORemoteServer::SendResponse(FSocket* Socket, int32 Status, const FString& ContentType, const TArray<uint8>& Body, bool bNoCache)
{
	TArray<uint8> Bytes;
	VibH2ORemoteInternal::AppendUtf8(Bytes, FString::Printf(
		TEXT("HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\n%s\r\n"),
		Status, VibH2ORemoteInternal::StatusText(Status), *ContentType, Body.Num(),
		bNoCache ? TEXT("Cache-Control: no-store\r\n") : TEXT("")));
	Bytes.Append(Body);

	// Responses are small - the page is a few tens of kilobytes. A blocking
	// send with a send buffer larger than any of them completes at once, and
	// spares a retry loop.
	int32 BufferSize = 0;
	Socket->SetSendBufferSize(512 * 1024, BufferSize);
	Socket->SetNonBlocking(false);

	int32 Offset = 0;
	while (Offset < Bytes.Num())
	{
		int32 Sent = 0;
		if (!Socket->Send(Bytes.GetData() + Offset, Bytes.Num() - Offset, Sent) || Sent <= 0)
		{
			break;
		}
		Offset += Sent;
	}
}

void FVibH2ORemoteServer::SendText(FSocket* Socket, int32 Status, const FString& Text)
{
	TArray<uint8> Body;
	VibH2ORemoteInternal::AppendUtf8(Body, Text);
	SendResponse(Socket, Status, TEXT("text/plain; charset=utf-8"), Body, true);
}

void FVibH2ORemoteServer::CloseClient(FClient& Client)
{
	if (Client.Socket != nullptr)
	{
		// Write side first, then close: the request has been read in full, so
		// nothing unread is left to turn the close into a reset that would
		// lose the response on the iPad's side.
		Client.Socket->Shutdown(ESocketShutdownMode::Write);
		Client.Socket->Close();
		SocketSubsystem->DestroySocket(Client.Socket);
		Client.Socket = nullptr;
	}
}

void FVibH2ORemoteServer::CloseAll()
{
	for (FClient& Client : Clients)
	{
		CloseClient(Client);
	}
	Clients.Reset();
}

// --------------------------------------------------------------------- Pure

bool FVibH2ORemoteServer::ParseCommandLine(const FString& Line, FVibH2OOscMessage& OutMessage)
{
	TArray<FString> Tokens;
	Line.TrimStartAndEnd().ParseIntoArrayWS(Tokens);
	if (Tokens.Num() == 0 || !Tokens[0].StartsWith(TEXT("/")))
	{
		return false;
	}

	OutMessage = FVibH2OOscMessage(Tokens[0]);
	for (int32 Index = 1; Index < Tokens.Num(); ++Index)
	{
		const FString& Token = Tokens[Index];
		if (Token.IsNumeric())
		{
			const bool bFloat = Token.Contains(TEXT(".")) || Token.Contains(TEXT("e")) || Token.Contains(TEXT("E"));
			OutMessage.Args.Add(bFloat
				? FVibH2OOscValue::MakeFloat(FCString::Atof(*Token))
				: FVibH2OOscValue::MakeInt(FCString::Atoi(*Token)));
		}
		else
		{
			OutMessage.Args.Add(FVibH2OOscValue::MakeString(Token));
		}
	}
	return true;
}

bool FVibH2ORemoteServer::ParseRequestHead(const FString& Head, FString& OutMethod, FString& OutPath, int32& OutContentLength)
{
	OutMethod.Reset();
	OutPath.Reset();
	OutContentLength = 0;

	TArray<FString> Lines;
	Head.ParseIntoArray(Lines, TEXT("\r\n"), /*InCullEmpty=*/true);
	if (Lines.Num() == 0)
	{
		return false;
	}

	TArray<FString> RequestLine;
	Lines[0].ParseIntoArrayWS(RequestLine);
	if (RequestLine.Num() < 2 || !RequestLine[1].StartsWith(TEXT("/")))
	{
		return false;
	}

	OutMethod = RequestLine[0].ToUpper();
	OutPath = RequestLine[1];
	int32 Query = INDEX_NONE;
	if (OutPath.FindChar(TEXT('?'), Query))
	{
		OutPath.LeftInline(Query);
	}

	for (int32 Index = 1; Index < Lines.Num(); ++Index)
	{
		FString Name;
		FString Value;
		if (Lines[Index].Split(TEXT(":"), &Name, &Value) && Name.TrimStartAndEnd().Equals(TEXT("Content-Length"), ESearchCase::IgnoreCase))
		{
			OutContentLength = FMath::Max(FCString::Atoi(*Value.TrimStartAndEnd()), 0);
		}
	}
	return true;
}

FString FVibH2ORemoteServer::ContentTypeFor(const FString& FileName)
{
	const FString Extension = FPaths::GetExtension(FileName).ToLower();
	if (Extension == TEXT("html") || Extension == TEXT("htm")) { return TEXT("text/html; charset=utf-8"); }
	if (Extension == TEXT("js")) { return TEXT("application/javascript; charset=utf-8"); }
	if (Extension == TEXT("css")) { return TEXT("text/css; charset=utf-8"); }
	if (Extension == TEXT("json")) { return TEXT("application/json; charset=utf-8"); }
	if (Extension == TEXT("webmanifest")) { return TEXT("application/manifest+json; charset=utf-8"); }
	if (Extension == TEXT("png")) { return TEXT("image/png"); }
	if (Extension == TEXT("svg")) { return TEXT("image/svg+xml"); }
	if (Extension == TEXT("ico")) { return TEXT("image/x-icon"); }
	return TEXT("application/octet-stream");
}
