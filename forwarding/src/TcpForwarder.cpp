#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <ostream>
#include <sstream>
#include <string>
#include <client.h>
#include <map>
#include "Forwarders.h"

using namespace forwarding;



namespace forwarding {

	struct ConnectedPair {
		SafeSocket local;
		SafeSocket remote;
		std::vector<char> to_remote;
		std::vector<char> to_local;
		bool closePending = false;
		bool collectPending = false;
		bool connected = false;
		int id;
	};



	struct ForwarderEntry {
		std::uint16_t port;
		SafeSocket listeningSocket;
		std::unique_ptr<ResolvedAddress> remoteAddr;
	};
	struct EventPair {
		SafeAutoResetEvent localEvent;
		SafeAutoResetEvent remoteEvent;
		EventPair() : localEvent(MakeAutoResetEvent()), remoteEvent(MakeAutoResetEvent()) {}
	};
	class TcpDataBridge {
	private:
		const int EventSlotCount = MAXIMUM_WAIT_OBJECTS / 2;
		std::atomic<bool> _running;
		std::thread _runningThread;
		std::vector<EventPair> _events;
		std::mutex _mut;
		std::map<int, std::vector<ConnectedPair>> _entriesSlots;

		int _lastUsedSlot = -1;

		void OnLocalSocketSignaled(int slot) {
			std::lock_guard<std::mutex> lg(_mut);
			auto& entries = _entriesSlots[slot];
			for (auto& pair : entries) {

				WSANETWORKEVENTS events;
				WSAEnumNetworkEvents(pair.local.Get(), nullptr, &events);

				if ((events.lNetworkEvents & FD_READ) == FD_READ) {

					u_long available = 0;
					ioctlsocket(pair.local.Get(), FIONREAD, &available);
					auto oldSize = pair.to_remote.size();
					pair.to_remote.resize(oldSize + available);
					auto actuallyRead = recv(pair.local.Get(), &pair.to_remote[oldSize], available, 0);
					pair.to_remote.resize(oldSize + actuallyRead);

					// write what we can to remote
					if (pair.to_remote.size() > 0) {
						auto written = send(pair.remote.Get(), &pair.to_remote[0], static_cast<int>(pair.to_remote.size()), 0);
						if (written > 0)
							pair.to_remote.erase(pair.to_remote.begin(), pair.to_remote.begin() + written);
					}

					long localEvents = FD_CLOSE;
					if (pair.to_remote.size() < 8192) {
						localEvents |= FD_READ;
					}
					if (pair.to_local.size() > 0) {
						localEvents |= FD_WRITE;
					}
					long remoteEvents = FD_CLOSE;
					if (pair.to_local.size() < 8192) {
						remoteEvents |= FD_READ;
					}
					if (pair.to_remote.size() > 0) {
						remoteEvents |= FD_WRITE;
					}
					WSAEventSelect(pair.local.Get(), _events[slot].localEvent.get(), localEvents);
					WSAEventSelect(pair.remote.Get(), _events[slot].remoteEvent.get(), remoteEvents);

				}
				if ((events.lNetworkEvents & FD_WRITE) == FD_WRITE) {

					if (pair.to_local.size() > 0) {
						auto written = send(pair.local.Get(), &pair.to_local[0], static_cast<int>(pair.to_local.size()), 0);
						if (written > 0)
							pair.to_local.erase(pair.to_local.begin(), pair.to_local.begin() + written);
					}

					long localEvents = FD_CLOSE;
					if (pair.to_remote.size() < 8192) {
						localEvents |= FD_READ;
					}
					if (pair.to_local.size() > 0) {
						localEvents |= FD_WRITE;
					}
					long remoteEvents = FD_CLOSE;
					if (pair.to_local.size() < 8192) {
						remoteEvents |= FD_READ;
					}
					if (pair.to_remote.size() > 0) {
						remoteEvents |= FD_WRITE;
					}

					WSAEventSelect(pair.local.Get(), _events[slot].localEvent.get(), localEvents);
					WSAEventSelect(pair.remote.Get(), _events[slot].remoteEvent.get(), remoteEvents);
					if (pair.to_local.size() == 0 && pair.to_remote.size() == 0 && pair.closePending) {
						pair.collectPending = true;
					}

				}
				if ((events.lNetworkEvents & FD_CLOSE) == FD_CLOSE) {

					if (pair.to_local.size() == 0 && pair.to_remote.size() == 0) {
						pair.collectPending = true;
					}
					else {
						pair.closePending = true;
					}

				}
			}
			entries.erase(std::remove_if(entries.begin(), entries.end(), [](const ConnectedPair& p) {return p.collectPending; }), entries.end());
		}

