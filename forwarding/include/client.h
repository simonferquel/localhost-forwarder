#pragma once
#include "common.h"
namespace forwarding {

	class TcpForwarder  {
	private:
		class Impl;
		std::shared_ptr<Impl> _impl;
	public:
		TcpForwarder();
		void Start();
		void Stop();
		~TcpForwarder();

		void AddEntry(std::uint16_t localPort, std::uint32_t remotePort, const char* remoteAddress);
		void RemoveEntry(std::uint16_t localPort);
	};

	class UdpForwarder {
	private:
		class Impl;
		std::shared_ptr<Impl> _impl;
	public:
		UdpForwarder();
		void Start();
		void Stop();
		~UdpForwarder();

		void AddEntry(std::uint16_t localPort, std::uint32_t remotePort, const char* remoteAddress);
		void RemoveEntry(std::uint16_t localPort);
	};
}