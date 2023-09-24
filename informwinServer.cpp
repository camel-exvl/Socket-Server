#include <cstdio>
#include <winsock2.h>

#define SERVER_PORT 3722 //侦听端口

//客户端向服务器传送的结构：
struct student {
	char name[32];
	int age;
};

SOCKET sListen;    //侦听套接字

DWORD WINAPI connectThread(LPVOID lpParam);
void shutdownServer();

int main() {
	WORD wVersionRequested;
	WSADATA wsaData;
	SOCKET sServer;   //连接套接字
	int ret, length;
	struct sockaddr_in saServer{}, saClient{};  //地址信息

	//WinSock初始化：
	wVersionRequested = MAKEWORD(2, 2); //希望使用的WinSock DLL的版本
	ret = WSAStartup(wVersionRequested, &wsaData);
	if (ret != 0) {
		printf("WSAStartup() failed!\n");
		return 0;
	}
	//确认WinSock DLL支持版本2.2：
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
		WSACleanup();
		printf("Invalid Winsock version!\n");
		return -1;
	}

	//创建socket，使用TCP协议：
	sListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sListen == INVALID_SOCKET) {
		WSACleanup();
		printf("socket() failed!\n");
		return 0;
	}
	atexit(shutdownServer);

	//构建本地地址信息：
	saServer.sin_family = AF_INET;//地址家族
	saServer.sin_port = htons(SERVER_PORT);//注意转化为网络字节序
	saServer.sin_addr.S_un.S_addr = htonl(INADDR_ANY);//使用INADDR_ANY指示任意地址

	//绑定：
	ret = bind(sListen, (struct sockaddr *) &saServer, sizeof(saServer));
	if (ret == SOCKET_ERROR) {
		printf("bind() failed! code:%d\n", WSAGetLastError());
		closesocket(sListen);//关闭套接字
		WSACleanup();
		return 0;
	}

	//侦听连接请求：
	ret = listen(sListen, SOMAXCONN);
	if (ret == SOCKET_ERROR) {
		printf("listen() failed! code:%d\n", WSAGetLastError());
		closesocket(sListen);//关闭套接字
		WSACleanup();
		return 0;
	}

	printf("Waiting for client connecting!\n");

	while (true) {
		//阻塞等待接受客户端连接：
		length = sizeof(saClient);
		sServer = accept(sListen, (struct sockaddr *) &saClient, &length);
		if (sServer == INVALID_SOCKET) {
			printf("accept() failed! code:%d\n", WSAGetLastError());
			continue;
		}
		printf("Accepted client: %s:%d\n",
		       inet_ntoa(saClient.sin_addr), ntohs(saClient.sin_port));
		CreateThread(nullptr, 0, connectThread, (LPVOID) sServer, 0, nullptr);
	}
}

DWORD WINAPI connectThread(LPVOID lpParam) {
	printf("Thread started!\n");
	SOCKET sServer = (SOCKET) lpParam;
	int ret, nLeft, length;
	struct student stu{};
	char *ptr;

	//按照预定协议，客户端将发来一个学生的信息：
	nLeft = sizeof(stu);
	ptr = (char *) &stu;
	while (nLeft > 0) {
		//接收数据：
		ret = recv(sServer, ptr, nLeft, 0);
		if (ret == SOCKET_ERROR) {
			printf("recv() failed!\n");
			break;
		}

		if (ret == 0) //客户端已经关闭连接
		{
			printf("client has closed the connection!\n");
			break;
		}
		nLeft -= ret;
		ptr += ret;
	}

	if (!nLeft) //已经接收到了所有数据
		printf("name: %s\nage:%d\n", stu.name, stu.age);

	closesocket(sServer);
	return 0;
}

void shutdownServer() {
	printf("Server is shutting down!\n");
	closesocket(sListen);//关闭套接字
	WSACleanup();
}