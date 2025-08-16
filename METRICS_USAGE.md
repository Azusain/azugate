# AzuGate Metrics and Observability

The AzuGate proxy includes a comprehensive metrics and observability system built with Prometheus-compatible metrics collection, structured logging, and system monitoring.

## Features

### Metrics Collection
- **HTTP Request Metrics**: Request counts, response times, status codes, request/response sizes
- **Cache Metrics**: Hit/miss ratios, cache size, entry counts
- **Load Balancer Metrics**: Upstream health, request distribution, response times
- **Circuit Breaker Metrics**: State tracking, request success/failure rates
- **Connection Metrics**: Active connections, connection duration
- **System Metrics**: Memory usage, CPU utilization
- **Error Metrics**: Categorized error tracking

### Metric Types
- **Counters**: Monotonically increasing values (requests, errors)
- **Gauges**: Current values that can go up/down (active connections, memory)
- **Histograms**: Distribution of values with configurable buckets (response times)
- **Summaries**: Quantile calculations over sliding windows

### Endpoints
- `/metrics`: Prometheus-format metrics endpoint
- `/health`: Health check endpoint
- `/ready`: Readiness probe endpoint

## Configuration

### Basic Setup

```cpp
#include "metrics.hpp"

// Initialize metrics collection
azugate::MetricsCollector::start_background_collection();

// Start metrics server
azugate::MetricsServer server(9090);
server.start();
```

### Configuration Options

```cpp
azugate::MetricsConfig config;
config.enabled = true;
config.server_port = 9090;
config.metrics_path = "/metrics";
config.health_path = "/health";
config.collection_interval = std::chrono::seconds(10);
config.collect_system_metrics = true;
```

## Usage Examples

### Recording HTTP Requests

```cpp
// Using the convenience macro
RECORD_HTTP_REQUEST("GET", "/api/users", 200, std::chrono::milliseconds(150));

// Using the metrics interface directly
auto& metrics = azugate::GatewayMetrics::instance();
metrics.record_http_request("POST", "/api/login", 401, std::chrono::milliseconds(50));
metrics.record_http_request_size(1024);   // Request body size
metrics.record_http_response_size(2048);  // Response body size
```

### Cache Metrics

```cpp
// Record cache operations
RECORD_CACHE_HIT();
RECORD_CACHE_MISS();

// Update cache size statistics
auto& metrics = azugate::GatewayMetrics::instance();
metrics.record_cache_size(1500, 1024 * 1024 * 50); // 1500 entries, 50MB
```

### Load Balancer Metrics

```cpp
// Record upstream requests
RECORD_UPSTREAM_REQUEST("backend-1", true, std::chrono::milliseconds(200));

// Record health check results
auto& metrics = azugate::GatewayMetrics::instance();
metrics.record_upstream_health_check("backend-1", true);
metrics.record_upstream_health_check("backend-2", false);
```

### Circuit Breaker Metrics

```cpp
auto& metrics = azugate::GatewayMetrics::instance();

// Record circuit breaker state changes
metrics.record_circuit_breaker_state("user-service", 0); // 0=closed, 1=open, 2=half-open

// Record circuit breaker requests
metrics.record_circuit_breaker_request("user-service", "success");
metrics.record_circuit_breaker_request("user-service", "failure");
metrics.record_circuit_breaker_request("user-service", "rejected");
```

### Connection Tracking

```cpp
auto& metrics = azugate::GatewayMetrics::instance();

// Update active connection count
metrics.record_active_connections(150);

// Record connection duration when connection closes
metrics.record_connection_duration(std::chrono::milliseconds(30000));
```

### Error Tracking

```cpp
// Using the convenience macro
RECORD_ERROR("timeout", "upstream");
RECORD_ERROR("validation", "request");

// Using the metrics interface
auto& metrics = azugate::GatewayMetrics::instance();
metrics.record_error("connection_refused", "backend");
metrics.record_error("invalid_json", "client");
```

### Request Timing

```cpp
// Using RAII timer (automatically records on destruction)
{
    MEASURE_REQUEST_DURATION("database_query");
    // ... perform database operation ...
} // Timer automatically records duration

// Using manual timer with custom callback
azugate::RequestTimer timer([](std::chrono::milliseconds duration) {
    auto& metrics = azugate::GatewayMetrics::instance();
    metrics.record_http_request("GET", "/slow-endpoint", 200, duration);
});

// ... perform operation ...

timer.finish(); // Manually trigger the callback
```

## Middleware Integration

### HTTP Request Middleware

```cpp
class HttpHandler {
    void handle_request(const Request& req, Response& res) {
        auto start = std::chrono::steady_clock::now();
        
        azugate::HttpMetricsMiddleware::record_request_start(req.method(), req.path());
        
        try {
            // Process request...
            res.set_status(200);
            
        } catch (const std::exception& e) {
            res.set_status(500);
            RECORD_ERROR("internal_error", "handler");
        }
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);
            
        azugate::HttpMetricsMiddleware::record_request_complete(
            req.method(), req.path(), res.status_code(), duration,
            req.content_length(), res.content_length());
    }
};
```

## Prometheus Integration

