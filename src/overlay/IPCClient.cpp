#include "IPCClient.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <stdexcept>

IPCClient::~IPCClient()
{
	if (sockfd >= 0) {
		close(sockfd);
		sockfd = -1;
	}
}

void IPCClient::Connect()
{
	sockfd = ::socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd < 0) {
		throw std::runtime_error("Failed to create IPC client socket: " + std::string(strerror(errno)));
	}

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, OPENVR_SPACECALIBRATOR_SOCKET_PATH, sizeof(addr.sun_path) - 1);

	if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(sockfd);
		sockfd = -1;
		throw std::runtime_error("Space Calibrator driver unavailable. Make sure SteamVR is running, and the Space Calibrator addon is enabled in SteamVR settings.");
	}

	auto response = SendBlocking(protocol::Request(protocol::RequestHandshake));
	if (response.type != protocol::ResponseHandshake || response.protocol.version != protocol::Version) {
		close(sockfd);
		sockfd = -1;
		throw std::runtime_error(
			"Incorrect driver version installed, try reinstalling Space Calibrator. (Client: " +
			std::to_string(protocol::Version) +
			", Driver: " +
			std::to_string(response.protocol.version) +
			")"
		);
	}
}

protocol::Response IPCClient::SendBlocking(const protocol::Request &request)
{
	Send(request);
	return Receive();
}

void IPCClient::Send(const protocol::Request &request)
{
	ssize_t bytesWritten = send(sockfd, &request, sizeof(protocol::Request), 0);
	if (bytesWritten < 0) {
		throw std::runtime_error("Error writing IPC request: " + std::string(strerror(errno)));
	}
	if (bytesWritten != sizeof(protocol::Request)) {
		throw std::runtime_error("Partial write of IPC request");
	}
}

protocol::Response IPCClient::Receive()
{
	protocol::Response response(protocol::ResponseInvalid);
	ssize_t bytesRead = recv(sockfd, &response, sizeof(protocol::Response), 0);
	
	if (bytesRead < 0) {
		throw std::runtime_error("Error reading IPC response: " + std::string(strerror(errno)));
	}
	
	if (bytesRead != sizeof(protocol::Response)) {
		throw std::runtime_error("Invalid IPC response size, got " + std::to_string(bytesRead) + " bytes");
	}

	return response;
}

