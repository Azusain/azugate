#ifndef __HTTP_CACHE_HPP
#define __HTTP_CACHE_HPP

#include <boost/beast/http.hpp>
#include <chrono>
#include <list>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <spdlog/spdlog.h>
#include <atomic>

namespace azugate {

// Cache entry representing a cached HTTP response
struct CacheEntry {
    std::string response_data;                    // Complete HTTP response
    std::chrono::steady_clock::time_point created_at;
    std::chrono::steady_clock::time_point expires_at;
    std::string etag;                            // For conditional requests
    std::string last_modified;                   // For conditional requests
    size_t content_length;
    std::string content_type;
    bool is_private;                             // Cache-Control: private
    bool no_cache;                              // Cache-Control: no-cache
    bool no_store;                              // Cache-Control: no-store
    bool must_revalidate;                       // Cache-Control: must-revalidate
    int status_code;
    size_t hit_count;                           // Number of times served from cache
    size_t size_bytes;                          // Memory usage
    
    CacheEntry() : created_at(std::chrono::steady_clock::now()),
                   expires_at(std::chrono::steady_clock::now()),
                   content_length(0), is_private(false), no_cache(false),
                   no_store(false), must_revalidate(false), status_code(200),
                   hit_count(0), size_bytes(0) {}
    
    bool is_expired() const {
        return std::chrono::steady_clock::now() >= expires_at;
    }
    
    bool is_cacheable() const {
        return !no_store && status_code == 200;
    }
    
    std::chrono::seconds age() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(now - created_at);
    }
};

// Configuration for HTTP cache behavior
struct HttpCacheConfig {
    size_t max_size_bytes = 100 * 1024 * 1024;     // 100MB default
    size_t max_entries = 10000;                     // Maximum number of entries
    std::chrono::seconds default_ttl{300};          // 5 minutes default TTL
    std::chrono::seconds max_ttl{3600};             // 1 hour maximum TTL
    std::chrono::seconds min_ttl{60};               // 1 minute minimum TTL
    bool respect_cache_control = true;              // Honor Cache-Control headers
    bool enable_conditional_requests = true;        // Support ETag/Last-Modified
    std::vector<std::string> cacheable_methods = {"GET", "HEAD"};
    std::vector<int> cacheable_status_codes = {200, 203, 300, 301, 302, 404, 410};
    size_t max_response_size = 1024 * 1024;        // 1MB max response size to cache
    bool cache_private_responses = false;           // Don't cache private responses
    std::vector<std::string> cache_bypass_headers = {"Authorization"};
    
    // Paths or patterns to never cache
    std::vector<std::string> no_cache_paths = {"/api/auth/", "/admin/"};
    
    // Force cache certain paths regardless of headers
    std::vector<std::string> force_cache_paths = {"/static/", "/assets/"};
};

// Key used for cache lookups
struct CacheKey {
    std::string method;
    std::string url;
    std::string query_params;
    std::string vary_headers;  // Headers that affect response variation
    
    std::string to_string() const {
        return method + ":" + url + "?" + query_params + "#" + vary_headers;
    }
    
    bool operator==(const CacheKey& other) const {
        return method == other.method && 
               url == other.url && 
               query_params == other.query_params &&
               vary_headers == other.vary_headers;
    }
};

// Hash function for CacheKey
struct CacheKeyHash {
    std::size_t operator()(const CacheKey& key) const {
        return std::hash<std::string>{}(key.to_string());
    }
};

// LRU Cache implementation for HTTP responses
class HttpCache {
public:
    explicit HttpCache(const HttpCacheConfig& config = HttpCacheConfig{});
    ~HttpCache() = default;
    
    // Main cache operations
    std::optional<std::shared_ptr<CacheEntry>> get(const CacheKey& key);
    bool put(const CacheKey& key, std::shared_ptr<CacheEntry> entry);
    bool remove(const CacheKey& key);
    void clear();
    
    // Cache analysis methods
    bool should_cache_request(const std::string& method, 
                             const std::string& path,
                             const std::unordered_map<std::string, std::string>& headers) const;
    
    bool should_cache_response(int status_code,
                              const std::unordered_map<std::string, std::string>& headers,
                              size_t content_length) const;
    
    std::chrono::seconds calculate_ttl(const std::unordered_map<std::string, std::string>& headers) const;
    
