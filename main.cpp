// To start server with logging to a file:
// screen
// ./OPUNetGameServer 2>&1 | tee log.txt
//
// This redirects stderr to stdout, and then pipes the output of stdout to tee.
// Tee will output stdout to the screen as well as to the file given.
// This is like a T-connector for splitting TV signals.
// The screen session is needed to keep the server running after logging out.
// Normally programs are terminated when the login shell that spawns them exits.
//
// An optional port number can also be specified on the command line, but this is not recommended.


#include "ErrorLog.h"
#include "GameServer.h"
#include <cstdlib>
#include <string>
#include <exception>


const char PortNumParam[] = "PortNum=";


int main(int argc, char **argv)
{
	int portNum = 47800;

	int stringLen = strlen(PortNumParam);
	// Check for command line parameters
	for (int i = 0; i < argc; i++)
	{
		if (strncmp(argv[i], PortNumParam, stringLen) == 0)
		{
			portNum = atoi(&argv[i][stringLen]);
		}
	}

	GameServer gameServer;
	try {
		gameServer.StartServer(portNum);
	}
	catch (const std::exception& e) {
		LogMessage("Game Server failed to start: " + std::string(e.what()));
		return 1;
	}

	// Show what port the server is bound to
	LogValue("Game Server started on port: ", portNum);

	// Enter server loop
	for (;;)
	{
		// Process network messages
		gameServer.Pump();

		// Yield to other processes
		gameServer.WaitForEvent();
	}

	return 0;
}
