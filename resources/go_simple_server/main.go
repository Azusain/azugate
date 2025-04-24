package main

import (
	"log"
	"net/http"
)

// build cmd:
// CGO_ENABLED=0 GOOS=linux GOARCH=amd64 go build -ldflags="-s -w" -o go_simple_server main.go
func main() {
	staticFolder := "../"

	fs := http.FileServer(http.Dir(staticFolder))
	http.Handle("/", fs)
	port := ":8081"
	log.Printf("Serving %s on HTTP port %s\n", staticFolder, port)
	if err := http.ListenAndServe(port, nil); err != nil {
		log.Fatal(err)
	}
}