		void OnRemoteSocketSignaled(int slot) {
			std::lock_guard<std::mutex> lg(_mut);
			auto& entries = _entriesSlots[slot];
			if (entries.size() == 0) {
				return;
			}
			for (auto& pair : entries) {
				WSANETWORKEVENTS events;
				WSAEnumNetworkEvents(pair.remote.Get(), nullptr, &events);

				if ((events.lNetworkEvents & FD_READ) == FD_READ) {


					u_long available = 0;
					ioctlsocket(pair.remote.Get(), FIONREAD, &available);
					auto oldSize = pair.to_local.size();
					pair.to_local.resize(oldSize + available);
					auto actuallyRead = recv(pair.remote.Get(), &pair.to_local[oldSize], available, 0);
					pair.to_local.resize(oldSize + actuallyRead);

					// write what we can to local
					if (pair.to_local.size() > 0) {
						auto written = send(pair.local.Get(), &pair.to_local[0], static_cast<int>(pair.to_local.size()), 0);
						if (written > 0)
							pair.to_local.erase(pair.to_local.begin(), pair.to_local.begin() + written);
					}

					long localEvents = FD_CLOSE;
					if (pair.to_remote.size() < 8192) {
						localEvents |= FD_READ;
					}
					if (pair.to_local.size() > 0) {
						localEvents |= FD_WRITE;
					}
					long remoteEvents = FD_CLOSE;
					if (pair.to_local.size() < 8192) {
						remoteEvents |= FD_READ;
					}
					if (pair.to_remote.size() > 0) {
						remoteEvents |= FD_WRITE;
					}
					WSAEventSelect(pair.local.Get(), _events[slot].localEvent.get(), localEvents);
					WSAEventSelect(pair.remote.Get(), _events[slot].remoteEvent.get(), remoteEvents);


				}
				if ((events.lNetworkEvents & FD_WRITE) == FD_WRITE) {

					if (!pair.connected) {
						pair.connected = true;
					}
					if (pair.to_remote.size() > 0) {
						auto written = send(pair.remote.Get(), &pair.to_remote[0], static_cast<int>(pair.to_remote.size()), 0);
						if (written > 0)
							pair.to_remote.erase(pair.to_remote.begin(), pair.to_remote.begin() + written);
					}
					long localEvents = FD_CLOSE;
					if (pair.to_remote.size() < 8192) {
						localEvents |= FD_READ;
					}
					if (pair.to_local.size() > 0) {
						localEvents |= FD_WRITE;
					}
					long remoteEvents = FD_CLOSE;
					if (pair.to_local.size() < 8192) {
						remoteEvents |= FD_READ;
					}
					if (pair.to_remote.size() > 0) {
						remoteEvents |= FD_WRITE;
					}

					WSAEventSelect(pair.local.Get(), _events[slot].localEvent.get(), localEvents);
					WSAEventSelect(pair.remote.Get(), _events[slot].remoteEvent.get(), remoteEvents);
					if (pair.to_local.size() == 0 && pair.to_remote.size() == 0 && pair.closePending) {
						pair.collectPending = true;
					}

				}
				if ((events.lNetworkEvents & FD_CLOSE) == FD_CLOSE) {

					if (pair.to_local.size() == 0 && pair.to_remote.size() == 0) {
						pair.collectPending = true;
					}
					else {
						pair.closePending = true;
					}

				}
			}

			entries.erase(std::remove_if(entries.begin(), entries.end(), [](const ConnectedPair& p) {return p.collectPending; }), entries.end());

		}
	public:
		TcpDataBridge() : _running(false)
		{
			_events.resize(EventSlotCount);
		}
		void Loop() {
			std::vector<HANDLE> events;
			for (auto& p : _events) {
				events.push_back(p.localEvent.get());
				events.push_back(p.remoteEvent.get());
			}
			while (_running) {
				auto waitResult = WaitForMultipleObjects(static_cast<DWORD>(events.size()), &events[0], FALSE, INFINITE);
				if (!_running) {
					return;
				}
				if (waitResult >=WAIT_ABANDONED_0 || waitResult == WAIT_IO_COMPLETION || waitResult == WAIT_TIMEOUT || waitResult == WAIT_FAILED) {
					continue;
				}
				else{
					auto evIndex = waitResult - WAIT_OBJECT_0;
					auto slotIndex = evIndex / 2;
					bool isRemote = (evIndex % 2) == 1;
					if (isRemote) {
						OnRemoteSocketSignaled(slotIndex);
					}
					else {
						OnLocalSocketSignaled(slotIndex);
					}
				}
			}
		}
		void Start() {
			if (_running) {
				return;
			}
			_running = true;
			_runningThread = std::thread([this]() {
				this->Loop();
			});
		}
		void Stop() {
			if (!_running) {
				return;
			}
			_running = false;
			{
				std::lock_guard<std::mutex> lg(_mut);
				_entriesSlots.clear();
				SetEvent(_events[0].localEvent.get());
			}
			_runningThread.join();
		}
		~TcpDataBridge() {
			Stop();
		}
		void AddConnectedPair(ConnectedPair&& pair) {

			std::lock_guard<std::mutex> lg(_mut);
			auto slot = (++_lastUsedSlot) % EventSlotCount;
			auto& evs = _events[slot];
			_entriesSlots[slot].push_back(std::move(pair));
			if ((_entriesSlots[slot].end() - 1)->connected) {
				WSAEventSelect((_entriesSlots[slot].end() - 1)->local.Get(), evs.localEvent.get(), FD_READ | FD_CLOSE);
				WSAEventSelect((_entriesSlots[slot].end() - 1)->remote.Get(), evs.remoteEvent.get(), FD_READ | FD_CLOSE);
			}
			else {
				WSAEventSelect((_entriesSlots[slot].end() - 1)->local.Get(), evs.localEvent.get(), FD_READ | FD_CLOSE);
				WSAEventSelect((_entriesSlots[slot].end() - 1)->remote.Get(), evs.remoteEvent.get(), FD_READ | FD_WRITE | FD_CLOSE);
			}
		}
	};

