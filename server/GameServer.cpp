#include "GameServer.h"
#include "ErrorLog.h"
#include <limits>
#include <stdexcept>

#ifndef WIN32
	#define closesocket close
	#define ioctlsocket ioctl
#endif

constexpr std::size_t InvalidGameSessionIndex = (std::numeric_limits<std::size_t>::max)();

constexpr unsigned int UpdateTime = 60;			// Number of seconds before requesting update
constexpr unsigned int RetryTime = 64;			// Number of seconds before requesting update retry
constexpr unsigned int GiveUpTime = 68;			// Number of seconds before clearing dead entries
constexpr unsigned int InitialReplyTime = 4;	// Number of seconds allowed for first update
constexpr GUID gameIdentifier = { 0x5A55CF11, 0xB841, 0x11CE, {0x92, 0x10, 0x00, 0xAA, 0x00, 0x6C, 0x49, 0x72} };



GameServer::GameServer(unsigned short port) :
	hostSocket(INVALID_SOCKET)
{
	counters = {};

#ifdef WIN32
	InitializeWinsock();
#endif

	// Create the host socket
	try {
		AllocSocket(hostSocket, port);
	}
	catch (const std::exception& e) {
		throw std::runtime_error("Error allocating GameServer's host socket: " + std::string(e.what()));
	}

	// Create the secondary socket
	try {
		AllocSocket(secondarySocket, port + 1);
	}
	catch (const std::exception& e) {
		throw std::runtime_error("Error allocating GameServer's secondary socket: " + std::string(e.what()));
	}
}

GameServer::~GameServer()
{
	// Check if a socket exists
	if (hostSocket != INVALID_SOCKET) {
		closesocket(hostSocket);
	}

#ifdef WIN32
	WSACleanup();
#endif
}

void GameServer::Pump()
{
	sockaddr_in from;
	Packet packet;
	int numBytes;

	// Process all received packets
	for (;;)
	{
		// Check for received packets
		numBytes = ReceiveFrom(packet, from);
		if (numBytes >= 0) {
			ProcessPacket(packet, from);
		}
		else
		{
			if (numBytes != PacketNone)
			{
				#ifdef DEBUG
					LogValue("ReceiveFrom returned error code: ", numBytes);
				#endif
			}
		}

		// Do periodic updates
		DoTimedUpdates();

		// Check if we are done processing packets
		if (numBytes == -1) {
			return;
		}
	}
}


void GameServer::WaitForEvent()
{
	fd_set readfds;
	timeval timeOut = { 1, 0 };

	// Clear the read
	FD_ZERO(&readfds);
	FD_SET(hostSocket, &readfds);
	FD_SET(secondarySocket, &readfds);
	// Wait for packets or timeout
	select(1, &readfds, nullptr, nullptr, &timeOut);
}


void GameServer::AllocSocket(SOCKET& hostSocket, unsigned short port)
{
	// Create the host socket
	hostSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if (hostSocket == INVALID_SOCKET) {
		throw std::runtime_error("Unable to allocate socket at port " + std::to_string(port));
	}

	// Bind the socket to the host port
	sockaddr_in hostAddress;
	// Initialize the struct
	memset(hostAddress.sin_zero, 0, sizeof(hostAddress.sin_zero));
	hostAddress.sin_family = AF_INET;
	hostAddress.sin_port = htons(port);
	hostAddress.sin_addr.s_addr = INADDR_ANY;
	// Bind the socket
	int errorCode = bind(hostSocket, (sockaddr*)&hostAddress, sizeof(hostAddress));

	if (errorCode == SOCKET_ERROR) {
		throw std::runtime_error(FormatSocketError("Unable to bind socket at host address of ", hostAddress));
	}

	// Set non-blocking mode
	unsigned long argp = true;
	errorCode = ioctlsocket(hostSocket, FIONBIO, &argp);

	if (errorCode != 0) {
		throw std::runtime_error(FormatSocketError("Unable to set non-blocking mode on socket at host address of  ", hostAddress));
	}
}

