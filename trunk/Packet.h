
#ifndef Packet_H_Seen
#define Packet_H_Seen



#ifdef WIN32
	#include <winsock2.h>
#else
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <string.h>
	#define SOCKET int

	struct GUID
	{
		unsigned long Data1;
		unsigned short Data2;
		unsigned short Data3;
		unsigned char Data4[8];

		bool operator != (GUID &guid)
		{
			return (memcmp(this, &guid, sizeof(guid)) != 0);
		}
	};
#endif


// Game Start Settings struct
struct StartupFlags
{
	int bDisastersOn:1;
	int bDayNightOn:1;
	int bMoraleOn:1;
	int bCampaign:1;
	int bMultiplayer:1;
	int bCheatsOn:1;
	unsigned int maxPlayers:3;
	int missionType:5;
	int b1:3;						// **
	unsigned int numInitialVehicles:3;
};


// Everything in this file should be byte aligned
#ifdef WIN32
#pragma pack(push, 1)
#define Pack
#else
#define Pack __attribute__((packed))
#endif


// ----------------------------------------

// ------
// Header
// ------

struct PacketHeader
{
	int sourcePlayerNetID Pack;
	int destPlayerNetID Pack;
	unsigned char sizeOfPayload;
	unsigned char type;
	int checksum Pack;
};



// ------------------------
// Transport Layer messages
// ------------------------

enum TransportLayerCommandType
{
	tlcJoinRequest = 0,
	tlcHostedGameSearchQuery = 7,
	tlcHostedGameSearchReply = 8,
	tlcGameServerPoke = 9,
	tlcJoinHelpRequest = 10,
	tlcRequestExternalAddress = 11,
	tlcEchoExternalAddress = 12,
};


// ----------------------------------------

// ---------------
// Payload formats
// ---------------

struct TransportLayerHeader
{
	TransportLayerCommandType commandType Pack;
};


// ----------------------------------------


struct JoinRequest : public TransportLayerHeader
{
	GUID sessionIdentifier Pack;
	int returnPortNum Pack;			// [47800-47807]
	char password[12];
};


// ---------------------
// Custom Packet formats
// ---------------------

// [Nested structure]
struct CreateGameInfo
{
	StartupFlags startupFlags Pack;
	char gameCreatorName[15];
};

struct HostedGameSearchQuery : public TransportLayerHeader
{
	GUID gameIdentifier Pack;
	unsigned int timeStamp Pack;
	char password[12];
};

struct HostedGameSearchReply : public TransportLayerHeader
{
	GUID gameIdentifier Pack;
	unsigned int timeStamp Pack;
	GUID sessionIdentifier Pack;
	CreateGameInfo createGameInfo;
	sockaddr_in hostAddress Pack;
};


enum PokeStatusCode
{
	pscGameHosted = 0,
	pscGameStarted = 1,
	pscGameCancelled = 2,
};

struct GameServerPoke : public TransportLayerHeader
{
	int statusCode Pack;
	int randValue Pack;
};

struct JoinHelpRequest : public TransportLayerHeader
{
	GUID sessionIdentifier Pack;
	int returnPortNum Pack;			// [47800-47807]
	char password[12];
	sockaddr_in clientAddr Pack;
};

struct RequestExternalAddress : public TransportLayerHeader
{
	unsigned short internalPort Pack;
};

struct EchoExternalAddress : public TransportLayerHeader
{
	unsigned short replyPort Pack;
	sockaddr_in addr Pack;
};


// ----------------------------------------

union TransportLayerMessage
{
	// Header only
	TransportLayerHeader tlHeader;

	// Custom messages
	JoinRequest joinRequest;
	HostedGameSearchQuery searchQuery;
	HostedGameSearchReply searchReply;
	GameServerPoke gameServerPoke;
	JoinHelpRequest joinHelpRequest;
	RequestExternalAddress requestExternalAddress;
	EchoExternalAddress echoExternalAddress;
};


// ----------------------------------------

// ------
// Packet
// ------

class Packet
{
public:
	// Member variables
	PacketHeader header;
	union
	{
		TransportLayerMessage tlMessage;
	};

public:
	// Member functions
	int Checksum();
};


// Restore old alignment setting
#ifdef WIN32
#pragma pack(pop)
#endif


#endif
