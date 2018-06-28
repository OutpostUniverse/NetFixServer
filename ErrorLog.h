#ifdef WIN32
	#include <iostream.h>
#else
	#include <iostream>
	using namespace std;
#endif
#include "GameServer.h"


extern ostream &logFile;


void LogMessage(const char* message);
void LogString(const char* message, const char* string);
void LogValue(const char* message, int value);
void LogValueHex(const char* message, int value);
void LogEndpoint(const char* message, unsigned long ipAddr, unsigned short port);

void LogCounters(GameServerCounters &counters);
