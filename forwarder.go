package main

import (
	"errors"
	"fmt"
	"os"
)

type forwardEntry struct {
	localPort     uint16
	remotePort    uint32
	remoteAddress string
}
type forwarder struct {
	nativeUDP  uintptr
	nativeTCP  uintptr
	tcpEntries map[forwardEntry]struct{}
	udpEntries map[forwardEntry]struct{}
	closed     bool
}

func newForwarder() *forwarder {
	res := &forwarder{
		nativeUDP:  forwarding_udp_new(),
		nativeTCP:  forwarding_tcp_new(),
		tcpEntries: make(map[forwardEntry]struct{}),
		udpEntries: make(map[forwardEntry]struct{}),
	}
	forwarding_tcp_start(res.nativeTCP)
	forwarding_udp_start(res.nativeUDP)
	return res
}
func (f *forwarder) Close() {
	if f.closed {
		return
	}
	forwarding_tcp_stop(f.nativeTCP)
	forwarding_udp_stop(f.nativeUDP)
	forwarding_tcp_delete(f.nativeTCP)
	forwarding_udp_delete(f.nativeUDP)
	f.closed = true
}

func (f *forwarder) addTCP(entry forwardEntry) error {
	if f.closed {
		return errors.New("forwarder is closed")
	}
	if _, ok := f.tcpEntries[entry]; ok {
		return nil
	}
	err := forwarding_tcp_addEntry(f.nativeTCP, entry.localPort, entry.remotePort, entry.remoteAddress)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Failed to forward tcp port %v to %s:%v\n", entry.localPort, entry.remoteAddress, entry.remotePort)
		return err
	}
	fmt.Printf("Forwarding tcp port %v to %s:%v\n", entry.localPort, entry.remoteAddress, entry.remotePort)
	f.tcpEntries[entry] = struct{}{}
	return nil
}

func (f *forwarder) removeTCP(entry forwardEntry) error {
	if f.closed {
		return errors.New("forwarder is closed")
	}
	if _, ok := f.tcpEntries[entry]; !ok {
		return nil
	}
	forwarding_tcp_removeEntry(f.nativeTCP, entry.localPort)
	fmt.Printf("Stopped forwarding tcp port %v to %s:%v\n", entry.localPort, entry.remoteAddress, entry.remotePort)
	delete(f.tcpEntries, entry)
	return nil
}

func (f *forwarder) addUDP(entry forwardEntry) error {
	if f.closed {
		return errors.New("forwarder is closed")
	}
	if _, ok := f.udpEntries[entry]; ok {
		return nil
	}
	err := forwarding_udp_addEntry(f.nativeUDP, entry.localPort, entry.remotePort, entry.remoteAddress)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Failed to forward udp port %v to %s:%v\n", entry.localPort, entry.remoteAddress, entry.remotePort)
		return err
	}
	fmt.Printf("Forwarding udp port %v to %s:%v\n", entry.localPort, entry.remoteAddress, entry.remotePort)
	f.udpEntries[entry] = struct{}{}
	return nil
}

func (f *forwarder) removeUDP(entry forwardEntry) error {
	if f.closed {
		return errors.New("forwarder is closed")
	}
	if _, ok := f.udpEntries[entry]; !ok {
		return nil
	}
	forwarding_udp_removeEntry(f.nativeUDP, entry.localPort)
	fmt.Printf("Stopped forwarding udp port %v to %s:%v\n", entry.localPort, entry.remoteAddress, entry.remotePort)
	delete(f.udpEntries, entry)
	return nil
}

func entriesDiff(current, toApply map[forwardEntry]struct{}) (toAdd, toRemove []forwardEntry) {
	for c := range current {
		if _, ok := toApply[c]; !ok {
			toRemove = append(toRemove, c)
		}
	}
	for c := range toApply {
		if _, ok := current[c]; !ok {
			toAdd = append(toAdd, c)
		}
	}
	return
}

func (f *forwarder) apply(tcp, udp map[forwardEntry]struct{}) error {
	if f.closed {
		return errors.New("forwarder is closed")
	}

	tcpAdd, tcpRemove := entriesDiff(f.tcpEntries, tcp)
	udpAdd, udpRemove := entriesDiff(f.udpEntries, udp)

	for _, e := range tcpRemove {
		err := f.removeTCP(e)
		if err != nil {
			return err
		}
	}
	for _, e := range udpRemove {
		err := f.removeUDP(e)
		if err != nil {
			return err
		}
	}
	for _, e := range tcpAdd {
		err := f.addTCP(e)
		if err != nil {
			return err
		}
	}
	for _, e := range udpAdd {
		err := f.addUDP(e)
		if err != nil {
			return err
		}
	}
	return nil
}
