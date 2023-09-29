#include "Server.h"

bool Server::isRunning = true;

int main() {
	try {
		Server server;
	} catch (std::exception &e) {
		printf("%s\n", e.what());
	}
	return 0;
}

DWORD WINAPI connectThread(LPVOID lpParam) noexcept {
	auto param = (std::pair<Server *, int> *) lpParam;
	try {
		param->first->connectToClient(param->second);
	} catch (std::exception &e) {
		printf("Thread %d: %s\n", param->second, e.what());
	}
	return 0;
}

Server::Server() {
	try {
		initWinSock();
	} catch (std::exception &e) {
		throw e;
	}
	try {
		createSocket();
		bindSocket();
		listenSocket();
		printf("Waiting for client connecting...\n");
		acceptSocket();
	} catch (std::exception &e) {
		printf("%s\n", e.what());
	}
}

Server::~Server() {
	printf("Server is shutting down.\n");
	closesocket(sListen);//关闭套接字
	WSACleanup();
}

void Server::initWinSock() {
	//WinSock初始化：
	WORD wVersionRequested = MAKEWORD(2, 2); //希望使用的WinSock DLL的版本
	WSADATA wsaData;
	int ret = WSAStartup(wVersionRequested, &wsaData);
	if (ret != 0) {
		throw std::runtime_error("WSAStartup() failed.");
	}
	//确认WinSock DLL支持版本2.2：
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
		WSACleanup();
		throw std::runtime_error("Invalid Winsock version.");
	}
}

void Server::createSocket() {
	//创建socket，使用TCP协议：
	sListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sListen == INVALID_SOCKET) {
		WSACleanup();
		throw std::runtime_error("socket() failed.");
	}
}

void Server::bindSocket() const {
	struct sockaddr_in saServer{};  //地址信息
	//构建本地地址信息：
	saServer.sin_family = AF_INET;//地址家族
	saServer.sin_port = htons(SERVER_PORT);//注意转化为网络字节序
	saServer.sin_addr.S_un.S_addr = htonl(INADDR_ANY);//使用INADDR_ANY指示任意地址

	//绑定：
	int ret = bind(sListen, (struct sockaddr *) &saServer, sizeof(saServer));
	if (ret == SOCKET_ERROR) {
		closesocket(sListen);//关闭套接字
		WSACleanup();
		throw std::runtime_error("bind() failed, code:" + std::to_string(WSAGetLastError()));
	}
}

void Server::listenSocket() const {
	//侦听连接请求：
	int ret = listen(sListen, SOMAXCONN);
	if (ret == SOCKET_ERROR) {
		closesocket(sListen);//关闭套接字
		WSACleanup();
		throw std::runtime_error("listen() failed, code:" + std::to_string(WSAGetLastError()));
	}
}

void Server::acceptSocket() {
	SOCKET sServer;   //连接套接字
	struct sockaddr_in saClient{};  //地址信息
	int iMode = 1;
	ioctlsocket(sListen, FIONBIO, (u_long FAR *) &iMode);
	signal(SIGINT, signalHandler);
	while (isRunning) {
		// 非阻塞等待接受客户端连接：
		int length = sizeof(saClient);
		sServer = accept(sListen, (struct sockaddr *) &saClient, &length);
		if (sServer == INVALID_SOCKET) {
//				printf("accept() failed! code:%d\n", WSAGetLastError());
			continue;
		}
		printf("Accepted client: %s:%d\n", inet_ntoa(saClient.sin_addr), ntohs(saClient.sin_port));
		static int clientIndex = 0;
		clients.emplace_back(
				Client{clientIndex++, ConnectStatus::CONNECTED, sServer, inet_ntoa(saClient.sin_addr),
				       ntohs(saClient.sin_port)});
		std::pair<Server *, int> param{this, clients.size() - 1};
		auto ret = CreateThread(nullptr, 0, connectThread, (LPVOID) &param, 0, nullptr);
		if (ret == nullptr) {
			throw std::runtime_error("CreateThread() " + std::to_string(clients.size() - 1) + " failed, code:" +
			                         std::to_string(WSAGetLastError()));
		}
	}
}

