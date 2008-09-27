#include "ErrorLog.h"
#include "GameServer.h"

int main(int argc, char **argv)
{
	int errorCode;
	GameServer gameServer;
	int portNum = 47777;

	// Check for command line parameters **TODO**

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
