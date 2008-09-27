#include "ErrorLog.h"


ostream &logFile = cerr;


void LogMessage(char* message)
{
	logFile << message << endl;
}

void LogString(char* message, char* string)
{
	logFile << message << string << endl;
}

void LogValue(char* message, int value)
{
	logFile << message << dec << value << endl;
}

void LogValueHex(char* message, int value)
{
	logFile << message << hex << value << endl;
}

void LogEndpoint(char* message, unsigned long ipAddr, unsigned short port)
{
	union
	{
		unsigned long ipAddr;
		struct
		{
			unsigned char b1;
			unsigned char b2;
			unsigned char b3;
			unsigned char b4;
		};
	} ip;

	ip.ipAddr = ipAddr;

	logFile << message << (unsigned int)ip.b1 << "." << (unsigned int)ip.b2 << "." << (unsigned int)ip.b3 << "." << (unsigned int)ip.b4 
			<< ":" << port << endl;
}

void LogCounters(GameServerCounters &counters)
{
	static GameServerCounters oldCounters = { 0 };

	// Check if updates need to be printed
	if (memcmp(&counters, &oldCounters, sizeof(counters)) == 0)
		return;

	// Store the current counters
	oldCounters = counters;

	// Output the counters
	// Game counters
	logFile << " Games: (Hosted: " << counters.numGamesHosted
			<< ", Started: " << counters.numGamesStarted
			<< ", Cancelled: " << counters.numGamesCancelled
			<< ", Dropped: " << counters.numGamesDropped << ")" << endl;
	// Traffic counters
	logFile << " Traffic: (PacketRecv: " << counters.numPacketsReceived
			<< ", ByteRecv: " << counters.numBytesReceived
			<< ", PacketSent: " << counters.numPacketsSent
			<< ", ByteSent: " << counters.numBytesSent << ")" << endl;
	// Performance counters
	logFile << " Performance: (DropHostPoke:" << counters.numDroppedHostedPokes
			<< ", UpdateRequestSent:" << counters.numUpdateRequestSent
			<< ", RetrySent:" << counters.numRetrySent << ")" << endl;
}


/*
void LogCounters(GameServerCounters &counters)
{
	static GameServerCounters oldCounters = { 0 };

	// Check if updates need to be printed
	if (memcmp(&counters, &oldCounters, sizeof(counters)) == 0)
		return;

	// Store the current counters
	oldCounters = counters;

	// Output the counters
	// Game counters
	logFile << endl;
	logFile << "counters.numGamesHosted   : " << counters.numGamesHosted << endl;
	logFile << "counters.numGamesStarted  : " << counters.numGamesStarted << endl;
	logFile << "counters.numGamesCancelled: " << counters.numGamesCancelled << endl;
	logFile << "counters.numGamesDropped  : " << counters.numGamesDropped << endl;
	// Traffic counters
	logFile << "counters.numPacketsReceived: " << counters.numPacketsReceived << endl;
	logFile << "counters.numBytesReceived  : " << counters.numBytesReceived << endl;
	logFile << "counters.numPacketsSent    : " << counters.numPacketsSent << endl;
	logFile << "counters.numBytesSent      : " << counters.numBytesSent << endl;
	// Performance counters
	logFile << "counters.numDroppedHostedPokes :" << counters.numDroppedHostedPokes << endl;
	logFile << "counters.numUpdateRequestSent  :" << counters.numUpdateRequestSent << endl;
	logFile << "counters.numRetrySent          :" << counters.numRetrySent << endl;
	logFile << "counters.numNewHost            :" << counters.numNewHost << endl;
	// Receive Error counts
	logFile << "counters.numMinSizeErrors      :" << counters.numMinSizeErrors << endl;
	logFile << "counters.numSizeFieldErrors    :" << counters.numSizeFieldErrors << endl;
	logFile << "counters.numTypeFieldErrors    :" << counters.numTypeFieldErrors << endl;
	logFile << "counters.numChecksumFieldErrors:" << counters.numChecksumFieldErrors << endl;
	// Error counts
	logFile << "counters.numFailedGameInfoAllocs:" << counters.numFailedGameInfoAllocs << endl;
	logFile << endl;
}
*/
