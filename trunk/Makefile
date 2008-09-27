OPUNetGameServer: main.o GameServer.o Packet.o ErrorLog.o
	g++ -o OPUNetGameServer main.o GameServer.o Packet.o ErrorLog.o

main.o: main.cpp GameServer.h
	g++ -c main.cpp

GameServer.o: GameServer.cpp GameServer.h ErrorLog.h
	g++ -c GameServer.cpp

Packet.o: Packet.cpp Packet.h
	g++ -c Packet.cpp

ErrorLog.o: ErrorLog.cpp ErrorLog.h GameServer.h
	g++ -c ErrorLog.cpp

