#include "package.h"

int Package::serialize(const PackageType &type, const void *field, char *buffer, int maxBufLen) {
	char *ptr = buffer + sizeof(int);
	memcpy(ptr, &type, sizeof(PackageType));
	ptr += sizeof(PackageType);

	int len = sizeof(int) + sizeof(PackageType);
	try {
		switch (type) {
			case PackageType::TIME: {
				len += Package::serialize(*(time_t *) field, ptr, maxBufLen - len);
				break;
			}
			case PackageType::STRING: {
				len += Package::serialize((char *) field, ptr, maxBufLen - len);
				break;
			}
			case PackageType::CLIENTS: {
				len += Package::serialize(*(std::vector<Client> *) field, ptr, maxBufLen - len);
				break;
			}
			case PackageType::FORWARD: {
				len += Package::serialize(*(ForwardRequest *) field, ptr, maxBufLen - len);
				break;
			}
		}
	} catch (std::exception &e) {
		throw std::runtime_error(e.what());
	}
	memcpy(buffer, &len, sizeof(int));
	return len;
}

int Package::deserialize(PackageType &type, void *field, const char *&buffer) noexcept {
	int len = *(int *) buffer - sizeof(int) - sizeof(PackageType);
	if (len <= 0) {
		type = PackageType::INVALID;
		field = (void *) "Invalid buffer length.";
		return -1;
	}
	const char *ptr = buffer + sizeof(int);
	type = *(PackageType *) ptr;
	ptr += sizeof(PackageType);
	switch (type) {
		case PackageType::TIME: {
			Package::deserialize((time_t *) field, ptr, len);
			break;
		}
		case PackageType::STRING: {
			Package::deserialize((char *) field, ptr, len);
			break;
		}
		case PackageType::CLIENTS: {
			try {
				Package::deserialize((std::vector<Client> *) field, ptr, len);
			}
			catch (std::exception &e) {
				type = PackageType::INVALID;
				field = (void *) e.what();
				return -1;
			}
			break;
		}
		case PackageType::FORWARD: {
			Package::deserialize((ForwardRequest *) field, ptr, len);
			break;
		}
	}
	return *(int *) buffer;
}

int Package::serialize(const time_t &field, char *buffer, int maxLen) {
	if (maxLen < sizeof(time_t)) {
		throw std::runtime_error("Buffer space not enough.");
	}
	memcpy(buffer, &field, sizeof(time_t));
	return sizeof(PackageType);
}

int Package::deserialize(time_t *field, const char *&buffer, int len) noexcept {
	memcpy(field, buffer + sizeof(int), len);
	return len;
}

int Package::serialize(const char *field, char *buffer, int maxLen) {
	int len = strlen(field);
	if (maxLen < len) {
		throw std::runtime_error("Buffer space not enough.");
	}
	memcpy(buffer, field, len);
	return len;
}

int Package::deserialize(char *field, const char *&buffer, int len) noexcept {
	field = new char[len + 1];
	memcpy(field, buffer + sizeof(int), len);
	field[len] = '\0';
	return len;
}

int Package::serialize(const std::vector<Client> &field, char *buffer, int maxLen) {
	int totalLen = 0;
	char *ptr = buffer;
	for (auto client: field) {
		int curLen =
				sizeof(int) + sizeof(int) + sizeof(ConnectStatus) + sizeof(int) + client.addr.length() + sizeof(int);
		totalLen += curLen;
		if (totalLen > maxLen) {
			throw std::runtime_error("Buffer space not enough.");
		}
		memcpy(ptr, &curLen, sizeof(int));
		ptr += sizeof(int);
		memcpy(ptr, &client.index, sizeof(int));
		ptr += sizeof(int);
		memcpy(ptr, &client.status, sizeof(ConnectStatus));
		ptr += sizeof(ConnectStatus);
		int addrLen = Package::serialize(PackageType::STRING, client.addr.c_str(), ptr);
		ptr += addrLen;
		memcpy(ptr, &client.port, sizeof(int));
		ptr += sizeof(int);
	}
	return totalLen;
}

int Package::deserialize(std::vector<Client> *field, const char *&buffer, int len) {
	int totalLen = len;
	const char *ptr = buffer;
	while (totalLen > 0) {
		int curLen = *(int *) ptr;
		totalLen -= curLen;
		if (totalLen < 0) {
			break;
		}
		ptr += sizeof(int);
		Client client;
		memcpy(&client.index, ptr, sizeof(int));
		ptr += sizeof(int);
		memcpy(&client.status, ptr, sizeof(ConnectStatus));
		ptr += sizeof(ConnectStatus);
		client.socket = INVALID_SOCKET;
		char addr[MAXBUFLEN];
		const char *addrPtr = ptr;
		PackageType type;
		int addrLen = Package::deserialize(type, addr, addrPtr);
		client.addr = addr;
		ptr += addrLen;
		memcpy(&client.port, ptr, sizeof(int));
		ptr += sizeof(int);
		field->push_back(client);
	}
	if (totalLen < 0) {
		throw std::runtime_error("Invalid buffer length");
	}
	return len;
}

int Package::serialize(const ForwardRequest &field, char *buffer, int maxLen) {
	int len = sizeof(int);
	memcpy(buffer, &field.to, sizeof(int));
	try {
		len += serialize(PackageType::STRING, field.data, buffer + sizeof(int), maxLen - sizeof(int));
	} catch (std::exception &e) {
		throw std::runtime_error(e.what());
	}
	return len;
}

int Package::deserialize(ForwardRequest *field, const char *&buffer, int len) noexcept {
	memcpy(&field->to, buffer, sizeof(int));
	const char *ptr = buffer + sizeof(int);
	memcpy(field->data, ptr, len - sizeof(int));
	return len;
}
