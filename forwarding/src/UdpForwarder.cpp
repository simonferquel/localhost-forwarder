#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <ostream>
#include <sstream>
#include <string>
#include <client.h>
#include "Forwarders.h"
#include <chrono>
#include <map>
#include <cstring>
#include <windows.h>

using namespace forwarding;
using namespace std;
using namespace std::chrono;

inline const bool operator <(const sockaddr_in& lhs, const sockaddr_in& rhs) {
	return memcmp(&lhs, &rhs, sizeof(sockaddr_in)) < 0;
}
namespace forwarding {
	const seconds ClientTimeout = 30s;

	struct UdpReply {
		sockaddr_in clientAddr;
		vector<char> data;
		UdpReply() = default;
		UdpReply(const UdpReply&) = default;
		UdpReply(UdpReply&&) = default;
		UdpReply& operator =(const UdpReply&) = default;
		UdpReply& operator =(UdpReply&&) = default;
		UdpReply(const sockaddr_in& addr, vector<char>&& data) : clientAddr(addr), data(move(data))
		{}
	};
	
	using UdpRequest = vector<char>;

	struct UdpPair {
		sockaddr_in clientAddr;
		SafeSocket remote;
		vector<UdpRequest> pendingRequests;
		steady_clock::time_point last_activity;
		UdpPair():last_activity(steady_clock::now()){
		}
		UdpPair(const sockaddr_in& clientAddr, SafeSocket&& remoteSock) : clientAddr(clientAddr), remote(move(remoteSock)), last_activity(steady_clock::now()) {
		}
		UdpPair(const UdpPair&) = delete;
		UdpPair(UdpPair&&) = default;
		bool timedOut() const {
			return (steady_clock::now() - last_activity) > ClientTimeout;
		}
		void trySendRequests() {
			while (!pendingRequests.empty()) {
				auto sent = send(remote.Get(), &pendingRequests[0][0], static_cast<int>(pendingRequests[0].size()), 0);
				if (sent <= 0) {
					if (WSAEWOULDBLOCK == WSAGetLastError()) { // can't send in non blocking way anymore
						break;
					}
					// if other error, simply drop the packet (conformly to UDP expecting packet losses)
				}
				last_activity = steady_clock::now();
				pendingRequests.erase(pendingRequests.begin());
			}
		}
		bool tryReadReply(UdpReply& reply) {
			u_long available = 0;
			ioctlsocket(remote.Get(), FIONREAD, &available);
			if (available > 0) {
				reply.data.resize(available);
				auto read = recv(remote.Get(), &reply.data[0], static_cast<int>(reply.data.size()), 0);
				if (read >= 0) {
					reply.clientAddr = clientAddr;
					reply.data.resize(read);
					last_activity = steady_clock::now();
					return true;
				}
			}
			return false;
		}
	};

	struct UdpForwarderEntry {
		uint16_t port;
		SafeSocket localSocket;
		std::unique_ptr<ResolvedAddress> remoteAddr;
		vector<UdpReply> pendingReplies;
		map<sockaddr_in,UdpPair> pairs;
	};

	class UdpForwarder::Impl : public enable_shared_from_this<UdpForwarder::Impl> {
	private:
		SafeAutoResetEvent _localEvent, _remoteEvent;
		std::mutex _mut;
		std::atomic<bool> _running;
		std::thread _runningThread;
		vector<std::unique_ptr<UdpForwarderEntry>> _entries;
		void OnLocalSocketSignaled() {

			std::lock_guard<std::mutex> lg(_mut);
			for (auto& entry : _entries) {
				WSANETWORKEVENTS events;
				WSAEnumNetworkEvents(entry->localSocket.Get(), nullptr, &events);
				if ((events.lNetworkEvents & FD_READ) == FD_READ) {
					sockaddr_in clientAddr;
					int clientAddrLength = sizeof(clientAddr);
					u_long available = 0;
					ioctlsocket(entry->localSocket.Get(), FIONREAD, &available);
					UdpRequest req;
					req.resize(available);
					auto readSize = recvfrom(entry->localSocket.Get(), &req[0], static_cast<int>(req.size()), 0, (sockaddr*)&clientAddr, &clientAddrLength);
					if (readSize >= 0) {
						req.resize(readSize);
						auto pairIt = entry->pairs.find(clientAddr);
						if (pairIt == entry->pairs.end()) {
							SafeSocket remote = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
							if (0 == connect(remote.Get(), entry->remoteAddr->SockAddr(), entry->remoteAddr->SockAddrLen())) {
								WSAEventSelect(remote.Get(), _remoteEvent.get(), FD_READ | FD_WRITE);
								UdpPair p(clientAddr, move(remote));
								p.pendingRequests.push_back(move(req));
								p.trySendRequests();
								entry->pairs.insert(make_pair(clientAddr, move(p)));
							}
						}
						else {
							pairIt->second.pendingRequests.push_back(move(req));
							pairIt->second.trySendRequests();
						}
					}
				}

				// try to send pending replies
				while (!entry->pendingReplies.empty()) {
					auto sent = sendto(entry->localSocket.Get(), &entry->pendingReplies[0].data[0], static_cast<int>(entry->pendingReplies[0].data.size()), 0,
						(const sockaddr*)&entry->pendingReplies[0].clientAddr, sizeof(entry->pendingReplies[0].clientAddr));
					if (sent <= 0) {
						if (WSAEWOULDBLOCK == WSAGetLastError()) { // can't send in non blocking way anymore
							break;
						}
						// if other error, simply drop the packet (conformly to UDP expecting packet losses)
					}
					entry->pendingReplies.erase(entry->pendingReplies.begin());
				}
			}
		}

