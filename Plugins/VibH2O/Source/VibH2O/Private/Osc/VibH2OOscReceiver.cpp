#include "Osc/VibH2OOscReceiver.h"

#include "Common/UdpSocketBuilder.h"
#include "HAL/RunnableThread.h"
#include "Osc/VibH2OOscParser.h"
#include "SocketSubsystem.h"
#include "Sockets.h"
#include "VibH2OModule.h"

namespace VibH2OReceiverInternal
{
	/**
	 * Maximum wait on the socket before re-checking the stop request. Short
	 * enough that shutdown looks instant, long enough not to spin.
	 */
	static constexpr int32 WaitMilliseconds = 50;

	/** Receive buffer size for a single datagram. */
	static constexpr int32 DatagramBufferSize = 65535;
}

FVibH2OOscReceiver::FVibH2OOscReceiver()
{
	ReceiveBuffer.SetNumUninitialized(VibH2OReceiverInternal::DatagramBufferSize);
}

FVibH2OOscReceiver::~FVibH2OOscReceiver()
{
	Shutdown();
}

FString FVibH2OOscReceiver::GetLastError() const
{
	FScopeLock Lock(&ErrorLock);
	return LastError;
}

bool FVibH2OOscReceiver::Start(const FString& ListenAddress, int32 Port, int32 ReceiveBufferSize, int32 InMaxQueuedMessages)
{
	Shutdown();

	MaxQueuedMessages = FMath::Max(InMaxQueuedMessages, 256);

	SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (SocketSubsystem == nullptr)
	{
		FScopeLock Lock(&ErrorLock);
		LastError = TEXT("Aucun sous-systeme de socket disponible.");
		UE_LOG(LogVibH2O, Error, TEXT("VibH2O: %s"), *LastError);
		return false;
	}

	FIPv4Address Address;
	if (!FIPv4Address::Parse(ListenAddress, Address))
	{
		// An invalid listen address must not prevent startup: fall back to "all
		// interfaces", which is the common case anyway.
		UE_LOG(LogVibH2O, Warning,
			TEXT("VibH2O: adresse d'ecoute '%s' invalide, repli sur 0.0.0.0."), *ListenAddress);
		Address = FIPv4Address::Any;
	}

	const FIPv4Endpoint Endpoint(Address, static_cast<uint16>(FMath::Clamp(Port, 1, 65535)));
	EndpointDescription = Endpoint.ToString();

	Socket = FUdpSocketBuilder(TEXT("VibH2OOscReceiver"))
		.AsNonBlocking()
		.AsReusable()
		.BoundToEndpoint(Endpoint)
		.WithReceiveBufferSize(FMath::Max(ReceiveBufferSize, 1 << 16))
		.Build();

	if (Socket == nullptr)
	{
		FScopeLock Lock(&ErrorLock);
		LastError = FString::Printf(
			TEXT("Impossible d'ouvrir le socket UDP sur %s. Le port est-il deja utilise par un autre logiciel ?"),
			*EndpointDescription);
		UE_LOG(LogVibH2O, Error, TEXT("VibH2O: %s"), *LastError);
		return false;
	}

	{
		FScopeLock Lock(&ErrorLock);
		LastError.Reset();
	}

	StopCounter.Reset();
	PacketCounter.Reset();
	MessageCounter.Reset();
	DroppedCounter.Reset();
	MalformedCounter.Reset();
	QueueDepth.Reset();

	Thread = FRunnableThread::Create(this, TEXT("VibH2OOscReceiver"), 0, TPri_AboveNormal);
	if (Thread == nullptr)
	{
		CloseSocket();
		FScopeLock Lock(&ErrorLock);
		LastError = TEXT("Impossible de creer le thread de reception.");
		UE_LOG(LogVibH2O, Error, TEXT("VibH2O: %s"), *LastError);
		return false;
	}

	bRunning = true;
	UE_LOG(LogVibH2O, Log, TEXT("VibH2O: ecoute OSC demarree sur %s."), *EndpointDescription);
	return true;
}

void FVibH2OOscReceiver::Shutdown()
{
	if (Thread != nullptr)
	{
		StopCounter.Increment();
		// Kill(true) waits for Run() to return. That is what guarantees no queue
		// write outlives the destruction of this object.
		Thread->Kill(true);
		delete Thread;
		Thread = nullptr;
	}

	CloseSocket();

	FVibH2OOscMessage Discarded;
	while (Queue.Dequeue(Discarded))
	{
	}
	QueueDepth.Reset();

	if (bRunning)
	{
		UE_LOG(LogVibH2O, Log, TEXT("VibH2O: ecoute OSC arretee (%d paquets, %d messages, %d jetes)."),
			PacketCounter.GetValue(), MessageCounter.GetValue(), DroppedCounter.GetValue());
	}
	bRunning = false;
}

void FVibH2OOscReceiver::CloseSocket()
{
	if (Socket != nullptr)
	{
		Socket->Close();
		if (SocketSubsystem != nullptr)
		{
			SocketSubsystem->DestroySocket(Socket);
		}
		Socket = nullptr;
	}
}

bool FVibH2OOscReceiver::Dequeue(FVibH2OOscMessage& OutMessage)
{
	if (Queue.Dequeue(OutMessage))
	{
		QueueDepth.Decrement();
		return true;
	}
	return false;
}

bool FVibH2OOscReceiver::Init()
{
	return Socket != nullptr;
}

uint32 FVibH2OOscReceiver::Run()
{
	TArray<FVibH2OOscMessage> Messages;
	Messages.Reserve(64);

	while (StopCounter.GetValue() == 0)
	{
		if (Socket == nullptr)
		{
			break;
		}

		// Bounded wait: the thread stays responsive to the stop request without
		// spinning between packets.
		if (!Socket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromMilliseconds(VibH2OReceiverInternal::WaitMilliseconds)))
		{
			continue;
		}

		uint32 PendingSize = 0;
		while (Socket != nullptr && Socket->HasPendingData(PendingSize) && StopCounter.GetValue() == 0)
		{
			int32 BytesRead = 0;
			TSharedRef<FInternetAddr> Sender = SocketSubsystem->CreateInternetAddr();

			if (!Socket->RecvFrom(ReceiveBuffer.GetData(), ReceiveBuffer.Num(), BytesRead, *Sender))
			{
				break;
			}
			if (BytesRead <= 0)
			{
				continue;
			}

			PacketCounter.Increment();

			Messages.Reset();
			if (!FVibH2OOscParser::ParsePacket(ReceiveBuffer.GetData(), BytesRead, Messages))
			{
				MalformedCounter.Increment();
				// A partially readable frame still leaves its valid messages in
				// Messages, so keep them.
			}

			for (FVibH2OOscMessage& Message : Messages)
			{
				if (QueueDepth.GetValue() >= MaxQueuedMessages)
				{
					// The game thread is falling behind. Drop rather than let
					// memory run away, and count it so it shows up in the debug
					// overlay.
					DroppedCounter.Increment();
					continue;
				}
				QueueDepth.Increment();
				MessageCounter.Increment();
				Queue.Enqueue(MoveTemp(Message));
			}
		}
	}

	return 0;
}

void FVibH2OOscReceiver::Stop()
{
	StopCounter.Increment();
}

void FVibH2OOscReceiver::Exit()
{
}