void Server::connectToClient(int index) noexcept {
	auto &curClient = clients[index];
	auto &sServer = curClient.socket;
	printf("Thread %d started.\n", curClient.index);

	// send hello
	char helloData[MAXBUFLEN];
	int helloLen = Package::serialize(PackageType::STRING, "Hello from server!\0", helloData);
	int ret = send(sServer, helloData, helloLen, 0);
	if (ret == SOCKET_ERROR) {
		printf("Thread %d: send() failed, code:%d\n", index, WSAGetLastError());
		clients[index].status = ConnectStatus::DISCONNECTED;
		closesocket(sServer);
		return;
	}
	printf("Thread %d: hello sent.\n", index);

	// receive
	char buffer[MAXBUFLEN];
	bool error = false;
	while (isRunning) {
		// receive length
		int nLeft = sizeof(int);
		char *ptr = buffer;
		while (nLeft > 0) {
			ret = recv(sServer, ptr, nLeft, 0);
			if (ret == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK) {
				printf("Thread %d: recv() failed, code:%d\n", index, WSAGetLastError());
				error = true;
				break;
			}
			if (ret == 0) {
				printf("Thread %d: client has closed the connection.\n", index);
				break;
			}
			nLeft -= ret;
			ptr += ret;
		}
		if (ret == 0 || error) {
			break;
		}
		int len = *(int *) buffer;

		// receive data
		nLeft = len;
		ptr = buffer + sizeof(int);
		while (nLeft > 0) {
			ret = recv(sServer, ptr, nLeft, 0);
			if (ret == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK) {
				printf("Thread %d: recv() failed, code:%d\n", index, WSAGetLastError());
				error = true;
				break;
			}
			if (ret == 0) {
				printf("Thread %d: client has closed the connection.\n", index);
				break;
			}
			nLeft -= ret;
			ptr += ret;
		}
		if (ret == 0 || error) {
			break;
		}
		buffer[len] = '\0';

		printf("Thread %d: received %d bytes: %s\n", index, len, buffer);
		try {
			handleDataInput(index, buffer);
		} catch (std::exception &e) {
			printf("%s\n", e.what());
			break;
		}
	}
	clients[index].status = ConnectStatus::DISCONNECTED;
	closesocket(sServer);
	printf("Thread %d: connection closed.\n", index);
	return;
}

void Server::handleDataInput(int index, const char *input) {
	char data[MAXBUFLEN];
	int len;
	PackageType type;
	void *field;
	Package::deserialize(type, field, input);
	switch (type) {
		case (PackageType::STRING): {
			std::string command = (char *) field;
			if (command == "date") {
				time_t t = time(nullptr);
				len = Package::serialize(PackageType::TIME, &t, data);
			} else if (command == "hostname") {
				char buffer[1024];
				gethostname(buffer, sizeof(buffer));
				len = Package::serialize(PackageType::STRING, buffer, data);
			} else if (command == "client-list") {
				len = Package::serialize(PackageType::CLIENTS, &clients, data);
			} else {
				len = Package::serialize(PackageType::STRING, "Unknown command.\0", data);
			}
			break;
		}
		case (PackageType::FORWARD): {
			ForwardRequest request = *(ForwardRequest *) field;
			if (request.to >= clients.size()) {
				len = Package::serialize(PackageType::STRING, "Invalid client index.\0", data);
			} else if (clients[request.to].status != ConnectStatus::CONNECTED) {
				len = Package::serialize(PackageType::STRING, "Client disconnected.\0", data);
			} else {
				int ret = send(clients[request.to].socket, request.data, strlen(request.data), 0);
				if (ret == SOCKET_ERROR) {
					std::string err = "Send to client " + std::to_string(request.to) + " failed. Code:" +
					                  std::to_string(WSAGetLastError());
					len = Package::serialize(PackageType::STRING, err.c_str(), data);
				} else {
					std::string msg = "Sent to client " + std::to_string(request.to) + " successfully.";
					len = Package::serialize(PackageType::STRING, msg.c_str(), data);
				}
			}
			break;
		}
		default: {
			if (type == PackageType::INVALID) {
				len = Package::serialize(PackageType::STRING, (char *) field, data);
			} else {
				len = Package::serialize(PackageType::STRING, "Invalid request.\0", data);
			}
			break;
		}
	}

	auto &curClient = clients[index];
	int ret = send(curClient.socket, data, len, 0);
	if (ret == SOCKET_ERROR) {
		throw std::runtime_error("Thread " + std::to_string(index) + ": send() failed, code:" +
		                         std::to_string(WSAGetLastError()));
	}
	printf("Thread %d: %d bytes sent.\n", index, ret);
}

void Server::signalHandler(int signum) noexcept {
	printf("Interrupt signal (%d) received.\n", signum);
	isRunning = false;
}