		void OnRemoteSocketSignaled() {
			std::lock_guard<std::mutex> lg(_mut);
			for (auto& entry : _entries) {
				for (auto& pair : entry->pairs) {					
					WSANETWORKEVENTS events;
					WSAEnumNetworkEvents(pair.second.remote.Get(), nullptr, &events);
					if ((events.lNetworkEvents & FD_READ) == FD_READ) {
						UdpReply reply;
						while (pair.second.tryReadReply(reply)) {
							entry->pendingReplies.push_back(move(reply));
							reply = UdpReply{};
						}
					}
					pair.second.trySendRequests();
				}
				// try to send pending replies
				while (!entry->pendingReplies.empty()) {
					auto sent = sendto(entry->localSocket.Get(), &entry->pendingReplies[0].data[0], static_cast<int>(entry->pendingReplies[0].data.size()), 0,
						(const sockaddr*)&entry->pendingReplies[0].clientAddr, static_cast<int>(sizeof(entry->pendingReplies[0].clientAddr)));
					if (sent <= 0) {
						if (WSAEWOULDBLOCK == WSAGetLastError()) { // can't send in non blocking way anymore
							break;
						}
						// if other error, simply drop the packet (conformly to UDP expecting packet losses)
					}
					entry->pendingReplies.erase(entry->pendingReplies.begin());
				}
			}
		}
	public:
		void Loop() {
			auto lastSweep = steady_clock::now();
			while (_running) {
				HANDLE events[] = { _localEvent.get(), _remoteEvent.get() };
				auto waitResult = WaitForMultipleObjects(2, events, FALSE, static_cast<int>(duration_cast<milliseconds>(ClientTimeout).count()));
				if (!_running) {
					return;
				}
				if (waitResult == WAIT_OBJECT_0) {
					OnLocalSocketSignaled();
				}
				else if (waitResult == WAIT_OBJECT_0 + 1) {
					OnRemoteSocketSignaled();
				}
				if(steady_clock::now()-lastSweep > ClientTimeout)
				{
					lock_guard<mutex> lg(_mut);
					for (auto& entries : _entries) {
						std::vector<sockaddr_in> toRemove;
						for (auto& pair : entries->pairs) {
							if (pair.second.timedOut()) {
								toRemove.push_back(pair.first);
							}
						}
						for (auto& k : toRemove) {
							entries->pairs.erase(k);
						}
					}
					lastSweep = steady_clock::now();
				}
			}
		}
		Impl() : _localEvent(MakeAutoResetEvent()), _remoteEvent(MakeAutoResetEvent()), _running(false)
		{}
		void Start() {
			if (_running) {
				return;
			}
			auto weak = std::weak_ptr<UdpForwarder::Impl>(shared_from_this());
			_running = true;
			_runningThread = std::thread([weak]() {
				auto that = weak.lock();
				if (that) {
					that->Loop();
				}
			});
		}
		void Stop() {
			if (!_running) {
				return;
			}
			_running = false;
			{
				std::lock_guard<std::mutex> lg(_mut);
				_entries.clear();
				SetEvent(_localEvent.get());
			}
			_runningThread.join();
		}
		~Impl() {
			Stop();
		}

		void AddEntry(std::uint16_t localPort, std::uint32_t remotePort, const char* remoteAddress) {
			{
				std::lock_guard<std::mutex> lg(_mut);
				auto found = std::find_if(_entries.begin(), _entries.end(), [localPort](const std::unique_ptr<UdpForwarderEntry>& e) {return e->port == localPort; });
				if (found != _entries.end()) {
					return;
				}
			}
			auto localAddress = ResolveUdp("127.0.0.1", localPort);
			auto entry = std::make_unique<UdpForwarderEntry>();
			entry->port = localPort;
			entry->localSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			entry->remoteAddr = ResolveUdp(remoteAddress, remotePort);
			int yes = 1;
			setsockopt(entry->localSocket.Get(), SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes));
			if (0 != ::bind(entry->localSocket.Get(), localAddress->SockAddr(), localAddress->SockAddrLen())) {
				throw TransportErrorException{ TransportError::BindFailed };
			}
			unsigned long nonBlocking = 1;
			{
				std::lock_guard<std::mutex> lg(_mut);
				WSAEventSelect(entry->localSocket.Get(), _localEvent.get(), FD_READ|FD_WRITE);
				_entries.push_back(std::move(entry));
			}
		}
		void RemoveEntry(std::uint16_t localPort) {
			std::lock_guard<std::mutex> lg(_mut);
			auto found = std::find_if(_entries.begin(), _entries.end(), [localPort](const std::unique_ptr<UdpForwarderEntry>& e) {return e->port == localPort; });
			if (found != _entries.end()) {
				_entries.erase(found);
			}
		}
	};

	UdpForwarder::UdpForwarder() :_impl(make_shared<UdpForwarder::Impl>())
	{
	}
	void UdpForwarder::Start()
	{
		_impl->Start();
	}
	void UdpForwarder::Stop()
	{
		_impl->Stop();
	}
	UdpForwarder::~UdpForwarder()
	{
	}
	void UdpForwarder::AddEntry(std::uint16_t localPort, std::uint32_t remotePort, const char * remoteAddress)
	{
		_impl->AddEntry(localPort, remotePort, remoteAddress);
	}
	void UdpForwarder::RemoveEntry(std::uint16_t localPort)
	{
		_impl->RemoveEntry(localPort);
	}
}