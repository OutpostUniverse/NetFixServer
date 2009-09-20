// **DEBUG**
//#define DEBUG
#include "ErrorLog.h"

#ifndef WIN32
	#define closesocket close
	#define ioctlsocket ioctl
#endif

#include "GameServer.h"


const unsigned int InitialGameListSize = 64;
const unsigned int GrowByGameListSize = 32;
const unsigned int UpdateTime = 60;			// Number of seconds before requesting update
const unsigned int RetryTime = 64;			// Number of seconds before requesting update retry
const unsigned int GiveUpTime = 68;			// Number of seconds before clearing dead entries
const unsigned int InitialReplyTime = 4;	// Number of seconds allowed for first update
GUID gameIdentifier = { 0x5A55CF11, 0xB841, 0x11CE, 0x92, 0x10, 0x00, 0xAA, 0x00, 0x6C, 0x49, 0x72 };



GameServer::GameServer()
{
	// Initialize network socket descriptor
	hostSocket = INVALID_SOCKET;
	// Initialize GameInfo list pointer
	gameInfo = 0;
	numGames = 0;
	maxNumGames = 0;
	// Clear counters
	memset(&counters, 0, sizeof(counters));

	// Perform Win32 specific initialization
	#ifdef WIN32
		bWinsockInitialized = false;
	#endif
}


GameServer::~GameServer()
{
	// Check if a socket exists
	if (hostSocket != INVALID_SOCKET)
		closesocket(hostSocket);

	// Perform Win32 specific cleanup
	#ifdef WIN32
		// Check if Winsock is initialized
		if (bWinsockInitialized != false)
			WSACleanup();
	#endif

	// Cleanup the games list
	delete[] gameInfo;
}



int GameServer::StartServer(unsigned short port)
{
	int errorCode;

	#ifdef WIN32
		// Initialize Winsock
		errorCode = InitWinsock();
		// Check for errors
		if (errorCode != 0)
			return errorCode;				// Error
	#endif

	// Create the host socket
	errorCode = AllocSocket(hostSocket, port);
	if (errorCode != NoError)
		return errorCode;
	// Create the secondary socket
	errorCode = AllocSocket(secondarySocket, port + 1);
	if (errorCode != NoError)
		return errorCode;

	// Allocate space to store games list
	numGames = 0;
	maxNumGames = InitialGameListSize;
	gameInfo = new GameInfo[maxNumGames];
	// Check for errors
	if (gameInfo == 0)
		return AllocGameListFailed;			// Error

	return NoError;							// Success
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
		if (numBytes >= 0)
			ProcessPacket(packet, from);
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
		if (numBytes == -1)
			return;		// Exit
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
	select(1, &readfds, 0, 0, &timeOut);
}


int GameServer::AllocSocket(SOCKET& hostSocket, unsigned short port)
{
	int errorCode;

	// Create the host socket
	hostSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	// Check for errors
	if (hostSocket == INVALID_SOCKET)
		return SocketCreateFailed;			// Error

	// Bind the socket to the host port
	sockaddr_in hostAddress;
	// Initialize the struct
	memset(hostAddress.sin_zero, 0, sizeof(hostAddress.sin_zero));
	hostAddress.sin_family = AF_INET;
	hostAddress.sin_port = htons(port);
	hostAddress.sin_addr.s_addr = INADDR_ANY;
	// Bind the socket
	errorCode = bind(hostSocket, (sockaddr*)&hostAddress, sizeof(hostAddress));
	// Check for errors
	if (errorCode == SOCKET_ERROR)
		return SocketBindFailed;			// Error

	// Set non-blocking mode
	unsigned long argp = true;
	errorCode = ioctlsocket(hostSocket, FIONBIO, &argp);
	// Check for errors
	if (errorCode != 0)
		return SocketNonBlockingModeFailed;	// Error

	// Return success
	return NoError;
}

