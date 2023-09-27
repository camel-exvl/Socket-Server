#ifndef LAB5_PACKAGE_H
#define LAB5_PACKAGE_H

#include <winsock2.h>
#include <string>
#include <stdexcept>
#include <vector>

#define SERVER_PORT 3722 // 侦听端口
#define MAXBUFLEN 1024  // 最大缓冲区长度

enum class ConnectStatus {  // 客户端连接状态
	DISCONNECTED, CONNECTED
};

enum class PackageType {    // 数据包类型
	INVALID, TIME, STRING, CLIENTS, FORWARD
};

typedef struct {
	int index;
	ConnectStatus status;
	SOCKET socket;
	std::string addr;
	int port;
} Client;   // 客户端信息

typedef struct {
	int to;
	char *data;
} ForwardRequest;   // 转发请求包

class Package { // 数据包
public:
	// Serialize to buffer
	// [in] type: package type
	// [in] field: field to serialize
	// [out] buffer: buffer to serialize
	// [in] maxBufLen: max length of buffer
	// return the length of written data
	static int serialize(const PackageType &type, const void *field, char *buffer, int maxBufLen = MAXBUFLEN);
	// Deserialize from buffer
	// [out] type: package type
	// [out] field: field to deserialize
	// [in] buffer: buffer to deserialize
	// return the length of read data
	static int deserialize(PackageType &type, void *field, const char *&buffer) noexcept;

private:
	// time_t
	static int serialize(const time_t &field, char *buffer, int maxLen);
	static int deserialize(time_t *field, const char *&buffer, int len) noexcept;
	// char *
	static int serialize(const char *field, char *buffer, int maxLen);
	static int deserialize(char *field, const char *&buffer, int len) noexcept;
	// vector<Client>
	static int serialize(const std::vector<Client> &field, char *buffer, int maxLen);
	static int deserialize(std::vector<Client> *field, const char *&buffer, int len);
	// ForwardRequest
	static int serialize(const ForwardRequest &field, char *buffer, int maxLen);
	static int deserialize(ForwardRequest *field, const char *&buffer, int len) noexcept;
};

#endif //LAB5_PACKAGE_H