    std::shared_ptr<CacheEntry> create_cache_entry(
        const std::string& response_data,
        int status_code,
        const std::unordered_map<std::string, std::string>& headers) const;
    
    CacheKey create_cache_key(const std::string& method,
                             const std::string& url,
                             const std::string& query_params,
                             const std::unordered_map<std::string, std::string>& headers) const;
    
    // Conditional request support
    bool needs_revalidation(const std::shared_ptr<CacheEntry>& entry,
                           const std::unordered_map<std::string, std::string>& request_headers) const;
    
    std::string create_conditional_request_headers(const std::shared_ptr<CacheEntry>& entry) const;
    
    // Cache statistics and management
    struct CacheStats {
        std::atomic<uint64_t> hits{0};
        std::atomic<uint64_t> misses{0};
        std::atomic<uint64_t> stores{0};
        std::atomic<uint64_t> evictions{0};
        std::atomic<uint64_t> expired_entries{0};
        std::atomic<size_t> current_size_bytes{0};
        std::atomic<size_t> current_entries{0};
        
        double hit_ratio() const {
            uint64_t total = hits.load() + misses.load();
            return total > 0 ? static_cast<double>(hits.load()) / total : 0.0;
        }
    };
    
    const CacheStats& get_stats() const { return stats_; }
    void reset_stats();
    
    // Configuration
    void update_config(const HttpCacheConfig& config);
    const HttpCacheConfig& get_config() const { return config_; }
    
    // Maintenance operations
    void cleanup_expired_entries();
    void force_evict_lru(size_t count = 1);
    
    // Thread-safe size information
    size_t size() const;
    size_t memory_usage() const;
    bool is_full() const;

private:
    // LRU list node
    using LRUNode = std::pair<CacheKey, std::shared_ptr<CacheEntry>>;
    using LRUList = std::list<LRUNode>;
    using LRUIterator = LRUList::iterator;
    
    // Internal methods
    void move_to_front(LRUIterator it);
    void evict_lru();
    void evict_if_needed();
    bool is_path_cacheable(const std::string& path) const;
    bool is_path_force_cached(const std::string& path) const;
    std::string extract_vary_headers(const std::unordered_map<std::string, std::string>& request_headers,
                                   const std::string& vary_header) const;
    
    // Thread synchronization
    mutable std::shared_mutex mutex_;
    
    // Cache data structures
    HttpCacheConfig config_;
    LRUList lru_list_;
    std::unordered_map<CacheKey, LRUIterator, CacheKeyHash> cache_map_;
    
    // Statistics
    mutable CacheStats stats_;
    
    // Background cleanup
    std::chrono::steady_clock::time_point last_cleanup_;
};

// Utility functions for HTTP cache integration
class HttpCacheManager {
public:
    static HttpCacheManager& instance();
    
    void initialize(const HttpCacheConfig& config = HttpCacheConfig{});
    void shutdown();
    
    std::shared_ptr<HttpCache> get_cache() { return cache_; }
    
    // Helper methods for integration with HTTP handlers
    static bool is_cacheable_method(const std::string& method);
    static bool is_cacheable_status(int status_code);
    static std::unordered_map<std::string, std::string> parse_http_headers(
        const boost::beast::http::fields& fields);
    
    // Cache control header parsing
    struct CacheControlDirectives {
        bool no_cache = false;
        bool no_store = false;
        bool must_revalidate = false;
        bool is_private = false;
        bool is_public = false;
        std::optional<std::chrono::seconds> max_age;
        std::optional<std::chrono::seconds> s_maxage;
    };
    
    static CacheControlDirectives parse_cache_control(const std::string& cache_control_header);
    static std::chrono::seconds parse_expires_header(const std::string& expires_header);

private:
    std::shared_ptr<HttpCache> cache_;
    std::mutex init_mutex_;
    bool initialized_ = false;
};

// Macro for easy integration in HTTP handlers
#define HTTP_CACHE_GET(key) HttpCacheManager::instance().get_cache()->get(key)
#define HTTP_CACHE_PUT(key, entry) HttpCacheManager::instance().get_cache()->put(key, entry)
#define HTTP_CACHE_STATS() HttpCacheManager::instance().get_cache()->get_stats()

} // namespace azugate

#endif // __HTTP_CACHE_HPP
