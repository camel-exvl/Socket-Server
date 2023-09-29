#ifndef LAB5_SERVER_H
#define LAB5_SERVER_H

#include <cstdio>
#include <csignal>
#include <ctime>
#include "package.h"

DWORD WINAPI connectThread(LPVOID lpParam) noexcept;

class Server {
public:
	Server();
	~Server();
	static void initWinSock();
	void createSocket();
	void bindSocket() const;
	void listenSocket() const;
	void acceptSocket();
	void connectToClient(int index) noexcept;
	void handleDataInput(int index, const char *input);
	static void signalHandler(int signum) noexcept;
private:
	SOCKET sListen{};    //侦听套接字
	std::vector<Client> clients;
	static bool isRunning;
};

#endif //LAB5_SERVER_H
