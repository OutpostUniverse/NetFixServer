#include "GameSession.h"
#include <cstring>

bool GameSession::SocketAddressMatches(const sockaddr_in& socketAddress) const
{
	return std::memcmp(&addr, &socketAddress, sizeof(socketAddress)) == 0;
}

bool GameSession::IsGameSessionExpected() const
{
	return (flags & GameServerGameFlags::GameSessionExpected) != 0;
}

bool GameSession::IsGameSessionReceived() const
{
	return (flags & GameServerGameFlags::GameSessionReceived) != 0;
}

bool GameSession::IsGameSessionUpdateRetrySent() const
{
	return (flags & GameServerGameFlags::GameSessionUpdateRetrySent) != 0;
}