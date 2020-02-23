#pragma once

#include "GameServer.h"
#include <iostream>


#include <string_view>
extern std::ostream &logFile;


void LogMessage(std::string_view message);
void LogString(std::string_view message, std::string_view string);
void LogValue(std::string_view message, int value);
void LogValueHex(std::string_view message, int value);
void LogEndpoint(std::string_view message, unsigned long ipAddr, unsigned short port);

void LogCounters(GameServerCounters &counters);
