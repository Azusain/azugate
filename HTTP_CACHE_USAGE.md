# HTTP Response Cache System

## Overview

The azugate proxy now includes a high-performance HTTP response cache system that dramatically improves performance by caching responses from upstream servers. This system provides:

- **LRU Eviction**: Least Recently Used cache eviction for optimal memory management
- **TTL Support**: Time-To-Live expiration with configurable defaults
- **HTTP Compliance**: Full support for HTTP cache control headers
- **Conditional Requests**: ETag and Last-Modified validation
- **Thread-Safe Operations**: High-concurrency support with fine-grained locking
- **Real-time Statistics**: Cache hit/miss ratios and performance metrics

## Key Features

### 🚀 **Performance Benefits**
- **Reduced Latency**: Serve cached responses in microseconds instead of milliseconds
- **Lower Server Load**: Reduce backend server requests by up to 90%
- **Bandwidth Optimization**: Avoid repeated content transfers
- **Scalability**: Handle more concurrent users with same infrastructure

### 📊 **Smart Caching Logic**
- Respects HTTP `Cache-Control` headers (no-cache, no-store, max-age, private)
- Supports `Expires` header parsing
- Handles `Vary` headers for request variation
- Configurable per-path caching rules

### 🔒 **Enterprise Features**
- Configurable cache sizes and entry limits
- Path-based cache inclusion/exclusion rules
- Memory usage monitoring and automatic cleanup
- Cache statistics for observability

## Configuration

### Basic Configuration

```cpp
#include "http_cache.hpp"

// Initialize cache with custom configuration
HttpCacheConfig config;
config.max_size_bytes = 200 * 1024 * 1024;        // 200MB
config.max_entries = 20000;                        // 20k entries max
config.default_ttl = std::chrono::seconds(600);    // 10 minutes
config.max_ttl = std::chrono::seconds(3600);       // 1 hour max
config.min_ttl = std::chrono::seconds(30);         // 30 seconds min
config.respect_cache_control = true;               // Honor HTTP headers
config.enable_conditional_requests = true;         // ETag/Last-Modified support

HttpCacheManager::instance().initialize(config);
```

### Advanced Configuration

```cpp
HttpCacheConfig config;

// Path-based rules
config.no_cache_paths = {"/api/auth/", "/admin/", "/user/profile"};
config.force_cache_paths = {"/static/", "/assets/", "/images/"};

// Cache behavior
config.cache_private_responses = false;            // Don't cache private responses
config.cache_bypass_headers = {"Authorization", "Cookie"};

// Size limits
config.max_response_size = 5 * 1024 * 1024;       // Don't cache responses > 5MB
config.cacheable_methods = {"GET", "HEAD"};        // Only cache GET/HEAD
config.cacheable_status_codes = {200, 301, 302, 404}; // Cacheable status codes

HttpCacheManager::instance().initialize(config);
```

## Integration Example

Here's how to integrate caching into HTTP request handling:

```cpp
void handleHttpRequestWithCache() {
    auto cache = HttpCacheManager::instance().get_cache();
    
    // 1. Create cache key from request
    std::string method = "GET";
    std::string url = "/api/users/123";
    std::string query = "format=json&limit=10";
    std::unordered_map<std::string, std::string> headers = {
        {"accept", "application/json"},
        {"user-agent", "azugate/1.0"}
    };
    
    auto cache_key = cache->create_cache_key(method, url, query, headers);
    
    // 2. Check cache first
    if (cache->should_cache_request(method, url, headers)) {
        auto cached_entry = cache->get(cache_key);
        
        if (cached_entry) {
            // Cache hit! Check if revalidation needed
            if (!cached_entry->needs_revalidation(headers)) {
                SPDLOG_INFO("Cache HIT for {}", cache_key.to_string());
                
                // Serve from cache
                sendCachedResponse(*cached_entry);
                return;
            } else {
                // Send conditional request to upstream
                sendConditionalRequest(cache_key, cached_entry);
                return;
            }
        }
    }
    
    // 3. Cache miss - fetch from upstream
    SPDLOG_DEBUG("Cache MISS for {}", cache_key.to_string());
    
    auto upstream_response = fetchFromUpstream(url);
    
    // 4. Check if response should be cached
    auto response_headers = parseResponseHeaders(upstream_response);
    int status_code = upstream_response.status_code;
    size_t content_length = upstream_response.body.size();
    
    if (cache->should_cache_response(status_code, response_headers, content_length)) {
        // 5. Create cache entry and store
        auto cache_entry = cache->create_cache_entry(
            upstream_response.raw_data,
            status_code,
            response_headers
        );
        
        if (cache->put(cache_key, cache_entry)) {
            SPDLOG_DEBUG("Cached response for {}", cache_key.to_string());
        }
    }
    
    // 6. Send response to client
    sendResponse(upstream_response);
}
```

## Cache Control Headers

The cache system fully supports HTTP cache control headers:

### Cache-Control Directives

```http
# Don't cache this response
Cache-Control: no-store

# Cache for 1 hour
Cache-Control: max-age=3600

# Cache but revalidate before serving
Cache-Control: no-cache

# Private response (user-specific)
Cache-Control: private

# Public response (can be cached by proxies)
Cache-Control: public, max-age=86400
```

### Expires Header

