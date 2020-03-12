#pragma once

#include "GameSession.h"
#include "Packet.h"
#include <ctime>
#include <cstring>
#include <cstddef>
#include <vector>
#include <string>

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
	unsigned int numFailedGameSessionAllocations;
};


class GameServer
{
public:
	GameServer(unsigned short port);
	~GameServer();

	void Pump();
	void WaitForEvent();

private:
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

	void AllocSocket(SOCKET& socket, unsigned short port);
	std::string FormatSocketError(const std::string& message, const sockaddr_in& address);
	void ProcessPacket(Packet& packet, sockaddr_in& from);
	void ProcessJoinRequest(Packet& packet, const sockaddr_in& from);
	void ProcessGameSearchQuery(Packet& packet, sockaddr_in& from);
	void ProcessGameSearchReply(Packet& packet, sockaddr_in& from);
	void ProcessPoke(Packet& packet, sockaddr_in& from);
	void ProcessRequestExternalAddress(Packet& packet, sockaddr_in& from);
	void DoTimedUpdates();
	void DropGameNoInitialContact(std::size_t sessionIndex);
	void DropGameLostContact(std::size_t sessionIndex);
	void RequestGameUpdateFirstTry(GameSession& gameSession);
	void RequestGameUpdateSecondTry(GameSession& gameSession);
	std::size_t FindGameSessionClient(const sockaddr_in& from, unsigned int clientRandValue);
	std::size_t FindGameSessionServer(const sockaddr_in& from, unsigned int serverRandValue);
	void FreeGameSession(std::size_t index);
	unsigned int GetNewRandValue();
	int ReceiveFrom(Packet& packet, const sockaddr_in& from);
	bool ReadSocketData(std::size_t& byteCountOut, SOCKET& socket, Packet& packetBuffer, const sockaddr_in& from);
	void SendTo(Packet& packet, const sockaddr_in& to);
	void SendGameSessionRequest(const GameSession& gameSession);
	// Win32 specific functions
#ifdef WIN32
	void InitializeWinsock();
#endif

	SOCKET hostSocket;
	SOCKET secondarySocket;
	std::vector<GameSession> gameSessions;
	GameServerCounters counters;
};
