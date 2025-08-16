#include "../../include/config_manager.hpp"
#include <fstream>

namespace azugate {

std::string ConfigTemplateGenerator::add_section_header(const std::string& title, const std::string& description) {
    std::string result = "\n# " + title + "\n";
    if (!description.empty()) {
        result += "# " + description + "\n";
    }
    result += "\n";
    return result;
}

std::string ConfigTemplateGenerator::add_commented_field(const std::string& field, const std::string& value, const std::string& description, int indent) {
    std::string result;
    std::string indentation(indent, ' ');
    
    if (!description.empty()) {
        result += indentation + "# " + description + "\n";
    }
    result += indentation + field + ": " + value + "\n";
    
    return result;
}

std::string ConfigTemplateGenerator::generate_full_template() {
    std::string config = R"(# AzuGate Proxy Configuration File
# This is a comprehensive configuration template showing all available options
# Uncomment and modify the settings you need

)" + add_section_header("Server Configuration", "Basic server settings");

    config += R"(server:
  # Port to listen on for incoming requests
  port: 8080
  
  # Host/IP to bind to (0.0.0.0 for all interfaces)
  host: "0.0.0.0"
  
  # Number of worker threads (default: CPU cores)
  worker_threads: 4
  
  # SSL/TLS configuration
  ssl:
    enabled: false
    cert_file: "/path/to/certificate.crt"
    key_file: "/path/to/private.key"
    
  # Connection settings
  keep_alive_timeout: "75s"
  read_timeout: "30s"
  write_timeout: "30s"

)" + add_section_header("Routes Configuration", "Define how requests are routed");

    config += R"(routes:
  # Static file serving
  - path: "/static/*"
    file_server:
      root: "/var/www/html"
      index_files: ["index.html", "index.htm"]
      directory_listing: true
      cache_control: "public, max-age=3600"
  
  # Reverse proxy to upstream servers
  - path: "/api/*"
    upstream:
      servers:
        - host: "localhost"
          port: 3000
          weight: 1
        - host: "localhost"
          port: 3001
          weight: 1
      strategy: "round_robin"  # round_robin, least_connections, weighted, ip_hash
      health_check:
        enabled: true
        path: "/health"
        interval: "30s"
        timeout: "5s"
  
  # TCP proxy (for non-HTTP protocols)
  - path: "/tcp/*"
    tcp_proxy:
      target_host: "backend.example.com"
      target_port: 5432
      buffer_size: 8192

)" + add_section_header("Authentication Configuration", "JWT and API key authentication");

    config += R"(auth:
  # JWT Authentication
  jwt:
    enabled: false
    secret_key: "your-super-secret-jwt-key-change-this-in-production"
    algorithm: "HS256"
    expiry: "24h"
    
  # API Key Authentication  
  api_key:
    enabled: false
    header_name: "X-API-Key"
    keys:
      - "api-key-1"
      - "api-key-2"

)" + add_section_header("Caching Configuration", "HTTP response caching");

    config += R"(cache:
  enabled: true
  type: "lru"  # lru, redis
  max_size: "100MB"
  max_entries: 10000
  ttl: "1h"
  
  # Cache rules
  rules:
    - path: "/api/data/*"
      ttl: "5m"
      vary_headers: ["Accept-Language", "Authorization"]
    - path: "/static/*"
      ttl: "24h"
      cache_private: false

)" + add_section_header("Load Balancer Configuration", "Upstream server management");

    config += R"(load_balancer:
  strategy: "round_robin"
  health_checks:
    enabled: true
    interval: "30s"
    timeout: "5s"
    unhealthy_threshold: 3
    healthy_threshold: 2
  
  # Session affinity
  session_affinity:
    enabled: false
    type: "cookie"  # cookie, ip_hash
    cookie_name: "azugate_session"

)" + add_section_header("Circuit Breaker Configuration", "Fault tolerance and resilience");

    config += R"(circuit_breaker:
  enabled: true
  failure_threshold: 5
  success_threshold: 3
  timeout: "60s"
  
  # Per-route circuit breakers
  routes:
    - path: "/api/critical/*"
      failure_threshold: 2
      timeout: "30s"

)" + add_section_header("Rate Limiting Configuration", "Request rate limiting");

    config += R"(rate_limiter:
  enabled: true
  type: "token_bucket"
  requests_per_second: 100
  burst_size: 200
  
  # Per-IP rate limiting
  per_ip:
    enabled: true
    requests_per_second: 10
    burst_size: 20
    
  # Per-route rate limits
  routes:
    - path: "/api/upload/*"
      requests_per_second: 5
      burst_size: 10

)" + add_section_header("Compression Configuration", "Response compression");

    config += R"(compression:
  enabled: true
  algorithms: ["gzip", "deflate"]
  level: 6  # 1-9, higher = better compression, slower
  min_size: 1024  # minimum response size to compress (bytes)
  
  # MIME types to compress
  mime_types:
    - "text/html"
    - "text/css"
    - "text/javascript"
    - "application/json"
    - "application/xml"

)" + add_section_header("Metrics and Observability", "Prometheus metrics and monitoring");

    config += R"(metrics:
  enabled: true
  port: 9090
  path: "/metrics"
  
  # Additional endpoints
  endpoints:
    health: "/health"
    ready: "/ready"
    config: "/config"
  
  # Collection settings
  collection_interval: "10s"
  system_metrics: true

)" + add_section_header("Logging Configuration", "Application logging settings");

    config += R"(logging:
  level: "info"  # trace, debug, info, warn, error, critical
  format: "text"  # text, json
  output: "stdout"  # stdout, stderr, file path
  
  # Log rotation (when output is a file)
  rotation:
    max_size: "100MB"
    max_files: 10
    max_age: "30d"
  
  # Access logging
  access_log:
    enabled: true
    format: 'combined'  # combined, common, custom
    output: "/var/log/azugate/access.log"

)" + add_section_header("Security Configuration", "Security headers and settings");

    config += R"(security:
  # Security headers
  headers:
    x_frame_options: "DENY"
    x_content_type_options: "nosniff"
    x_xss_protection: "1; mode=block"
    strict_transport_security: "max-age=31536000; includeSubDomains"
    content_security_policy: "default-src 'self'"
  
  # CORS settings
  cors:
    enabled: false
    allowed_origins: ["*"]
    allowed_methods: ["GET", "POST", "PUT", "DELETE", "OPTIONS"]
    allowed_headers: ["Content-Type", "Authorization"]
    max_age: "86400"

)" + add_section_header("Development Settings", "Settings for development environment");

    config += R"(# Development-only settings (remove in production)
