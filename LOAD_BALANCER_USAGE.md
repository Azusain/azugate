# Load Balancer and Health Check Features

## Overview

The azugate proxy now includes comprehensive load balancing and health checking capabilities that make it production-ready for high-availability deployments. These features provide:

- **Multiple Load Balancing Strategies**: Round-robin, least connections, weighted round-robin, random, and IP hash
- **Active Health Checking**: Configurable HTTP health checks with failure detection
- **Real-time Metrics**: Connection tracking and response time monitoring
- **High Availability**: Automatic failover when upstream servers become unhealthy

## Load Balancing Strategies

### 1. Round Robin (Default)
Distributes requests evenly across all healthy servers in rotation.

### 2. Least Connections
Routes requests to the server with the fewest active connections.

### 3. Weighted Round Robin
Allows assigning different weights to servers based on their capacity.

### 4. Random
Randomly selects a healthy server for each request.

### 5. IP Hash
Routes requests from the same client IP to the same server (session affinity).

## Configuration Example

Here's how to set up load balancing in your application:

```cpp
#include "load_balancer.hpp"

// Create load balancer with desired strategy
auto load_balancer = std::make_shared<azugate::LoadBalancer>(
    io_context, 
    azugate::LoadBalancingStrategy::RoundRobin
);

// Add upstream servers
load_balancer->add_server("192.168.1.10", 8080, 1);  // weight = 1
load_balancer->add_server("192.168.1.11", 8080, 2);  // weight = 2 (higher capacity)
load_balancer->add_server("192.168.1.12", 8080, 1);  // weight = 1

// Configure health checks
azugate::HealthCheckConfig health_config;
health_config.path = "/health";                    // Health check endpoint
health_config.interval = std::chrono::milliseconds(5000);  // Check every 5s
health_config.timeout = std::chrono::milliseconds(2000);   // 2s timeout
health_config.healthy_threshold = 2;               // 2 successes = healthy
health_config.unhealthy_threshold = 3;             // 3 failures = unhealthy
health_config.expected_status = 200;               // Expected HTTP status

load_balancer->set_health_check_config(health_config);
load_balancer->enable_health_checks(true);

// Register load balancer for a specific route
azugate::register_load_balancer("/api/v1/", load_balancer);
```

## Integration with HTTP Proxy Handler

The load balancer integrates seamlessly with the existing HTTP proxy handling:

```cpp
// Modified handleHttpRequest to use load balancer
void handleHttpRequestWithLoadBalancer() {
    // Get load balancer for this route
    auto lb = azugate::get_load_balancer_for_route(source_connection_info_);
    if (!lb) {
        // Fallback to original single-server handling
        return handleHttpRequest(target_address, target_port);
    }
    
    // Get next server from load balancer
    auto server = lb->get_server(client_ip);
    if (!server) {
        SPDLOG_ERROR("No healthy servers available");
        sendServiceUnavailableResponse();
        return;
    }
    
    // Track request start
    auto start_time = std::chrono::steady_clock::now();
    lb->on_request_start(server);
    
    // Make request to selected server
    bool success = makeHttpRequest(server->address(), server->port());
    
    // Track request completion
    auto end_time = std::chrono::steady_clock::now();
    auto response_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    lb->on_request_complete(server, response_time, success);
}
```

## Health Check Endpoint Requirements

Your upstream services should implement a health check endpoint that returns:
- **HTTP 200 OK** when the service is healthy
- **HTTP 503 Service Unavailable** when the service is unhealthy
- Optional: specific response body for validation

Example health check endpoint:
```json
GET /health
HTTP/1.1 200 OK
Content-Type: application/json

{
    "status": "healthy",
    "timestamp": "2024-01-15T10:30:00Z",
    "version": "1.2.3"
}
```

## Command Line Usage

You can now configure load balancing via command line options:

```bash
# Enable load balancing with health checks
./azugate \
  --config config.yaml \
  --enable-load-balancing \
  --lb-strategy round-robin \
  --health-check-interval 5000 \
  --health-check-path /health
```

## Monitoring and Observability

The load balancer provides real-time statistics:

```cpp
// Get load balancer statistics
auto servers = load_balancer->get_all_servers();
for (const auto& server : servers) {
    SPDLOG_INFO("Server {}:{} - Status: {}, Connections: {}, Avg Response: {}ms",
                server->address(), 
                server->port(),
                static_cast<int>(server->health_status()),
                server->active_connections(),
                server->response_time_ms());
}

SPDLOG_INFO("Total servers: {}, Healthy: {}", 
            load_balancer->total_servers(),
            load_balancer->healthy_servers());
```

## YAML Configuration

Example YAML configuration for load balancing:

```yaml
# config.yaml
port: 8080
admin_port: 9090

load_balancers:
  - name: "api_backend"
    strategy: "weighted_round_robin"
    health_check:
      path: "/health"
      interval: 5000
      timeout: 2000
      healthy_threshold: 2
      unhealthy_threshold: 3
      expected_status: 200
    servers:
      - address: "api1.internal.com"
        port: 8080
        weight: 2
      - address: "api2.internal.com"
        port: 8080
        weight: 1
      - address: "api3.internal.com"
        port: 8080
        weight: 1

routing:
  - source:
      type: "http"
      http_url: "/api/"
    target:
      load_balancer: "api_backend"
```

## Best Practices

1. **Health Check Frequency**: Balance between responsiveness and overhead (5-30 seconds)
2. **Thresholds**: Use conservative thresholds to avoid flapping (2-3 consecutive checks)
3. **Weights**: Assign weights based on actual server capacity
4. **Monitoring**: Implement comprehensive monitoring of server health and performance
5. **Graceful Degradation**: Always have fallback mechanisms when all servers are unhealthy

## Performance Impact

The load balancer is designed for high performance:
- **Minimal Overhead**: O(1) server selection for most strategies
- **Thread-Safe**: All operations are protected with fine-grained locking
- **Async Health Checks**: Non-blocking health check operations
- **Connection Pooling**: Reuses connections where possible

## Troubleshooting

Common issues and solutions:

1. **No Healthy Servers**: Check health check endpoint and network connectivity
2. **Uneven Distribution**: Verify server weights and health status
3. **Slow Health Checks**: Adjust timeout values and check server response times
4. **Memory Usage**: Monitor for connection leaks and adjust connection limits

This load balancing system makes azugate enterprise-ready and suitable for production deployments requiring high availability and scalability.