std::string GameServer::FormatSocketError(const std::string& message, const sockaddr_in& address)
{
	std::string formattedMessage = message + FormatIPAddressWithPort(address.sin_addr.s_addr, address.sin_port) + ". ";

#ifdef WIN32
	formattedMessage += std::string(". Windows Sockets Error Code: " + std::to_string(WSAGetLastError()));
#endif

	return formattedMessage;
}

void GameServer::ProcessPacket(Packet &packet, sockaddr_in &from)
{
	#ifdef DEBUG
		//LogMessage("ProcessPacket");
		//LogValue("CommandType: ", packet.tlMessage.tlHeader.commandType);
	#endif

	switch(packet.tlMessage.tlHeader.commandType)
	{
	case tlcJoinRequest:
		ProcessJoinRequest(packet, from);
		return;
	case tlcHostedGameSearchQuery:
		ProcessGameSearchQuery(packet, from);
		return;
	case tlcHostedGameSearchReply:
		ProcessGameSearchReply(packet, from);
		return;
	case tlcGameServerPoke:
		ProcessPoke(packet, from);
		return;
	case tlcRequestExternalAddress:
		ProcessRequestExternalAddress(packet, from);
		return;
	// Message types not relevant to the game server
	// Unused messages included to prevent GCC-8 case label warning
	case tlcJoinHelpRequest:
	case tlcEchoExternalAddress:
		return;
	}
}

void GameServer::ProcessJoinRequest(Packet& packet, const sockaddr_in& from)
{
	// Verify the packet size
	if (packet.header.sizeOfPayload != sizeof(JoinRequest)) {
		return;		// Discard (bad size)
	}

	LogEndpoint("Game Join Request from: ", from.sin_addr.s_addr, from.sin_port);

	packet.tlMessage.tlHeader.commandType = tlcJoinHelpRequest;
	packet.tlMessage.joinHelpRequest.clientAddr = from;
	packet.tlMessage.joinHelpRequest.clientAddr.sin_family = 2;		// ** AF_INET

	// Search for a corresponding Session Identifer
	for (const GameSession& gameSession : gameSessions)
	{
		// Check if the Session Identifier matches
		if (memcmp(&gameSession.sessionIdentifier, &packet.tlMessage.joinRequest.sessionIdentifier, sizeof(packet.tlMessage.joinRequest.sessionIdentifier)) == 0)
		{
			// Send the Join Help Request
			SendTo(packet, gameSession.addr);
		}
	}
}

void GameServer::ProcessGameSearchQuery(Packet& packet, sockaddr_in& from)
{
	// Verify packet size
	if (packet.header.sizeOfPayload != sizeof(HostedGameSearchQuery)) {
		return;		// Discard (bad size)
	}
	// Verify the game identifier
	if (packet.tlMessage.searchQuery.gameIdentifier != gameIdentifier) {
		return;		// Discard (wrong game)
	}

	LogEndpoint("Game Search Query from: ", from.sin_addr.s_addr, from.sin_port);

	// Send client a list of games
	// Setup common packet fields
	packet.header.sizeOfPayload = sizeof(HostedGameSearchReply);
	packet.tlMessage.tlHeader.commandType = tlcHostedGameSearchReply;
	// Search games list for suitable games
	for (const GameSession& gameSession : gameSessions)
	{
		// Make sure we have valid game data
		if ((gameSession.flags & GameSessionReceived) != 0)
		{
			LogString("  GameCreator: ", gameSession.createGameInfo.gameCreatorName);

			// Consruct a reply packet for this game
			packet.tlMessage.searchReply.sessionIdentifier = gameSession.sessionIdentifier;
			packet.tlMessage.searchReply.createGameInfo = gameSession.createGameInfo;
			packet.tlMessage.searchReply.hostAddress = gameSession.addr;
			packet.tlMessage.searchReply.hostAddress.sin_family = 2;		// ** AF_INET
			// Send the reply
			SendTo(packet, from);
		}
	}
}