	class TcpForwarder::Impl : public std::enable_shared_from_this<TcpForwarder::Impl> {
	private:

		SafeAutoResetEvent _acceptEvent;

		std::mutex _entriesMut;
		std::vector<std::unique_ptr<ForwarderEntry>> _entries;
		std::atomic<bool> _running;
		int _bridgeSlot = 0;
		std::unique_ptr<TcpDataBridge> _bridges[4];

		std::thread _runningThread;

		void OnEntryAcceptedOrClosed() {
			{
				std::lock_guard<std::mutex> lg(_entriesMut);
				for (auto it = _entries.begin(); it != _entries.end(); ++it) {
					WSANETWORKEVENTS events;
					WSAEnumNetworkEvents(it->get()->listeningSocket.Get(), nullptr, &events);
					if ((events.lNetworkEvents & FD_ACCEPT) == FD_ACCEPT) {
						auto rawSock = WSAAccept(it->get()->listeningSocket.Get(), nullptr, nullptr, nullptr, NULL);
						if (INVALID_SOCKET == rawSock) {
							continue;
						}
						{
							ConnectedPair pair;
							pair.local = rawSock;
							pair.remote = socket(AF_INET, SOCK_STREAM, 0);
							unsigned long nonBlocking = 1;
							ioctlsocket(pair.remote.Get(), FIONBIO, &nonBlocking);
							auto connectResult = connect(pair.remote.Get(), it->get()->remoteAddr->SockAddr(), it->get()->remoteAddr->SockAddrLen());
							if (connectResult == 0) {
								pair.connected = true;
								pair.id = _bridgeSlot;
								_bridges[(_bridgeSlot++) % 4]->AddConnectedPair(std::move(pair));
							}
							else if (WSAGetLastError() == WSAEWOULDBLOCK) {
								pair.id = _bridgeSlot;
								_bridges[(_bridgeSlot++) % 4]->AddConnectedPair(std::move(pair));
							}

							// if connection error, don't do anything
						}
					}
				}
			}
		}

