#include <cstdio>
#include <winsock2.h>
#include <vector>
#include <stdexcept>
#include <csignal>
#include <ctime>

#define SERVER_PORT 3722 //侦听端口

////客户端向服务器传送的结构：
//struct student {
//	char name[32];
//	int age;
//};

enum class ConnectStatus {
	DISCONNECTED, CONNECTED
};

typedef struct {
	int len;
	char *data;
} Packet;

typedef struct {
	int index;
	ConnectStatus status;
	SOCKET socket;
	std::string addr;
	int port;
} Client;

class Server {
public:
	Server() {
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

	~Server() {
		printf("Server is shutting down.\n");
		closesocket(sListen);//关闭套接字
		WSACleanup();
	}

	void initWinSock() {
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

	void createSocket() {
		//创建socket，使用TCP协议：
		sListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sListen == INVALID_SOCKET) {
			WSACleanup();
			throw std::runtime_error("socket() failed.");
		}
	}

	void bindSocket() const {
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

	void listenSocket() const {
		//侦听连接请求：
		int ret = listen(sListen, SOMAXCONN);
		if (ret == SOCKET_ERROR) {
			closesocket(sListen);//关闭套接字
			WSACleanup();
			throw std::runtime_error("listen() failed, code:" + std::to_string(WSAGetLastError()));
		}
	}

	void acceptSocket() {
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
			auto ret = CreateThread(nullptr, 0, connectThread, (LPVOID) &clients.back(), 0, nullptr);
			if (ret == nullptr) {
				throw std::runtime_error("CreateThread() " + std::to_string(clients.size() - 1) + " failed, code:" +
				                         std::to_string(WSAGetLastError()));
			}
		}
	}

	static DWORD WINAPI connectThread(LPVOID lpParam) {
		auto curClient = (Client *) lpParam;
		auto sServer = curClient->socket;
		printf("Thread %d started.\n", curClient->index);

		// send hello
		char helloString[] = "Hello from server!\0";
		Packet hello{0, helloString};
		hello.len = strlen(helloString);
		int ret = send(sServer, hello.data, hello.len, 0);
		if (ret == SOCKET_ERROR) {
			throw std::runtime_error("send() failed, code:" + std::to_string(WSAGetLastError()));
		}
		printf("Thread %d: Hello sent.\n", curClient->index);

		// receive
		char buffer[1024];
		while (isRunning) {
			// receive length
			int nLeft = sizeof(int);
			char *ptr = buffer;
			while (nLeft > 0) {
				ret = recv(sServer, ptr, nLeft, 0);
				if (ret == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK) {
					printf("Thread %d: recv() failed, code:%d\n", curClient->index, WSAGetLastError());
					break;
				}
				if (ret == 0) {
					printf("Thread %d: client has closed the connection.\n", curClient->index);
					break;
				}
				nLeft -= ret;
				ptr += ret;
			}
			if (ret == 0) {
				break;
			}
			int len = *(int *) buffer;

			// receive data
			nLeft = len;
			ptr = buffer;
			while (nLeft > 0) {
				ret = recv(sServer, buffer, len, 0);
				if (ret == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK) {
					printf("Thread %d: recv() failed, code:%d\n", curClient->index, WSAGetLastError());
					break;
				}
				if (ret == 0) {
					printf("Thread %d: client has closed the connection.\n", curClient->index);
					break;
				}
				nLeft -= ret;
				ptr += ret;
			}
			if (ret == 0) {
				break;
			}
			buffer[len] = '\0';

			printf("Thread %d: received %d bytes: %s\n", curClient->index, len, buffer);
			std::string s(buffer);
			try {
				handleDataInput(curClient, s);
			} catch (std::exception &e) {
				printf("%s\n", e.what());
				break;
			}
		}
		closesocket(sServer);
		return 0;
	}

	static void handleDataInput(Client *&curClient, std::string &s) {
		Packet packet{0, nullptr};  // the packet need to be sent
		if (s == "date") {
			time_t now = time(nullptr);
			packet.data = ctime(&now);
			packet.len = strlen(packet.data);
		} else {
			char unknownString[] = "Unknown command!\0";
			packet.data = unknownString;
			packet.len = strlen(unknownString);
		}

		int ret = send(curClient->socket, packet.data, packet.len, 0);
		if (ret == SOCKET_ERROR) {
			throw std::runtime_error("Thread " + std::to_string(curClient->index) + ": send() failed, code:" +
			                         std::to_string(WSAGetLastError()));
		}
		printf("Thread %d: %d bytes sent.\n", curClient->index, ret);
	}

	static void signalHandler(int signum) {
		printf("Interrupt signal (%d) received.\n", signum);
		isRunning = false;
	}

private:
	SOCKET sListen{};    //侦听套接字
	std::vector<Client> clients;
	static bool isRunning;
};

bool Server::isRunning = true;

int main() {
	try {
		Server server;
	} catch (std::exception &e) {
		printf("%s\n", e.what());
	}
	return 0;
}


