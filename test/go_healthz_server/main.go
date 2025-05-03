package main

import (
	"flag"
	"fmt"
	"net/http"
)

func healthzHandler(w http.ResponseWriter, r *http.Request) {
	fmt.Println("new connection")
	w.WriteHeader(http.StatusOK)
}

func main() {
	port := flag.String("port", "9001", "Port to run the server on")
	flag.Parse()

	http.HandleFunc("/healthz", healthzHandler)

	address := fmt.Sprintf(":%s", *port)
	fmt.Printf("Starting server on %s\n", address)
	if err := http.ListenAndServe(address, nil); err != nil {
		panic(err)
	}
}