	public:
		void Loop() {
			while (_running) {
				HANDLE events[] = { _acceptEvent.get() };
				auto waitResult = WaitForMultipleObjects(1, events, FALSE, INFINITE);
				if (!_running) {
					return;
				}
				if (waitResult == WAIT_OBJECT_0) {
					OnEntryAcceptedOrClosed();
				}
			}
		}
		void Start() {
			if (_running) {
				return;
			}
			auto weak = std::weak_ptr<TcpForwarder::Impl>(shared_from_this());
			_running = true;
			_runningThread = std::thread([weak]() {
				auto that = weak.lock();
				if (that) {
					that->Loop();
				}
			});
			for (int i = 0; i < 4; ++i) {
				_bridges[i]->Start();
			}
		}
		void Stop() {
			if (!_running) {
				return;
			}
			_running = false;
			{
				std::lock_guard<std::mutex> lg(_entriesMut);
				_entries.clear();
				SetEvent(_acceptEvent.get());
			}
			_runningThread.join();

			for (int i = 0; i < 4; ++i) {
				_bridges[i]->Stop();
			}
		}

		void AddEntry(std::uint16_t localPort, std::uint32_t remotePort, const char* remoteAddress) {
			{
				std::lock_guard<std::mutex> lg(_entriesMut);
				auto found = std::find_if(_entries.begin(), _entries.end(), [localPort](const std::unique_ptr<ForwarderEntry>& e) {return e->port == localPort; });
				if (found != _entries.end()) {
					return;
				}
			}
			auto localAddress = Resolve("127.0.0.1", localPort);
			auto entry = std::make_unique<ForwarderEntry>();
			entry->port = localPort;
			entry->listeningSocket = socket(AF_INET, SOCK_STREAM, 0);
			entry->remoteAddr = Resolve(remoteAddress, remotePort);
			int yes = 1;
			setsockopt(entry->listeningSocket.Get(), SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes));
			if (0 != ::bind(entry->listeningSocket.Get(), localAddress->SockAddr(), localAddress->SockAddrLen())) {
				throw TransportErrorException{ TransportError::BindFailed };
			}
			if (0 != listen(entry->listeningSocket.Get(), SOMAXCONN)) {
				throw TransportErrorException{ TransportError::ListenFailed };
			}
			{
				std::lock_guard<std::mutex> lg(_entriesMut);
				WSAEventSelect(entry->listeningSocket.Get(), _acceptEvent.get(), FD_ACCEPT);
				_entries.push_back(std::move(entry));
			}
		}
		void RemoveEntry(std::uint16_t localPort) {
			std::lock_guard<std::mutex> lg(_entriesMut);
			auto found = std::find_if(_entries.begin(), _entries.end(), [localPort](const std::unique_ptr<ForwarderEntry>& e) {return e->port == localPort; });
			if (found != _entries.end()) {
				_entries.erase(found);
			}
		}

		Impl() : _acceptEvent(MakeAutoResetEvent()), _running(false), _bridges{ std::make_unique<TcpDataBridge>(),std::make_unique<TcpDataBridge>(), std::make_unique<TcpDataBridge>(), std::make_unique<TcpDataBridge>() } {}
		~Impl() {
			Stop();
		}
	};
	TcpForwarder::TcpForwarder() : _impl(std::make_shared<Impl>())
	{
	}
	void TcpForwarder::Start()
	{
		_impl->Start();
	}
	void TcpForwarder::Stop()
	{
		_impl->Stop();
	}
	TcpForwarder::~TcpForwarder()
	{
	}
	void TcpForwarder::AddEntry(std::uint16_t localPort, std::uint32_t remotePort, const char* remoteAddress)
	{
		_impl->AddEntry(localPort, remotePort, remoteAddress);
	}
	void TcpForwarder::RemoveEntry(std::uint16_t localPort)
	{
		_impl->RemoveEntry(localPort);
	}
}