development:
  debug: false
  hot_reload_config: true
  detailed_errors: false
  profiling: false
  
# Configuration validation
config:
  validation:
    strict: true
    warn_unused: true
  hot_reload: true
)";

    return config;
}

std::string ConfigTemplateGenerator::generate_minimal_template() {
    return R"(# Minimal AzuGate Configuration
# This configuration provides basic reverse proxy functionality

server:
  port: 8080
  host: "0.0.0.0"

routes:
  # Proxy API requests to backend
  - path: "/api/*"
    upstream:
      servers:
        - host: "localhost"
          port: 3000
  
  # Serve static files
  - path: "/*"
    file_server:
      root: "./public"

# Optional: Enable metrics
metrics:
  enabled: true
  port: 9090

# Optional: Enable basic logging
logging:
  level: "info"
)";
}

std::string ConfigTemplateGenerator::generate_development_template() {
    return R"(# Development Configuration for AzuGate
# Optimized for local development with debugging features

server:
  port: 8080
  host: "localhost"
  worker_threads: 2

routes:
  # Development API server
  - path: "/api/*"
    upstream:
      servers:
        - host: "localhost"
          port: 3000
      health_check:
        enabled: true
        interval: "10s"
  
  # Static assets with no caching during development
  - path: "/assets/*"
    file_server:
      root: "./assets"
  
  # Default route for SPA development
  - path: "/*"
    file_server:
      root: "./public"
      index_files: ["index.html"]

# Caching disabled for development
cache:
  enabled: false

# Detailed logging for debugging
logging:
  level: "debug"
  format: "text"

# Development-friendly metrics
metrics:
  enabled: true
  port: 9090

# Hot reload configuration changes
development:
  debug: true
  hot_reload_config: true
  detailed_errors: true

config:
  hot_reload: true
  validation:
    strict: false
)";
}