void GameServer::ProcessGameSearchReply(Packet& packet, sockaddr_in& from)
{
	// Verify packet size
	if (packet.header.sizeOfPayload != sizeof(HostedGameSearchReply)) {
		return;		// Discard (bad size)
	}

	// Make sure we queried this server
	std::size_t gameSessionIndex = FindGameSessionServer(from, packet.tlMessage.searchReply.timeStamp);
	if (gameSessionIndex == InvalidGameSessionIndex) {
		return;		// Discard (not requested or bad time stamp, possible spam or spoofing attack)
	}

	LogEndpoint("Received Host Info from: ", from.sin_addr.s_addr, from.sin_port);

	// Add the game to the list
	GameSession& gameSession = gameSessions[gameSessionIndex];
	gameSession.sessionIdentifier = packet.tlMessage.searchReply.sessionIdentifier;
	gameSession.addr = from;
	gameSession.flags |= GameSessionReceived;
	gameSession.flags &= ~GameSessionExpected & ~GameSessionUpdateRetrySent;
	gameSession.createGameInfo = packet.tlMessage.searchReply.createGameInfo;
	gameSession.time = std::time(nullptr);
}

void GameServer::ProcessPoke(Packet& packet, sockaddr_in& from)
{
	// Verify packet size
	if (packet.header.sizeOfPayload != sizeof(GameServerPoke)) {
		return;		// Discard (bad size)
	}

#ifdef DEBUG
	//LogValue("Poke: ", packet.tlMessage.gameServerPoke.statusCode);
#endif

	// Find the current GameSession
	std::size_t gameSessionIndex = FindGameSessionClient(from, packet.tlMessage.gameServerPoke.randValue);

	// Check what kind of poke this is
	switch (packet.tlMessage.gameServerPoke.statusCode)
	{
	case pscGameHosted: {
		// Check if this game server is not already known
		if (gameSessionIndex == InvalidGameSessionIndex)
		{
			counters.numNewHost++;
			gameSessions.push_back(GameSession());
			gameSessionIndex = gameSessions.size() - 1;
		}

		LogEndpoint("Game Hosted from: ", from.sin_addr.s_addr, from.sin_port);

		GameSession& newGameSession = gameSessions[gameSessionIndex];
		newGameSession.addr = from;
		newGameSession.clientRandValue = packet.tlMessage.gameServerPoke.randValue;
		newGameSession.serverRandValue = GetNewRandValue();
		newGameSession.flags |= GameSessionExpected;
		newGameSession.time = std::time(nullptr);

		// Send a request for games
		SendGameSessionRequest(newGameSession);

		// Update counters
		counters.numGamesHosted++;

		return;
	}
	case pscGameStarted: {
		LogEndpoint("Game Started: ", from.sin_addr.s_addr, from.sin_port);

		// Remove the game from the list
		FreeGameSession(gameSessionIndex);
		counters.numGamesStarted++;

		return;
	}
	case pscGameCancelled: {
		LogEndpoint("Game Cancelled: ", from.sin_addr.s_addr, from.sin_port);

		// Remove the game from the list
		FreeGameSession(gameSessionIndex);
		counters.numGamesCancelled++;

		return;
	}
	}
}

void GameServer::ProcessRequestExternalAddress(Packet& packet, sockaddr_in& from)
{
	// Verify packet size
	if (packet.header.sizeOfPayload != sizeof(RequestExternalAddress)) {
		return;		// Discard (bad size)
	}

	// Cache the internal port being used
	unsigned short internalPort = packet.tlMessage.requestExternalAddress.internalPort;

	// Construct a reply
	packet.header.sizeOfPayload = sizeof(EchoExternalAddress);
	packet.tlMessage.tlHeader.commandType = tlcEchoExternalAddress;
	packet.tlMessage.echoExternalAddress.addr = from;
	packet.tlMessage.echoExternalAddress.replyPort = from.sin_port;

	// Send first reply
	SendTo(packet, from);

	// Check if they have a different internal address
	if (from.sin_port != internalPort)
	{
		// Send second reply
		packet.tlMessage.echoExternalAddress.replyPort = internalPort;
		from.sin_port = htons(internalPort);
		SendTo(packet, from);
	}
}