void GameServer::ProcessPacket(Packet &packet, sockaddr_in &from)
{
	unsigned int gameInfoIndex;
	GameInfo* currentGameInfo;

	#ifdef DEBUG
		//LogMessage("ProcessPacket");
		//LogValue("CommandType: ", packet.tlMessage.tlHeader.commandType);
	#endif

	switch(packet.tlMessage.tlHeader.commandType)
	{
	case tlcJoinRequest:
		// Verify the packet size
		if (packet.header.sizeOfPayload != sizeof(JoinRequest))
			return;		// Discard (bad size)
		
		LogEndpoint("Game Join Request from: ", from.sin_addr.s_addr, from.sin_port);

		packet.tlMessage.tlHeader.commandType = tlcJoinHelpRequest;
		packet.tlMessage.joinHelpRequest.clientAddr = from;
		packet.tlMessage.joinHelpRequest.clientAddr.sin_family = 2;		// ** AF_INET

		// Search for a corresponding Session Identifer
		for (gameInfoIndex = 0; gameInfoIndex < numGames; gameInfoIndex++)
		{
			// Check if the Session Identifier matches
			if (memcmp(&gameInfo[gameInfoIndex].sessionIdentifier, &packet.tlMessage.joinRequest.sessionIdentifier, sizeof(packet.tlMessage.joinRequest.sessionIdentifier)) == 0)
			{
				// Send the Join Help Request
				SendTo(packet, gameInfo[gameInfoIndex].addr);
			}
		}

		break;
	case tlcHostedGameSearchQuery:
		// Verify packet size
		if (packet.header.sizeOfPayload != sizeof(HostedGameSearchQuery))
			return;		// Discard (bad size)
		// Verify the game identifier
		if (packet.tlMessage.searchQuery.gameIdentifier != gameIdentifier)
			return;		// Discard (wrong game)

		LogEndpoint("Game Search Query from: ", from.sin_addr.s_addr, from.sin_port);

		// Send client a list of games
		// Setup common packet fields
		packet.header.sizeOfPayload = sizeof(HostedGameSearchReply);
		packet.tlMessage.tlHeader.commandType = tlcHostedGameSearchReply;
		// Search games list for suitable games
		unsigned int i;
		for (i = 0; i < numGames; ++i)
		{
			// Make sure we have valid game data
			if ((gameInfo[i].flags & GameInfoReceived) != 0)
			{
				LogString("  GameCreator: ", gameInfo[i].createGameInfo.gameCreatorName);

				// Consruct a reply packet for this game
				packet.tlMessage.searchReply.sessionIdentifier = gameInfo[i].sessionIdentifier;
				packet.tlMessage.searchReply.createGameInfo = gameInfo[i].createGameInfo;
				packet.tlMessage.searchReply.hostAddress = gameInfo[i].addr;
				packet.tlMessage.searchReply.hostAddress.sin_family = 2;		// ** AF_INET
				// Send the reply
				SendTo(packet, from);
			}
		}

		break;
	case tlcHostedGameSearchReply:
		// Verify packet size
		if (packet.header.sizeOfPayload != sizeof(HostedGameSearchReply))
			return;		// Discard (bad size)
		// Make sure we queried this server
		gameInfoIndex = FindGameInfoServer(from, packet.tlMessage.searchReply.timeStamp);
		if (gameInfoIndex == -1)
			return;		// Discard (not requested or bad time stamp, possible spam or spoofing attack)
		currentGameInfo = &gameInfo[gameInfoIndex];

		LogEndpoint("Received Host Info from: ", from.sin_addr.s_addr, from.sin_port);

		// Add the game to the list
		currentGameInfo->sessionIdentifier = packet.tlMessage.searchReply.sessionIdentifier;
		currentGameInfo->addr = from;
		currentGameInfo->flags |= GameInfoReceived;
		currentGameInfo->flags &= ~GameInfoExpected & ~GameInfoUpdateRetrySent;
		currentGameInfo->createGameInfo = packet.tlMessage.searchReply.createGameInfo;
		currentGameInfo->time = time(0);

		break;
	case tlcGameServerPoke:
		// Verify packet size
		if (packet.header.sizeOfPayload != sizeof(GameServerPoke))
			return;		// Discard (bad size)

		#ifdef DEBUG
			//LogValue("Poke: ", packet.tlMessage.gameServerPoke.statusCode);
		#endif

		// Find the current GameInfo
		gameInfoIndex = FindGameInfoClient(from, packet.tlMessage.gameServerPoke.randValue);

		// Check what kind of poke this is
		switch(packet.tlMessage.gameServerPoke.statusCode)
		{
		case pscGameHosted:
			// Check if this game server is not already known
			if (gameInfoIndex == -1)
			{
				// Update counters
				counters.numNewHost++;
				gameInfoIndex = GetNewGameInfo();	// Allocate a new record
			}
			// Make sure we have a record to use
			if (gameInfoIndex == -1)
			{
				// Update counters
				counters.numFailedGameInfoAllocs++;
				return;			// Abort  (failed to allocate new record)
			}
			currentGameInfo = &gameInfo[gameInfoIndex];

			LogEndpoint("Game Hosted from: ", from.sin_addr.s_addr, from.sin_port);

			// Initialize the new record
			currentGameInfo->addr = from;
			currentGameInfo->clientRandValue = packet.tlMessage.gameServerPoke.randValue;
			currentGameInfo->serverRandValue = GetNewRandValue();
			currentGameInfo->flags |= GameInfoExpected;
			currentGameInfo->time = time(0);

			// Send a request for games
			SendGameInfoRequest(from, currentGameInfo->serverRandValue);

			// Update counters
			counters.numGamesHosted++;

			break;
		case pscGameStarted:
			LogEndpoint("Game Started: ", from.sin_addr.s_addr, from.sin_port);

			// Remove the game from the list
			FreeGameInfo(gameInfoIndex);
			// Update counters
			counters.numGamesStarted++;

			break;
		case pscGameCancelled:
			LogEndpoint("Game Cancelled: ", from.sin_addr.s_addr, from.sin_port);

			// Remove the game from the list
			FreeGameInfo(gameInfoIndex);
			// Update counters
			counters.numGamesCancelled++;

			break;
		}
		break;
	case tlcRequestExternalAddress:
		// Verify packet size
		if (packet.header.sizeOfPayload != sizeof(RequestExternalAddress))
			return;		// Discard (bad size)

		// Cache the internal port being used
		unsigned short internalPort = packet.tlMessage.requestExternalAddress.internalPort;

		// Construct a reply
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
			from.sin_port = internalPort;
			SendTo(packet, from);
		}

		break;
	}
}


