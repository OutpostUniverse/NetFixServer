#pragma once

#include "Packet.h"
#include <time.h>
#include <cstddef>
#include <functional>

#ifdef WIN32
	#include <winsock2.h>
	#define socklen_t int
#else
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <sys/ioctl.h>
	#include <netinet/in.h>
	#include <unistd.h>
	#define SOCKET int
	#define SOCKET_ERROR -1
	#define INVALID_SOCKET -1
#endif


struct GameServerCounters
{
	// Game counts
	unsigned int numGamesHosted;
	unsigned int numGamesStarted;
	unsigned int numGamesCancelled;
	unsigned int numGamesDropped;
	// Traffic counts
	unsigned int numPacketsReceived;
	unsigned int numBytesReceived;
	unsigned int numPacketsSent;
	unsigned int numBytesSent;
	// Performance counts
	unsigned int numDroppedHostedPokes;
	unsigned int numUpdateRequestSent;
	unsigned int numRetrySent;
	unsigned int numNewHost;
	// Receive Error counts
	unsigned int numMinSizeErrors;
	unsigned int numSizeFieldErrors;
	unsigned int numTypeFieldErrors;
	unsigned int numChecksumFieldErrors;
	// Error counts
	unsigned int numFailedGameInfoAllocs;
};

enum GameServerErrorCode
{
	NoError = 0,
	// Init Errors
	WinsockInitFailed,
	WinsockVersionFailed,
	SocketCreateFailed,
	SocketBindFailed,
	SocketNonBlockingModeFailed,
	AllocGameListFailed,
};


class GameServer
{
public:
	GameServer();
	~GameServer();

	int StartServer(unsigned short port);
	void Pump();
	void WaitForEvent();

private:
	struct GameInfo
	{
		GUID sessionIdentifier;
		sockaddr_in addr;
		time_t time;
		unsigned int clientRandValue;
		unsigned int serverRandValue;
		unsigned int flags;
		CreateGameInfo createGameInfo;
	};

	enum GameServerGameFlags
	{
		GameInfoExpected = 1,
		GameInfoReceived = 2,
		GameInfoUpdateRetrySent = 4,
	};

	enum GameServerPacketErrorCode
	{
		// No packet
		PacketNone = -1,
		// Packet errors
		PacketSizeBad = -2,
		PacketSizeFieldBad = -3,
		PacketTypeBad = -4,
		PacketChecksumBad = -5,
	};

	int AllocSocket(SOCKET& socket, unsigned short port);
	void ProcessPacket(Packet& packet, sockaddr_in& from);
	void ProcessJoinRequest(Packet& packet, const sockaddr_in& from);
	void ProcessGameSearchQuery(Packet& packet, sockaddr_in& from);
	void ProcessGameSearchReply(Packet& packet, sockaddr_in& from);
	void ProcessPoke(Packet& packet, sockaddr_in& from);
	void ProcessRequestExternalAddress(Packet& packet, sockaddr_in& from);
	void DoTimedUpdates();
	std::size_t FindGameInfoClient(const sockaddr_in& from, unsigned int clientRandValue);
	std::size_t FindGameInfoServer(const sockaddr_in& from, unsigned int serverRandValue);
	std::size_t FindGameInfo(const sockaddr_in& from, const std::function <bool(const GameInfo&)>& compareFunction);
	int GetNewGameInfo();
	void FreeGameInfo(std::size_t index);
	unsigned int GetNewRandValue();
	int ReceiveFrom(Packet& packet, sockaddr_in& from);
	void SendTo(Packet& packet, sockaddr_in& to);
	void SendGameInfoRequest(sockaddr_in& to, unsigned int serverRandValue);
	// Win32 specific functions
#ifdef WIN32
	int InitWinsock();
#endif

	SOCKET hostSocket;
	SOCKET secondarySocket;
	GameInfo* gameInfo;
	unsigned int numGames;
	unsigned int maxNumGames;
	GameServerCounters counters;
	// Win32 specific data
	#ifdef WIN32
		int bWinsockInitialized;
	#endif
};