```http
# Cache until specific date/time
Expires: Wed, 21 Oct 2024 07:28:00 GMT
```

### Conditional Request Headers

```http
# ETag-based validation
ETag: "abc123def456"
If-None-Match: "abc123def456"

# Last-Modified validation
Last-Modified: Wed, 21 Oct 2024 07:28:00 GMT
If-Modified-Since: Wed, 21 Oct 2024 07:28:00 GMT
```

## Command Line Options

```bash
# Enable caching with custom settings
./azugate \
  --config config.yaml \
  --enable-cache \
  --cache-size 200MB \
  --cache-ttl 600 \
  --cache-max-entries 20000
```

## Monitoring and Statistics

### Real-time Statistics

```cpp
// Get cache statistics
auto stats = HttpCacheManager::instance().get_cache()->get_stats();

SPDLOG_INFO("Cache Statistics:");
SPDLOG_INFO("  Hits: {}", stats.hits.load());
SPDLOG_INFO("  Misses: {}", stats.misses.load());
SPDLOG_INFO("  Hit Ratio: {:.2f}%", stats.hit_ratio() * 100);
SPDLOG_INFO("  Entries: {}", stats.current_entries.load());
SPDLOG_INFO("  Memory: {}MB", stats.current_size_bytes.load() / (1024*1024));
SPDLOG_INFO("  Evictions: {}", stats.evictions.load());
```

### Cache Management

```cpp
auto cache = HttpCacheManager::instance().get_cache();

// Manual cleanup
cache->cleanup_expired_entries();

// Force eviction
cache->force_evict_lru(100);  // Evict 100 LRU entries

// Clear entire cache
cache->clear();

// Check cache status
SPDLOG_INFO("Cache size: {} entries, {}MB", 
            cache->size(), 
            cache->memory_usage() / (1024*1024));
```

## YAML Configuration

```yaml
# config.yaml
http_cache:
  enabled: true
  max_size_mb: 200
  max_entries: 20000
  default_ttl_seconds: 600
  max_ttl_seconds: 3600
  min_ttl_seconds: 30
  respect_cache_control: true
  enable_conditional_requests: true
  
  # Path-based rules
  no_cache_paths:
    - "/api/auth/"
    - "/admin/"
    - "/user/profile"
    
  force_cache_paths:
    - "/static/"
    - "/assets/"
    - "/images/"
  
  # Cache behavior
  cache_private_responses: false
  max_response_size_mb: 5
  
  # Bypass headers
  cache_bypass_headers:
    - "Authorization"
    - "Cookie"
```

## Performance Optimization

### Memory Management
- Configure `max_size_bytes` based on available RAM
- Use `max_entries` to limit number of cached items
- Monitor memory usage with `memory_usage()` method

### Cache Efficiency
- Set appropriate TTL values for your content
- Use path-based rules to cache static content aggressively
- Exclude dynamic/private content from caching

### Monitoring
- Track hit ratio - aim for 70%+ for good performance
- Monitor eviction rates - high evictions may indicate undersized cache
- Watch expired entries - frequent expiration may indicate too-short TTL

## Best Practices

1. **Size the Cache Appropriately**
   - Allocate 10-20% of server RAM to cache
   - Monitor hit ratios and adjust size accordingly

2. **Configure TTL Wisely**
   - Static assets: Long TTL (hours/days)
   - API responses: Short TTL (minutes)
   - Dynamic content: Very short TTL or no cache

3. **Use Path-Based Rules**
   - Force cache static assets regardless of headers
   - Never cache authentication endpoints
   - Exclude user-specific content

4. **Monitor Performance**
   - Log cache statistics regularly
   - Alert on low hit ratios
   - Monitor memory usage trends

5. **Conditional Requests**
   - Enable ETag/Last-Modified support
   - Reduces bandwidth even on cache misses
   - Improves user experience

## Integration with Load Balancer

The cache works seamlessly with the load balancer:

```cpp
// Cache-aware load balancing
void handleRequestWithCacheAndLB() {
    auto cache = HttpCacheManager::instance().get_cache();
    auto lb = get_load_balancer_for_route(route);
    
    // Check cache first
    auto cached_response = checkCache(request);
    if (cached_response) {
        return serveCachedResponse(cached_response);
    }
    
    // Cache miss - use load balancer
    auto server = lb->get_server(client_ip);
    auto response = fetchFromServer(server);
    
    // Cache the response for next time
    cacheResponse(request, response);
    return response;
}
```

This HTTP cache system transforms azugate into a high-performance caching proxy capable of handling enterprise-scale traffic with dramatically improved response times and reduced backend load.

## Example Scenarios

### Static Asset Caching
```cpp
// CSS/JS files cached for 24 hours
config.force_cache_paths = {"/css/", "/js/", "/images/"};
// Override any no-cache headers from CDN
```

### API Response Caching  
```cpp
// Cache API responses for 5 minutes
config.default_ttl = std::chrono::seconds(300);
// Respect backend cache-control headers
config.respect_cache_control = true;
```

### Security-Sensitive Paths
```cpp
// Never cache authentication or user data
config.no_cache_paths = {"/api/auth/", "/user/", "/admin/"};
config.cache_bypass_headers = {"Authorization"};
```

This comprehensive caching system provides enterprise-grade HTTP caching capabilities that rival commercial solutions like Varnish or NGINX Plus.
