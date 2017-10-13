#pragma once
#include <stdint.h>
#if defined(FORWARDING_EXPORTS)
#define  FORWARDING_DLL __declspec(dllexport)
#else
#define  FORWARDING_DLL __declspec(dllimport)
#endif /* MyLibrary_EXPORTS */

#ifdef __cplusplus
extern "C" {
#endif

enum forwarding_error{
    FORWARDING_OK = 0,
	FORWARDING_UNKNOWN_ERROR = 1,
    FORWARDING_NAME_RESOLUTION_FAILED = 2,
    FORWARDING_BIND_FAILED = 3,
};

typedef void* forwarding_udp;
typedef void* forwarding_tcp;

FORWARDING_DLL forwarding_udp forwarding_udp_new();
FORWARDING_DLL void forwarding_udp_delete(forwarding_udp);
FORWARDING_DLL void forwarding_udp_start(forwarding_udp);
FORWARDING_DLL void forwarding_udp_stop(forwarding_udp);
FORWARDING_DLL forwarding_error forwarding_udp_addEntry(forwarding_udp, uint16_t localPort, uint32_t remotePort, char* remoteAddress);
FORWARDING_DLL void forwarding_udp_removeEntry(forwarding_udp, uint16_t localPort);

FORWARDING_DLL forwarding_tcp forwarding_tcp_new();
FORWARDING_DLL void forwarding_tcp_delete(forwarding_tcp);
FORWARDING_DLL void forwarding_tcp_start(forwarding_tcp);
FORWARDING_DLL void forwarding_tcp_stop(forwarding_tcp);
FORWARDING_DLL forwarding_error forwarding_tcp_addEntry(forwarding_tcp, uint16_t localPort, uint32_t remotePort, char* remoteAddress);
FORWARDING_DLL void forwarding_tcp_removeEntry(forwarding_tcp, uint16_t localPort);

#ifdef __cplusplus
}
#endif