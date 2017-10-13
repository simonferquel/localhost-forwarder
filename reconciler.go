package main

import (
	"context"

	"github.com/docker/docker/api/types"
	"github.com/docker/docker/client"
)

type reconciler struct {
	docker client.APIClient
	f      *forwarder
}

func newReconciler(f *forwarder) (*reconciler, error) {
	c, err := client.NewEnvClient()
	if err != nil {
		return nil, err
	}
	_, err = c.Ping(context.Background())
	if err != nil {
		return nil, err
	}
	return &reconciler{docker: c, f: f}, nil
}

func (r *reconciler) reconcile() error {
	tcpEntries := make(map[forwardEntry]struct{})
	udpEntries := make(map[forwardEntry]struct{})
	containers, err := r.docker.ContainerList(context.Background(), types.ContainerListOptions{})
	if err != nil {
		return err
	}
	for _, c := range containers {
		if len(c.Ports) == 0 {
			continue
		}
		details, err := r.docker.ContainerInspect(context.Background(), c.ID)
		if err != nil {
			return err
		}
		if details.NetworkSettings == nil {
			continue
		}
		ip := details.NetworkSettings.IPAddress
		if ip == "" {
			for _, n := range details.NetworkSettings.Networks {
				ip = n.IPAddress
				if ip != "" {
					break
				}
			}
		}
		if ip == "" {
			continue
		}
		for _, p := range c.Ports {
			if p.PublicPort == 0 {
				continue
			}
			if p.Type == "tcp" {
				tcpEntries[forwardEntry{localPort: uint16(p.PublicPort), remotePort: uint32(p.PrivatePort), remoteAddress: ip}] = struct{}{}
			} else if p.Type == "udp" {
				udpEntries[forwardEntry{localPort: uint16(p.PublicPort), remotePort: uint32(p.PrivatePort), remoteAddress: ip}] = struct{}{}
			}

		}
	}
	return r.f.apply(tcpEntries, udpEntries)
}
