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


const char PortNumParam[] = "PortNum=";


int main(int argc, char **argv)
{
	int errorCode;
	GameServer gameServer;
	int portNum = 47800;
	int stringLen;
	int i;

	stringLen = strlen(PortNumParam);
	// Check for command line parameters
	for (i = 0; i < argc; i++)
	{
		if (strncmp(argv[i], PortNumParam, stringLen) == 0)
		{
			portNum = atoi(&argv[i][stringLen]);
		}
	}

	// Start the game server
	errorCode = gameServer.StartServer(portNum);
	// Check for errors
	if (errorCode != 0)
	{
		LogMessage("Game Server failed to start");
		return errorCode;
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
