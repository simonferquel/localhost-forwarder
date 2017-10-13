#include <client.h>
#include <client_c.h>

forwarding_udp forwarding_udp_new() {
	return reinterpret_cast<forwarding_udp>(new forwarding::UdpForwarder());
}

void forwarding_udp_delete(forwarding_udp udp) {
	delete reinterpret_cast<forwarding::UdpForwarder*>(udp);
}
void forwarding_udp_start(forwarding_udp udp) {
	reinterpret_cast<forwarding::UdpForwarder*>(udp)->Start();
}
void forwarding_udp_stop(forwarding_udp udp) {
	reinterpret_cast<forwarding::UdpForwarder*>(udp)->Stop();
}
forwarding_error forwarding_udp_addEntry(forwarding_udp udp, uint16_t localPort, uint32_t remotePort, char* remoteAddress) {
	try {
		reinterpret_cast<forwarding::UdpForwarder*>(udp)->AddEntry(localPort, remotePort, remoteAddress);
		return FORWARDING_OK;
	}
	catch (forwarding::TransportErrorException& ex) {
		switch (ex.Error)
		{
		case forwarding::TransportError::NameResolutionFailed:
			return FORWARDING_NAME_RESOLUTION_FAILED;
		case forwarding::TransportError::BindFailed:
			return FORWARDING_BIND_FAILED;
		default:
			return FORWARDING_UNKNOWN_ERROR;
		}
	}
	catch(...){
		return FORWARDING_UNKNOWN_ERROR;
	}
}
void forwarding_udp_removeEntry(forwarding_udp udp, uint16_t localPort) {
	reinterpret_cast<forwarding::UdpForwarder*>(udp)->RemoveEntry(localPort);
}

forwarding_tcp forwarding_tcp_new() {
	return reinterpret_cast<forwarding_tcp>(new forwarding::TcpForwarder());
}
void forwarding_tcp_delete(forwarding_tcp tcp) {
	delete reinterpret_cast<forwarding::TcpForwarder*>(tcp);
}
void forwarding_tcp_start(forwarding_tcp tcp) {
	reinterpret_cast<forwarding::TcpForwarder*>(tcp)->Start();
}
void forwarding_tcp_stop(forwarding_tcp tcp) {
	reinterpret_cast<forwarding::TcpForwarder*>(tcp)->Stop();
}
forwarding_error forwarding_tcp_addEntry(forwarding_tcp tcp, uint16_t localPort, uint32_t remotePort, char* remoteAddress) {
	try {
		reinterpret_cast<forwarding::TcpForwarder*>(tcp)->AddEntry(localPort, remotePort, remoteAddress);
		return FORWARDING_OK;
	}
	catch (forwarding::TransportErrorException& ex) {
		switch (ex.Error)
		{
		case forwarding::TransportError::NameResolutionFailed:
			return FORWARDING_NAME_RESOLUTION_FAILED;
		case forwarding::TransportError::BindFailed:
			return FORWARDING_BIND_FAILED;
		default:
			return FORWARDING_UNKNOWN_ERROR;
		}
	}
	catch (...) {
		return FORWARDING_UNKNOWN_ERROR;
	}
}
void forwarding_tcp_removeEntry(forwarding_tcp tcp, uint16_t localPort) {
	reinterpret_cast<forwarding::TcpForwarder*>(tcp)->RemoveEntry(localPort);
}