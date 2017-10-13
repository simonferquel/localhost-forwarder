package main

//sys forwarding_udp_new() (ptr uintptr) = forwarding.forwarding_udp_new
//sys forwarding_udp_delete(ptr uintptr) = forwarding.forwarding_udp_delete
//sys forwarding_udp_start(ptr uintptr) = forwarding.forwarding_udp_start
//sys forwarding_udp_stop(ptr uintptr) = forwarding.forwarding_udp_stop
//sys forwarding_udp_addEntry(ptr uintptr, localport uint16, remotePort uint32, remoteAddress string) (err error)[failretval!=0] = forwarding.forwarding_udp_addEntry
//sys forwarding_udp_removeEntry(ptr uintptr, localport uint16) = forwarding.forwarding_udp_removeEntry

//sys forwarding_tcp_new() (ptr uintptr) = forwarding.forwarding_tcp_new
//sys forwarding_tcp_delete(ptr uintptr) = forwarding.forwarding_tcp_delete
//sys forwarding_tcp_start(ptr uintptr) = forwarding.forwarding_tcp_start
//sys forwarding_tcp_stop(ptr uintptr) = forwarding.forwarding_tcp_stop
//sys forwarding_tcp_addEntry(ptr uintptr, localport uint16, remotePort uint32, remoteAddress string) (err error)[failretval!=0] = forwarding.forwarding_tcp_addEntry
//sys forwarding_tcp_removeEntry(ptr uintptr, localport uint16) = forwarding.forwarding_tcp_removeEntry
