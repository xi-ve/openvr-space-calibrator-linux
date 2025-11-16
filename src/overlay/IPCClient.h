#pragma once

#include "../common/Protocol.h"

class IPCClient
{
public:
	~IPCClient();

	void Connect();
	protocol::Response SendBlocking(const protocol::Request &request);

	void Send(const protocol::Request &request);
	protocol::Response Receive();

private:
	int sockfd = -1;
};

