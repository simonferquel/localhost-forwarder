package main

import (
	"context"
	"fmt"
	"os"
	"time"

	"github.com/docker/docker/api/types"
)

func main() {
	f := newForwarder()
	r, err := newReconciler(f)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Can't create reconciler: %s\n", err.Error())
		return
	}

	ch := make(chan struct{})
	go func() {
		for {
			ch <- struct{}{}
			time.Sleep(20 * time.Second)
		}
	}()
	go func() {
		for {
			evs, errs := r.docker.Events(context.Background(), types.EventsOptions{})
			for {
				select {
				case <-errs:
					break // re-launch event loop
				case msg := <-evs:
					if msg.Type == "container" &&
						(msg.Action == "kill" || msg.Action == "pause" || msg.Action == "restart" || msg.Action == "start" || msg.Action == "stop" || msg.Action == "unpause" || msg.Action == "update") {
						ch <- struct{}{}
					}
				}
			}
		}
	}()
	for {
		<-ch
		err = r.reconcile()
		if err != nil {
			fmt.Fprintf(os.Stderr, "Reconciliation failed: %s\n", err.Error())
		} else {
			fmt.Printf("Reconciliation succeeded\n")
		}
	}
}
