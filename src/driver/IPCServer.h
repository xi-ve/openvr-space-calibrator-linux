#pragma once

#include "../common/Protocol.h"

#include <thread>
#include <set>
#include <mutex>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>

class ServerTrackedDeviceProvider;

class IPCServer
{
public:
	IPCServer(ServerTrackedDeviceProvider *driver) : driver(driver) { }
	~IPCServer();

	void Run();
	void Stop();

private:
	void HandleRequest(const protocol::Request &request, protocol::Response &response);

	struct SocketInstance
	{
		int socket;
		IPCServer *server;
		protocol::Request request;
		protocol::Response response;
		bool reading;
	};

	SocketInstance *CreateSocketInstance(int socket);
	void CloseSocketInstance(SocketInstance *socketInst);

	static void RunThread(IPCServer *_this);
	static int CreateAndBindSocket();
	static void SetNonBlocking(int fd);

	std::thread mainThread;

	bool running = false;
	bool stop = false;

	std::set<SocketInstance *> sockets;
	std::mutex socketsMutex;

	int serverSocket = -1;
	int epollFd = -1;

	ServerTrackedDeviceProvider *driver;
};