void GameServer::DoTimedUpdates()
{
	auto currentTime = std::time(nullptr);

	// Check for timed out game entries
	for (std::size_t i = gameSessions.size(); i-- > 0; )
	{
		GameSession& gameSession = gameSessions[i];
		auto timeDiff = currentTime - gameSession.time;

		// Check for no initial update within required time
		if (timeDiff >= InitialReplyTime && !gameSession.IsGameSessionReceived())
		{
			DropGameNoInitialContact(i);
		}
		// Check if no updates have occured for a while
		else if (timeDiff >= UpdateTime && gameSession.IsGameSessionReceived())
		{
			// Entry is old and requires update
			// --------------------------------

			// Check if enough time has elapsed to give up
			if (timeDiff >= GiveUpTime)
			{
				DropGameLostContact(i);
			}
			else if (!gameSession.IsGameSessionExpected())
			{
				RequestGameUpdateFirstTry(gameSession);
			}
			else if (timeDiff >= RetryTime && !gameSession.IsGameSessionUpdateRetrySent())
			{
				RequestGameUpdateSecondTry(gameSession);
			}
		}
	}

	LogCounters(counters);
}

void GameServer::DropGameNoInitialContact(std::size_t sessionIndex)
{
	LogEndpoint("Dropping Game: No initial Host Info from: ", gameSessions[sessionIndex].addr.sin_addr.s_addr, gameSessions[sessionIndex].addr.sin_port);

	// Clear bogus entry
	FreeGameSession(sessionIndex);
	// Update counters
	counters.numDroppedHostedPokes++;
}

void GameServer::DropGameLostContact(std::size_t sessionIndex)
{
	GameSession& gameSession = gameSessions[sessionIndex];
	LogEndpoint("Dropping Game: Lost contact with host: ", gameSession.addr.sin_addr.s_addr, gameSession.addr.sin_port);

	// Give up
	FreeGameSession(sessionIndex);
	counters.numGamesDropped++;
}

void GameServer::RequestGameUpdateFirstTry(GameSession& gameSession)
{
	LogEndpoint("Requesting Game info update 1 (periodic): ", gameSession.addr.sin_addr.s_addr, gameSession.addr.sin_port);

	// Game info is stale, request update
	SendGameSessionRequest(gameSession);
	gameSession.flags |= GameSessionExpected;
	counters.numUpdateRequestSent++;
}

void GameServer::RequestGameUpdateSecondTry(GameSession& gameSession)
{
	LogEndpoint("Requesting Game info update 2 (retry): ", gameSession.addr.sin_addr.s_addr, gameSession.addr.sin_port);

	// Assume the packet was dropped. Retry.
	SendGameSessionRequest(gameSession);
	gameSession.flags |= GameSessionUpdateRetrySent;
	counters.numRetrySent++;
}

std::size_t GameServer::FindGameSessionClient(const sockaddr_in &from, unsigned int clientRandValue)
{
	// Search games list
	for (std::size_t i = 0; i < gameSessions.size(); ++i)
	{
		// Check if this address matches
		if ((gameSessions[i].clientRandValue == clientRandValue) && gameSessions[i].SocketAddressMatches(from))
		{
			// Return the GameSession
			return i;
		}
	}

	// GameSession not found
	return InvalidGameSessionIndex;
}


std::size_t GameServer::FindGameSessionServer(const sockaddr_in &from, unsigned int serverRandValue)
{
	// Search games list
	for (std::size_t i = 0; i < gameSessions.size(); ++i)
	{
		// Check if this address matches
		if ((gameSessions[i].serverRandValue == serverRandValue) && gameSessions[i].SocketAddressMatches(from))
		{
			// Return the GameSession
			return i;
		}
	}

	// GameSession not found
	return InvalidGameSessionIndex;
}

void GameServer::FreeGameSession(std::size_t index)
{
	gameSessions.erase(gameSessions.begin() + index);
}


unsigned int GameServer::GetNewRandValue()
{
	return 0;		// **TODO**
}


