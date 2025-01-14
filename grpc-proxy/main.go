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

	corsMux := http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Access-Control-Allow-Origin", "*")
		w.Header().Set("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS")
		if requestHeaders := r.Header.Get("Access-Control-Request-Headers"); requestHeaders != "" {
			w.Header().Set("Access-Control-Allow-Headers", requestHeaders)
		}
		if r.Method == http.MethodOptions {
			w.WriteHeader(http.StatusOK)
			return
		}
		mux.ServeHTTP(w, r)
	})

	err := api.RegisterConfigServiceHandlerFromEndpoint(ctx, mux, "localhost:50051", opts)
	if err != nil {
		slog.Error("failed to register ConfigService handler")
		return
	}

	slog.Info("http sever runs on port 8081")
	// Start HTTP server (and proxy calls to gRPC server endpoint)
	if err := http.ListenAndServe(":8081", corsMux); err != nil {
		slog.Error("failed to serve on 8081")
		return
	}
}
