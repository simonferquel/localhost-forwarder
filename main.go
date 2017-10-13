package main

import (
	"fmt"
	"os"
	"time"
)

func main() {
	f := newForwarder()
	r, err := newReconciler(f)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Can't create reconciler: %s\n", err.Error())
		return
	}
	for {
		err = r.reconcile()
		if err != nil {
			fmt.Fprintf(os.Stderr, "Reconciliation failed: %s\n", err.Error())
		} else {
			fmt.Printf("Reconciliation succeeded\n")
		}

		time.Sleep(5 * time.Second)
	}
}