### Scrape Configuration

```yaml
# prometheus.yml
scrape_configs:
  - job_name: 'azugate'
    static_configs:
      - targets: ['localhost:9090']
    metrics_path: '/metrics'
    scrape_interval: 15s
```

### Example Prometheus Queries

```promql
# Request rate
rate(azugate_http_requests_total[5m])

# Average response time
azugate_http_request_duration_seconds_sum / azugate_http_request_duration_seconds_count

# Error rate
rate(azugate_http_requests_total{status_code=~"5.."}[5m]) / rate(azugate_http_requests_total[5m])

# Cache hit rate
azugate_cache_hits_total / (azugate_cache_hits_total + azugate_cache_misses_total)

# Upstream health
azugate_upstream_healthy

# Memory usage
azugate_memory_usage_bytes

# Active connections
azugate_active_connections
```

## Grafana Dashboards

### Key Panels

1. **Request Rate**: `rate(azugate_http_requests_total[5m])`
2. **Response Time Percentiles**: 
   ```promql
   histogram_quantile(0.95, rate(azugate_http_request_duration_seconds_bucket[5m]))
   histogram_quantile(0.50, rate(azugate_http_request_duration_seconds_bucket[5m]))
   ```
3. **Error Rate**: 
   ```promql
   rate(azugate_http_requests_total{status_code=~"5.."}[5m]) / rate(azugate_http_requests_total[5m]) * 100
   ```
4. **Cache Performance**: 
   ```promql
   rate(azugate_cache_hits_total[5m]) / (rate(azugate_cache_hits_total[5m]) + rate(azugate_cache_misses_total[5m])) * 100
   ```
5. **System Resources**: 
   ```promql
   azugate_memory_usage_bytes
   azugate_cpu_usage_percent
   ```

## Alerting Rules

### Example Alerts

```yaml
# alerts.yml
groups:
  - name: azugate
    rules:
      - alert: HighErrorRate
        expr: rate(azugate_http_requests_total{status_code=~"5.."}[5m]) / rate(azugate_http_requests_total[5m]) > 0.1
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High error rate detected"
          
      - alert: HighResponseTime
        expr: histogram_quantile(0.95, rate(azugate_http_request_duration_seconds_bucket[5m])) > 1.0
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High response time detected"
          
      - alert: UpstreamDown
        expr: azugate_upstream_healthy == 0
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "Upstream server is down"
          
      - alert: HighMemoryUsage
        expr: azugate_memory_usage_bytes > 1e9  # 1GB
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High memory usage"
```

## Performance Considerations

### Metric Cardinality
- Be cautious with high-cardinality labels (user IDs, timestamps)
- Use path normalization to reduce URL variations
- Implement label limits to prevent memory exhaustion

### Collection Efficiency
- Background collection runs every 10 seconds by default
- System metrics collection is lightweight
- Histogram buckets are pre-configured for web traffic patterns

### Memory Management
- Metrics use atomic operations for thread safety
- Summary observations are trimmed automatically
- Label combinations are limited by default

## Integration with Existing Features

### Load Balancer Integration
```cpp
// In your load balancer selection logic
auto& metrics = azugate::GatewayMetrics::instance();
metrics.record_upstream_health_check(upstream_name, is_healthy);

// When forwarding requests
auto start = std::chrono::steady_clock::now();
bool success = forward_request_to_upstream(upstream, request);
auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now() - start);
metrics.record_upstream_request(upstream_name, success, duration);
```

### Cache Integration
```cpp
// In your cache lookup logic
if (auto cached_response = cache.get(cache_key)) {
    RECORD_CACHE_HIT();
    return cached_response;
} else {
    RECORD_CACHE_MISS();
    auto response = fetch_from_upstream();
    cache.put(cache_key, response);
    return response;
}

// Periodic cache size reporting
auto [entries, bytes] = cache.get_statistics();
auto& metrics = azugate::GatewayMetrics::instance();
metrics.record_cache_size(entries, bytes);
```

### Circuit Breaker Integration
```cpp
// In your circuit breaker logic
void on_state_change(const std::string& name, CircuitBreakerState new_state) {
    auto& metrics = azugate::GatewayMetrics::instance();
    metrics.record_circuit_breaker_state(name, static_cast<int>(new_state));
}

void on_request_result(const std::string& name, bool success, bool rejected) {
    auto& metrics = azugate::GatewayMetrics::instance();
    if (rejected) {
        metrics.record_circuit_breaker_request(name, "rejected");
    } else {
        metrics.record_circuit_breaker_request(name, success ? "success" : "failure");
    }
}
```

## Best Practices

1. **Label Naming**: Use consistent, descriptive label names
2. **Metric Naming**: Follow Prometheus naming conventions
3. **Cardinality**: Monitor label cardinality to prevent explosions
4. **Sampling**: Consider sampling for high-frequency events
5. **Documentation**: Document custom metrics and their meanings
6. **Testing**: Include metrics in your integration tests
7. **Monitoring**: Monitor the metrics system itself

The metrics system is designed to be lightweight, efficient, and provide comprehensive observability into your AzuGate proxy's performance and health.
