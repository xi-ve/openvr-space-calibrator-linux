#include "IPCServer.h"
#include "ServerTrackedDeviceProvider.h"
#include <cstring>
#include <cerrno>
#include <cstdio>

void IPCServer::HandleRequest(const protocol::Request &request, protocol::Response &response)
{
	switch (request.type)
	{
	case protocol::RequestHandshake:
		response.type = protocol::ResponseHandshake;
		response.protocol.version = protocol::Version;
		break;

	case protocol::RequestSetDeviceTransform:
		driver->SetDeviceTransform(request.setDeviceTransform);
		response.type = protocol::ResponseSuccess;
		break;

	case protocol::RequestDebugOffset:
		driver->HandleApplyRandomOffset();
		response.type = protocol::ResponseSuccess;
		break;

	case protocol::RequestSetAlignmentSpeedParams:
		driver->HandleSetAlignmentSpeedParams(request.setAlignmentSpeedParams);
		response.type = protocol::ResponseSuccess;
		break;

	default:
		fprintf(stderr, "Invalid IPC request: %d\n", request.type);
		break;
	}
}

IPCServer::~IPCServer()
{
	Stop();
}

void IPCServer::Run()
{
	mainThread = std::thread(RunThread, this);
}

void IPCServer::Stop()
{
	if (!running)
		return;

	stop = true;
	if (epollFd >= 0) {
		close(epollFd);
		epollFd = -1;
	}
	if (mainThread.joinable()) {
		mainThread.join();
	}
	running = false;
}

IPCServer::SocketInstance *IPCServer::CreateSocketInstance(int socket)
{
	auto socketInst = new SocketInstance;
	socketInst->socket = socket;
	socketInst->server = this;
	socketInst->reading = true;
	
	std::lock_guard<std::mutex> lock(socketsMutex);
	sockets.insert(socketInst);
	return socketInst;
}

void IPCServer::CloseSocketInstance(SocketInstance *socketInst)
{
	if (socketInst->socket >= 0) {
		close(socketInst->socket);
	}
	
	std::lock_guard<std::mutex> lock(socketsMutex);
	sockets.erase(socketInst);
	delete socketInst;
}

void IPCServer::SetNonBlocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0) {
		return;
	}
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int IPCServer::CreateAndBindSocket()
{
	unlink(OPENVR_SPACECALIBRATOR_SOCKET_PATH);

	int sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) {
		return -1;
	}

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, OPENVR_SPACECALIBRATOR_SOCKET_PATH, sizeof(addr.sun_path) - 1);

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(sock);
		return -1;
	}

	if (listen(sock, 5) < 0) {
		close(sock);
		return -1;
	}

	SetNonBlocking(sock);
	return sock;
}

void IPCServer::RunThread(IPCServer *_this)
{
	_this->running = true;

	_this->serverSocket = CreateAndBindSocket();
	if (_this->serverSocket < 0) {
		fprintf(stderr, "Failed to create IPC server socket: %s\n", strerror(errno));
		_this->running = false;
		return;
	}

	_this->epollFd = epoll_create1(0);
	if (_this->epollFd < 0) {
		fprintf(stderr, "Failed to create epoll: %s\n", strerror(errno));
		close(_this->serverSocket);
		_this->running = false;
		return;
	}

	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = _this->serverSocket;
	if (epoll_ctl(_this->epollFd, EPOLL_CTL_ADD, _this->serverSocket, &ev) < 0) {
		fprintf(stderr, "Failed to add server socket to epoll: %s\n", strerror(errno));
		close(_this->epollFd);
		close(_this->serverSocket);
		_this->running = false;
		return;
	}

	struct epoll_event events[64];

	while (!_this->stop)
	{
		int nfds = epoll_wait(_this->epollFd, events, 64, 100);
		if (nfds < 0) {
			if (errno == EINTR) {
				continue;
			}
			fprintf(stderr, "epoll_wait failed: %s\n", strerror(errno));
			break;
		}

		for (int i = 0; i < nfds; i++) {
			if (events[i].data.fd == _this->serverSocket) {
				struct sockaddr_un clientAddr;
				socklen_t clientLen = sizeof(clientAddr);
				int clientSocket = accept(_this->serverSocket, (struct sockaddr *)&clientAddr, &clientLen);
				
				if (clientSocket >= 0) {
					SetNonBlocking(clientSocket);
					auto socketInst = _this->CreateSocketInstance(clientSocket);
					
					struct epoll_event clientEv;
					clientEv.events = EPOLLIN | EPOLLET;
					clientEv.data.ptr = socketInst;
					epoll_ctl(_this->epollFd, EPOLL_CTL_ADD, clientSocket, &clientEv);
				}
			} else {
				SocketInstance *socketInst = (SocketInstance *)events[i].data.ptr;
				
				if (events[i].events & (EPOLLERR | EPOLLHUP)) {
					epoll_ctl(_this->epollFd, EPOLL_CTL_DEL, socketInst->socket, nullptr);
					_this->CloseSocketInstance(socketInst);
					continue;
				}

				if (socketInst->reading) {
					ssize_t bytesRead = recv(socketInst->socket, &socketInst->request, sizeof(protocol::Request), 0);
					
					if (bytesRead <= 0) {
						if (bytesRead == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
							epoll_ctl(_this->epollFd, EPOLL_CTL_DEL, socketInst->socket, nullptr);
							_this->CloseSocketInstance(socketInst);
						}
						continue;
					}

					if (bytesRead == sizeof(protocol::Request)) {
						_this->HandleRequest(socketInst->request, socketInst->response);
						socketInst->reading = false;
						
						struct epoll_event ev;
						ev.events = EPOLLOUT | EPOLLET;
						ev.data.ptr = socketInst;
						epoll_ctl(_this->epollFd, EPOLL_CTL_MOD, socketInst->socket, &ev);
					}
				} else {
					ssize_t bytesWritten = send(socketInst->socket, &socketInst->response, sizeof(protocol::Response), 0);
					
					if (bytesWritten < 0) {
						if (errno != EAGAIN && errno != EWOULDBLOCK) {
							epoll_ctl(_this->epollFd, EPOLL_CTL_DEL, socketInst->socket, nullptr);
							_this->CloseSocketInstance(socketInst);
						}
						continue;
					}

					if (bytesWritten == sizeof(protocol::Response)) {
						socketInst->reading = true;
						
						struct epoll_event ev;
						ev.events = EPOLLIN | EPOLLET;
						ev.data.ptr = socketInst;
						epoll_ctl(_this->epollFd, EPOLL_CTL_MOD, socketInst->socket, &ev);
					}
				}
			}
		}
	}

	std::lock_guard<std::mutex> lock(_this->socketsMutex);
	for (auto socketInst : _this->sockets) {
		close(socketInst->socket);
		delete socketInst;
	}
	_this->sockets.clear();

	if (_this->serverSocket >= 0) {
		close(_this->serverSocket);
		unlink(OPENVR_SPACECALIBRATOR_SOCKET_PATH);
	}
	if (_this->epollFd >= 0) {
		close(_this->epollFd);
	}

	_this->running = false;
}