void GameServer::DoTimedUpdates()
{
	int i;
	time_t currentTime;
	unsigned int timeDiff;

	#ifdef DEBUG
		//LogMessage("DoTimedUpdates()");
	#endif

	// Get the current time
	currentTime = time(0);
	// Check for timed out game entries
	for (i = numGames - 1; i >= 0; --i)
	{
		// Get the current time difference
		timeDiff = currentTime - gameInfo[i].time;

		// Check for no initial update within required time
		if ((timeDiff >= InitialReplyTime) && ((gameInfo[i].flags & GameInfoReceived) == 0))
		{
			LogEndpoint("Dropping Game: No initial Host Info from: ", gameInfo[i].addr.sin_addr.s_addr, gameInfo[i].addr.sin_port);

			// Clear bogus entry
			FreeGameInfo(i);
			// Update counters
			counters.numDroppedHostedPokes++;
		}
		// Check if no updates have occured for a while
		else if ((timeDiff >= UpdateTime) && ((gameInfo[i].flags & GameInfoReceived) != 0))
		{
			// Entry is old and requires update
			// --------------------------------

			// Check if enough time has elapsed to give up
			if (timeDiff >= GiveUpTime)
			{
				LogEndpoint("Dropping Game: Lost contact with host: ", gameInfo[i].addr.sin_addr.s_addr, gameInfo[i].addr.sin_port);

				// Give up
				FreeGameInfo(i);
				// Update counters
				counters.numGamesDropped++;
			}
			else if ((gameInfo[i].flags & GameInfoExpected) == 0)
			{
				LogEndpoint("Requesting Game info update 1 (periodic): ", gameInfo[i].addr.sin_addr.s_addr, gameInfo[i].addr.sin_port);

				// Game info is stale, request update
				SendGameInfoRequest(gameInfo[i].addr, gameInfo[i].serverRandValue);
				// Update flags
				gameInfo[i].flags |= GameInfoExpected;
				// Update counters
				counters.numUpdateRequestSent++;
			}
			else if ((timeDiff >= RetryTime) && ((gameInfo[i].flags & GameInfoUpdateRetrySent) == 0))
			{
				LogEndpoint("Requesting Game info update 2 (retry): ", gameInfo[i].addr.sin_addr.s_addr, gameInfo[i].addr.sin_port);

				// Assume the packet was dropped. Retry.
				SendGameInfoRequest(gameInfo[i].addr, gameInfo[i].serverRandValue);
				// Update flags
				gameInfo[i].flags |= GameInfoUpdateRetrySent;
				// Update counters
				counters.numRetrySent++;
			}
		}
	}

	// Check if we should reduce memory usage **TODO**

	// Output Log Info
	LogCounters(counters);
}


int GameServer::FindGameInfoClient(sockaddr_in &from, unsigned int clientRandValue)
{
	unsigned int i;

	// Search games list
	for (i = 0; i < numGames; ++i)
	{
		// Check if this address matches
		if ((gameInfo[i].clientRandValue == clientRandValue) && (memcmp(&gameInfo[i].addr, &from, sizeof(sockaddr_in)) == 0))
		{
			// Return the GameInfo
			return i;
		}
	}

	// GameInfo not found
	return -1;
}


int GameServer::FindGameInfoServer(sockaddr_in &from, unsigned int serverRandValue)
{
	unsigned int i;

	// Search games list
	for (i = 0; i < numGames; ++i)
	{
		// Check if this address matches
		if ((gameInfo[i].serverRandValue == serverRandValue) && (memcmp(&gameInfo[i].addr, &from, sizeof(sockaddr_in)) == 0))
		{
			// Return the GameInfo
			return i;
		}
	}

	// GameInfo not found
	return -1;
}

