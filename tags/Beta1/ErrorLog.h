#ifdef WIN32
	#include <iostream.h>
#else
	#include <iostream>
	using namespace std;
#endif
#include "GameServer.h"


extern ostream &logFile;


void LogMessage(char* message);
void LogString(char* message, char* string);
void LogValue(char* message, int value);
void LogValueHex(char* message, int value);
void LogEndpoint(char* message, unsigned long ipAddr, unsigned short port);

void LogCounters(GameServerCounters &counters);