std::string ConfigTemplateGenerator::generate_production_template() {
    return R"(# Production Configuration for AzuGate
# Optimized for production deployment with security and performance

server:
  port: 80
  host: "0.0.0.0"
  worker_threads: 8  # Adjust based on CPU cores
  
  # SSL/TLS configuration (recommended for production)
  ssl:
    enabled: true
    cert_file: "/etc/ssl/certs/azugate.crt"
    key_file: "/etc/ssl/private/azugate.key"

routes:
  # API routes with load balancing
  - path: "/api/*"
    upstream:
      servers:
        - host: "backend-1.internal"
          port: 8080
          weight: 1
        - host: "backend-2.internal"
          port: 8080
          weight: 1
        - host: "backend-3.internal"
          port: 8080
          weight: 1
      strategy: "least_connections"
      health_check:
        enabled: true
        path: "/health"
        interval: "30s"
        timeout: "5s"
        unhealthy_threshold: 3

  # Static assets with aggressive caching
  - path: "/static/*"
    file_server:
      root: "/var/www/static"
      cache_control: "public, max-age=31536000"  # 1 year

  # Default route
  - path: "/*"
    file_server:
      root: "/var/www/html"
      index_files: ["index.html"]

# Authentication for API endpoints
auth:
  jwt:
    enabled: true
    secret_key: "${JWT_SECRET_KEY}"  # Load from environment
    algorithm: "HS256"
    expiry: "24h"

# Production caching
cache:
  enabled: true
  type: "lru"
  max_size: "1GB"
  max_entries: 100000
  ttl: "1h"
  
  rules:
    - path: "/api/data/*"
      ttl: "10m"
    - path: "/static/*"
      ttl: "24h"

# Load balancer with health checks
load_balancer:
  strategy: "least_connections"
  health_checks:
    enabled: true
    interval: "30s"
    timeout: "5s"
    unhealthy_threshold: 3
    healthy_threshold: 2

# Circuit breaker for fault tolerance
circuit_breaker:
  enabled: true
  failure_threshold: 5
  success_threshold: 3
  timeout: "60s"

# Rate limiting
rate_limiter:
  enabled: true
  requests_per_second: 1000
  burst_size: 2000
  
  per_ip:
    enabled: true
    requests_per_second: 50
    burst_size: 100

# Compression for better performance
compression:
  enabled: true
  algorithms: ["gzip", "br"]  # Brotli for better compression
  level: 6
  min_size: 1024

# Production metrics
metrics:
  enabled: true
  port: 9090
  collection_interval: "15s"
  system_metrics: true

# Production logging
logging:
  level: "warn"  # Reduce log noise in production
  format: "json"  # Structured logging
  output: "/var/log/azugate/azugate.log"
  
  rotation:
    max_size: "100MB"
    max_files: 30
    max_age: "30d"
  
  access_log:
    enabled: true
    format: "combined"
    output: "/var/log/azugate/access.log"

# Security headers
security:
  headers:
    x_frame_options: "DENY"
    x_content_type_options: "nosniff"
    x_xss_protection: "1; mode=block"
    strict_transport_security: "max-age=31536000; includeSubDomains; preload"
    content_security_policy: "default-src 'self'; script-src 'self' 'unsafe-inline'"
  
  cors:
    enabled: true
    allowed_origins: ["https://yourdomain.com"]
    allowed_methods: ["GET", "POST", "PUT", "DELETE", "OPTIONS"]
    allowed_headers: ["Content-Type", "Authorization"]

# Production configuration settings
config:
  validation:
    strict: true
    warn_unused: true
  hot_reload: false  # Disable in production for security
)";
}

bool ConfigManager::generate_sample_config(const std::string& output_path) {
    try {
        std::ofstream file(output_path);
        if (!file.is_open()) {
            SPDLOG_ERROR("Failed to create configuration file: {}", output_path);
            return false;
        }
        
        // Generate full template by default
        file << ConfigTemplateGenerator::generate_full_template();
        file.close();
        
        SPDLOG_INFO("Sample configuration generated: {}", output_path);
        return true;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Error generating sample configuration: {}", e.what());
        return false;
    }
}

} // namespace azugate