int GameServer::GetNewGameInfo()
{
	// Check if we need to allocate more space
	if ((gameInfo == 0) || (numGames >= maxNumGames))
	{
		// Allocate more space
		int newSize = maxNumGames + GrowByGameListSize;
		GameInfo* newGameInfo = new GameInfo[newSize];
		// Check for errors
		if (newGameInfo == 0)
			return -1;			// Abort (Failed, could not allocate space)

		// Copy the old info to the new array
		memcpy(newGameInfo, gameInfo, numGames * sizeof(GameInfo));

		// Update the array info
		delete[] gameInfo;
		gameInfo = newGameInfo;
		maxNumGames = newSize;
	}

	// Clear the flags of the new entry
	gameInfo[numGames].flags = 0;

	// Return the next free entry (and increment count)
	return numGames++;
}


void GameServer::FreeGameInfo(unsigned int index)
{
	// Make sure it's a valid index
	if (index >= numGames)
	{
		// System Error **TODO** report this
		#ifdef DEBUG
			LogMessage("Internal Error: Tried to free an non-existent GameInfo record");
		#endif
		return;		// Abort
	}

	// Decrease game count
	numGames--;
	// Avoid the copy if we don't need to
	if (index != numGames)
		gameInfo[index] = gameInfo[numGames];	// Copy the last array element into the free space
}


unsigned int GameServer::GetNewRandValue()
{
	return 0;		// **TODO**
}


int GameServer::ReceiveFrom(Packet &packet, sockaddr_in &from)
{
	unsigned int numBytes;
	socklen_t addrLen = sizeof(from);

	// Read any received packets
	numBytes = recvfrom(hostSocket, (char*)&packet, sizeof(packet), 0, (sockaddr*)&from, &addrLen);
	// Check for errors
	if (numBytes == SOCKET_ERROR)
	{
		// Try the secondary socket
		numBytes = recvfrom(secondarySocket, (char*)&packet, sizeof(packet), 0, (sockaddr*)&from, &addrLen);

		// Check for errors
		if (numBytes == SOCKET_ERROR)
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

	// Update counters
	counters.numPacketsReceived++;
	counters.numBytesReceived += numBytes;

	return numBytes;
}


void GameServer::SendTo(Packet &packet, sockaddr_in &to)
{
	int errorCode;
	int size = packet.header.sizeOfPayload + sizeof(packet.header);
	int toLen = sizeof(to);

	// Calculate the packet checksum
	packet.header.checksum = packet.Checksum();

	// Send the packet
	errorCode = sendto(hostSocket, (char*)&packet, size, 0, (sockaddr*)&to, toLen);

	// Check for errors
	if (errorCode == SOCKET_ERROR)
	{
		// Error  **TODO** Update counters
		#ifdef DEBUG
			LogMessage("Error: SendTo socket error");
		#endif
	}
	else
	{
		// Success.  Update counters
		counters.numPacketsSent++;
		counters.numBytesSent += size;
	}
}


void GameServer::SendGameInfoRequest(sockaddr_in &to, unsigned int serverRandValue)
{
	Packet packet;

	// Consntruct a Hosted Game Search Query packet
	packet.header.sourcePlayerNetID = 0;
	packet.header.destPlayerNetID = 0;
	packet.header.type = 1;
	packet.header.sizeOfPayload = sizeof(HostedGameSearchQuery);
	packet.tlMessage.tlHeader.commandType = tlcHostedGameSearchQuery;
	packet.tlMessage.searchQuery.gameIdentifier = gameIdentifier;
	packet.tlMessage.searchQuery.timeStamp = serverRandValue;
	memset(packet.tlMessage.searchQuery.password, 0, sizeof(packet.tlMessage.searchQuery.password));

	// Send the packet
	SendTo(packet, to);
}


#ifdef WIN32
	int GameServer::InitWinsock()
	{
		// Make sure Winsock was not already initialized
		if (bWinsockInitialized != false)
		{
			return WinsockInitFailed;			// Error: Already initialized
		}

		// Initialize Winsock
		WSADATA wsaData;
		int errorCode;
		WORD version = MAKEWORD(2, 2);

		// Initialize Winsock
		errorCode = WSAStartup(version, &wsaData);
		// Check for success
		if (errorCode == 0)
		{
			// Check if we got the right version
			if (wsaData.wVersion != version)
			{
				// Wrong version
				WSACleanup();
				return WinsockVersionFailed;	// Error: Wrong Winsock version
			}
		}

		// Winsock Initialized successfully
		bWinsockInitialized = true;
		return NoError;							// Success
	}
#endif

