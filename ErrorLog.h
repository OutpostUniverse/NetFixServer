#ifdef WIN32
	#include <iostream.h>
#else
	#include <iostream>
#endif
#include "GameServer.h"


extern std::ostream &logFile;


void LogMessage(const char* message);
void LogString(const char* message, const char* string);
void LogValue(const char* message, int value);
void LogValueHex(const char* message, int value);
void LogEndpoint(const char* message, unsigned long ipAddr, unsigned short port);

void LogCounters(GameServerCounters &counters);
