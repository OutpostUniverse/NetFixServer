#pragma once

#include "Packet.h"
#include <ctime>

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

enum GameServerGameFlags
{
	GameSessionExpected = 1,
	GameSessionReceived = 2,
	GameSessionUpdateRetrySent = 4,
};

struct GameSession
{
	GUID sessionIdentifier;
	sockaddr_in addr;
	std::time_t time;
	unsigned int clientRandValue;
	unsigned int serverRandValue;
	unsigned int flags = 0;
	CreateGameInfo createGameInfo;

	bool SocketAddressMatches(const sockaddr_in& socketAddress) const;

	bool IsGameSessionExpected() const;
	bool IsGameSessionReceived() const;
	bool IsGameSessionUpdateRetrySent() const;
};
