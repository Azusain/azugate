package main

import (
	"context"
	"log/slog"
	"net/http"
	"proxy/api"

	"github.com/grpc-ecosystem/grpc-gateway/v2/runtime"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
)

func main() {
	ctx := context.Background()
	ctx, cancel := context.WithCancel(ctx)
	defer cancel()

	mux := runtime.NewServeMux()
	opts := []grpc.DialOption{grpc.WithTransportCredentials(insecure.NewCredentials())}
	err := api.RegisterConfigServiceHandlerFromEndpoint(ctx, mux, "localhost:50051", opts)
	if err != nil {
		slog.Error("wrong")
		return
	}

	slog.Info("http sever runs on port 8081")
	// Start HTTP server (and proxy calls to gRPC server endpoint)
	if err := http.ListenAndServe(":8081", mux); err != nil {
		slog.Error("error")
		return
	}
}
