#include <common.h>
#include <mutex>
#include "compat.h"
#include <thread>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
using namespace forwarding;
using namespace std;

once_flag g_initflag;

void init_transport_once() {
	std::call_once(g_initflag, []() {init_transport(); });
}

forwarding::Listener::Listener(int port)
{
	init_transport_once();
	_listeningSocket = socket(AF_INET, SOCK_STREAM, 0);
	sockaddr_in addr{ 0 };
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (0 != ::bind(_listeningSocket.Get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
		throw TransportErrorException{ TransportError::BindFailed };
	}
	if (0 != listen(_listeningSocket.Get(), SOMAXCONN)) {
		throw TransportErrorException{ TransportError::ListenFailed };
	}
}

forwarding::Listener::Listener(const char* address, int port)
{
	init_transport_once();
	auto resolved = Resolve(address, port);
	_listeningSocket = socket(AF_INET, SOCK_STREAM, 0);
	int yes = 1;
	setsockopt(_listeningSocket.Get(), SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes));
#ifndef _WIN32
	setsockopt(_listeningSocket.Get(), SOL_SOCKET, SO_REUSEPORT, (char*)&yes, sizeof(yes));
#endif
	if (0 != ::bind(_listeningSocket.Get(), resolved->SockAddr(), resolved->SockAddrLen())) {
		throw TransportErrorException{ TransportError::BindFailed };
	}
	if (0 != listen(_listeningSocket.Get(), SOMAXCONN)) {
		throw TransportErrorException{ TransportError::ListenFailed };
	}
}

void forwarding::Listener::StartAcceptLoop(const std::function<void(unique_ptr<Connection>)>& callback){
	for (;;) {
		auto rawSock = accept(_listeningSocket.Get(), nullptr, nullptr);
		if (INVALID_SOCKET == rawSock) {
			return;
		}
		SafeSocket clientSock = rawSock;
		callback( make_unique<Connection>(std::move(clientSock)));
	}
}

forwarding::Connection::Connection(SafeSocket && s) :_socket(std::move(s))
{
}

void forwarding::Connection::Send(const BufferView & buf)
{
	if (send(_socket.Get(), buf.begin(), buf.size(), 0) != buf.size()) {
		throw TransportErrorException{ TransportError::SendReceiveFailed };
	}
}

void forwarding::Connection::Receive(BufferView & buf)
{
#ifndef _WIN32
	pollfd fds;
	fds.fd = _socket.Get();
	fds.events = POLLIN | POLLPRI;
	fds.revents = 0;
	if (poll(&fds, 1, -1) <= 0) {
		throw TransportErrorException{ TransportError::SendReceiveFailed };
	}
#endif
	auto receivedAmount = recv(_socket.Get(), buf.begin(), buf.size(), MSG_WAITALL);
	if (receivedAmount != buf.size()) {
		throw TransportErrorException{ TransportError::SendReceiveFailed };
	}
}

std::unique_ptr<Connection> forwarding::ConnectTo(const ResolvedAddress& address)
{
	init_transport_once();
	SafeSocket s = socket(AF_INET, SOCK_STREAM, 0);
	if (0 != connect(s.Get(), address.SockAddr(), address.SockAddrLen())) {
		throw TransportErrorException{ TransportError::ConnectFailed };
	}
	return std::make_unique<Connection>(std::move(s));
}


forwarding::TransportErrorException::TransportErrorException(TransportError e) : Error(e)
{
	fprintf(stderr, "TransortErrorException %d\n", (int)e);
}

std::unique_ptr<Connection> forwarding::ConnectTo(const ResolvedAddress& address, std::chrono::milliseconds timeout)
{
	init_transport_once();
	SafeSocket s = socket(AF_INET, SOCK_STREAM, 0);
	unsigned long nonBlocking = 1, blocking = 0;
	ioctlsocket(s.Get(), FIONBIO, &nonBlocking);
	auto secs = timeout.count() / 1000;
	auto msecs = timeout.count() % 1000;
	if (0 != connect(s.Get(), address.SockAddr(), address.SockAddrLen())) {
		auto err = WSAGetLastError();
		if (err != WSAEWOULDBLOCK) {
			throw TransportErrorException{ TransportError::ConnectFailed };
		}
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(s.Get(), &fds);
		timeval timeout;
		timeout.tv_sec = static_cast<long>(secs);
		timeout.tv_usec = static_cast<long>(1000 * msecs);
		if (select(1, nullptr, &fds, nullptr, &timeout) != 1) {
			throw TransportErrorException{ TransportError::ConnectFailed };
		}
	}
	ioctlsocket(s.Get(), FIONBIO, &blocking);
	return std::make_unique<Connection>(std::move(s));
}


void forwarding::init_transport() {
	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 0), &WSAData);
}

class WindowsResolvedAddress : public ResolvedAddress {
private:
	addrinfo* _info;
public:
	WindowsResolvedAddress(addrinfo* info):_info(info){}
	virtual ~WindowsResolvedAddress() {
		freeaddrinfo(_info);
	}

	virtual const sockaddr* SockAddr()const override {
		return _info->ai_addr;
	}
	virtual int SockAddrLen() const  override {
		return (int)_info->ai_addrlen;
	}
};

std::unique_ptr<ResolvedAddress> forwarding::Resolve(const char * hostName, int port)
{	
	init_transport_once();
	auto sPort = std::to_string(port);
	addrinfo hints;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	addrinfo* results = nullptr;

	if (getaddrinfo(hostName, sPort.c_str(), &hints, &results) != 0) {
		throw TransportErrorException{ TransportError::NameResolutionFailed };
	}
	if (results == nullptr) {
		throw TransportErrorException{ TransportError::NameResolutionFailed };
	}
	return std::make_unique<WindowsResolvedAddress>(results);
}
std::unique_ptr<ResolvedAddress> forwarding::ResolveUdp(const char * hostName, int port)
{
	init_transport_once();
	auto sPort = std::to_string(port);
	addrinfo hints;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	addrinfo* results = nullptr;

	if (getaddrinfo(hostName, sPort.c_str(), &hints, &results) != 0) {
		throw TransportErrorException{ TransportError::NameResolutionFailed };
	}
	if (results == nullptr) {
		throw TransportErrorException{ TransportError::NameResolutionFailed };
	}
	return std::make_unique<WindowsResolvedAddress>(results);
}
