#include <stdio.h>
#include <winsock2.h>
#include "../package.h"

const char *ip = "10.162.83.210";
WORD wVersionRequested;
WSADATA wsaData;
int ret;
SOCKET sClient; //连接套接字
struct sockaddr_in saServer;//地址信息
BOOL fSuccess = TRUE;
std::vector<Client> clients;

void receive();
void handleDataInput(const char *input);
void sendToServer(char *data, int len);

int main() {
//	if (argc != 4) {
//		printf("usage: informWinClient serverIP name age\n");
//		return 0;
//	}

	//WinSock初始化：
	wVersionRequested = MAKEWORD(2, 2);//希望使用的WinSock DLL的版本
	ret = WSAStartup(wVersionRequested, &wsaData);
	if (ret != 0) {
		printf("WSAStartup() failed!\n");
		return 0;
	}
	//确认WinSock DLL支持版本2.2：
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
		WSACleanup();
		printf("Invalid Winsock version!\n");
		return 0;
	}

	//创建socket，使用TCP协议：
	sClient = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sClient == INVALID_SOCKET) {
		WSACleanup();
		printf("socket() failed!\n");
		return 0;
	}

	//构建服务器地址信息：
	saServer.sin_family = AF_INET;//地址家族
	saServer.sin_port = htons(SERVER_PORT);//注意转化为网络字节序
	saServer.sin_addr.S_un.S_addr = inet_addr(ip);


	//连接服务器：
	ret = connect(sClient, (struct sockaddr *) &saServer, sizeof(saServer));
	if (ret == SOCKET_ERROR) {
		printf("connect() failed!\n");
		closesocket(sClient);//关闭套接字
		WSACleanup();
		return 0;
	}

	receive();
	char data[MAXBUFLEN];
	int len = Package::serialize(PackageType::STRING, "date", data);
	sendToServer(data, len);
	receive();
	len = Package::serialize(PackageType::STRING, "hostname", data);
	sendToServer(data, len);
	receive();
	len = Package::serialize(PackageType::STRING, "client-list", data);
	sendToServer(data, len);
	receive();
	if (clients.size() > 10) {
		ForwardRequest request;
		request.to = clients[0].index;
		request.from = 0;
		request.sender_addr = "";
		request.sender_port = 0;
		request.data = new char[MAXBUFLEN];
		Package::serialize(PackageType::STRING, "hello from another client", request.data);
		len = Package::serialize(PackageType::FORWARD, &request, data);
		delete request.data;
		sendToServer(data, len);
		receive();
	} else {
		for (;;) {
			receive();
		}
	}

	closesocket(sClient);//关闭套接字
	WSACleanup();
	system("pause");
}

void sendToServer(char *data, int len) {
	ret = send(sClient, data, len, 0);
	if (ret == SOCKET_ERROR) {
		throw std::runtime_error("send() failed, code:" + std::to_string(WSAGetLastError()));
	}
	printf("send %d bytes\n", ret);
}

void receive() {
	char buffer[MAXBUFLEN];
	bool error = false;
	bool isRunning = true;
	while (isRunning) {
		isRunning = false;
		// receive length
		int nLeft = sizeof(int);
		char *ptr = buffer;
		int ret;
		while (nLeft > 0) {
			ret = recv(sClient, ptr, nLeft, 0);
			if (ret == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK) {
				printf("recv() failed, code:%d\n", WSAGetLastError());
				error = true;
				break;
			} else if (ret == 0) {
				printf("client has closed the connection.\n");
				break;
			} else if (ret != SOCKET_ERROR) {
				nLeft -= ret;
				ptr += ret;
			}
		}
		if (ret == 0 || error) {
			break;
		}
		int len = *(int *) buffer;

		// receive data
		nLeft = len - sizeof(int);
		ptr = buffer + sizeof(int);
		while (nLeft > 0) {
			ret = recv(sClient, ptr, nLeft, 0);
			if (ret == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK) {
				printf("recv() failed, code:%d\n", WSAGetLastError());
				error = true;
				break;
			} else if (ret == 0) {
				printf("client has closed the connection.\n");
				break;
			} else if (ret != SOCKET_ERROR) {
				nLeft -= ret;
				ptr += ret;
			}
		}
		if (ret == 0 || error) {
			break;
		}
		buffer[len] = '\0';

		printf("received %d bytes\n", len);
		try {
			handleDataInput(buffer);
		} catch (std::exception &e) {
			printf("%s\n", e.what());
			break;
		}
	}
}

void handleDataInput(const char *input) {
	PackageType type;
	void *field;
	Package::deserialize(type, field, input);
	printf("type: %d\n", type);
	switch (type) {
		case PackageType::TIME: {
			printf("field: %lld\n", *(time_t *) field);
			break;
		}
		case PackageType::STRING: {
			printf("field: %s\n", (char *) field);
			break;
		}
		case PackageType::CLIENTS: {
			clients = *(std::vector<Client> *) field;
			for (auto &client: clients) {
				printf("client %d: %s:%d\n", client.index, client.addr.c_str(), client.port);
			}
			break;
		}
		case PackageType::FORWARD: {
			ForwardRequest request = *(ForwardRequest *) field;
			printf("request: %d -> %d\n", request.from, request.to);
			printf("sender: %s:%d\n", request.sender_addr.c_str(), request.sender_port);
			printf("data:\n");
			handleDataInput(request.data);
			break;
		}
		default:
			printf("Invalid package type.\n");
	}
}