int GameServer::ReceiveFrom(Packet &packet, const sockaddr_in &from)
{
	// Read any received packets
	std::size_t numBytes;

	if (!ReadSocketData(numBytes, hostSocket, packet, from))
	{
		if (!ReadSocketData(numBytes, secondarySocket, packet, from))
		{
			// Check if not a would block error **TODO**
			return PacketNone;				// No Packet (would block)
		}
	}

	#ifdef DEBUG
		//LogMessage("Packet Received");
	#endif

	// Do simple error checks common to all packets
	// Check the packet min size
	if (numBytes < sizeof(packet.header) + sizeof(packet.tlMessage.tlHeader))
	{
		counters.numMinSizeErrors++;
		return PacketSizeBad;			// Invalid packet (bad size)
	}
	// Check the packet size field
	if ((packet.header.sizeOfPayload + sizeof(packet.header)) != numBytes)
	{
		counters.numSizeFieldErrors++;
		return PacketSizeFieldBad;		// Invalid packet (bad size field)
	}
	// Check the packet type
	if (packet.header.type != 1)
	{
		counters.numTypeFieldErrors++;
		return PacketTypeBad;			// Invalid packet (format)
	}
	// Check the packet checksum
	if (packet.header.checksum != packet.Checksum())
	{
		counters.numChecksumFieldErrors++;
		return PacketChecksumBad;		// Invalid packet (checksum)
	}

	#ifdef DEBUG
		//LogMessage("Received packet passes all common tests");
	#endif

	counters.numPacketsReceived++;
	counters.numBytesReceived += numBytes;

	return numBytes;
}

// Hide implementation details of recvfrom library function
// On failure, byteCountOut is set to 0. Returns false is SOCKET_ERROR encountered.
bool GameServer::ReadSocketData(std::size_t& byteCountOut, SOCKET& socket, Packet& packetBuffer, const sockaddr_in& from)
{
	socklen_t addrLen = sizeof(from);

	// recvfrom's return type is different on MSVC (int) and Linux (ssize_t) compilations
	auto byteCount = recvfrom(socket, (char*)&packetBuffer, sizeof(packetBuffer), 0, (sockaddr*)&from, &addrLen);

	if (byteCount == SOCKET_ERROR) {
		byteCountOut = 0u;
		return false;
	}

	byteCountOut = static_cast<std::size_t>(byteCount);
	return true;
}

void GameServer::SendTo(Packet &packet, const sockaddr_in &to)
{
	int size = packet.header.sizeOfPayload + sizeof(packet.header);
	int toLen = sizeof(to);

	// Calculate the packet checksum
	packet.header.checksum = packet.Checksum();

	// Send the packet
	auto errorCode = sendto(hostSocket, (char*)&packet, size, 0, (sockaddr*)&to, toLen);

	if (errorCode == SOCKET_ERROR)
	{
		// Error  **TODO** Update counters
		#ifdef DEBUG
			LogMessage("Error: SendTo socket error");
		#endif
	}
	else
	{
		counters.numPacketsSent++;
		counters.numBytesSent += size;
	}
}


void GameServer::SendGameSessionRequest(const GameSession& gameSession)
{
	Packet packet;

	// Consntruct a Hosted Game Search Query packet
	packet.header.sourcePlayerNetID = 0;
	packet.header.destPlayerNetID = 0;
	packet.header.type = 1;
	packet.header.sizeOfPayload = sizeof(HostedGameSearchQuery);
	packet.tlMessage.tlHeader.commandType = tlcHostedGameSearchQuery;
	packet.tlMessage.searchQuery.gameIdentifier = gameIdentifier;
	packet.tlMessage.searchQuery.timeStamp = gameSession.serverRandValue;
	memset(packet.tlMessage.searchQuery.password, 0, sizeof(packet.tlMessage.searchQuery.password));

	// Send the packet
	SendTo(packet, gameSession.addr);
}


#ifdef WIN32
	void GameServer::InitializeWinsock()
	{
		// Initialize Winsock
		WSADATA wsaData;
		WORD version = MAKEWORD(2, 2);

		// Initialize Winsock
		int errorCode = WSAStartup(version, &wsaData);

		if (errorCode == 0)
		{
			// Check if we got the right version
			if (wsaData.wVersion != version)
			{
				WSACleanup();
				throw std::runtime_error("Incorrect version of Winsock initialized in GameServer. Requested Version: " +
					std::to_string(version) + ". Initialized Version: " + std::to_string(wsaData.wVersion));
			}
		}
	}
#endif
