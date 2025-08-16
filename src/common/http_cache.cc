#include "../../include/http_cache.hpp"
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <shared_mutex>
#include <sstream>

namespace azugate {

// HttpCache Implementation
HttpCache::HttpCache(const HttpCacheConfig& config) 
    : config_(config), last_cleanup_(std::chrono::steady_clock::now()) {
    SPDLOG_INFO("HTTP cache initialized - Max size: {}MB, Max entries: {}", 
                config_.max_size_bytes / (1024 * 1024), config_.max_entries);
}

std::optional<std::shared_ptr<CacheEntry>> HttpCache::get(const CacheKey& key) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    auto it = cache_map_.find(key);
    if (it == cache_map_.end()) {
        stats_.misses++;
        return std::nullopt;
    }
    
    auto& entry = it->second->second;
    
    // Check if entry is expired
    if (entry->is_expired()) {
        stats_.expired_entries++;
        stats_.misses++;
        
        // Remove expired entry (need to upgrade to exclusive lock)
        lock.unlock();
        std::unique_lock<std::shared_mutex> write_lock(mutex_);
        
        // Double-check the entry is still there and still expired
        auto check_it = cache_map_.find(key);
        if (check_it != cache_map_.end() && check_it->second->second->is_expired()) {
            stats_.current_size_bytes -= check_it->second->second->size_bytes;
            stats_.current_entries--;
            lru_list_.erase(check_it->second);
            cache_map_.erase(check_it);
        }
        
        return std::nullopt;
    }
    
    // Move to front of LRU list (need to upgrade to exclusive lock)
    lock.unlock();
    std::unique_lock<std::shared_mutex> write_lock(mutex_);
    
    // Double-check the entry is still there
    auto check_it = cache_map_.find(key);
    if (check_it != cache_map_.end()) {
        move_to_front(check_it->second);
        check_it->second->second->hit_count++;
        stats_.hits++;
        return check_it->second->second;
    }
    
    stats_.misses++;
    return std::nullopt;
}

bool HttpCache::put(const CacheKey& key, std::shared_ptr<CacheEntry> entry) {
    if (!entry || !entry->is_cacheable() || entry->size_bytes > config_.max_response_size) {
        return false;
    }
    
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    // Check if key already exists
    auto existing_it = cache_map_.find(key);
    if (existing_it != cache_map_.end()) {
        // Update existing entry
        stats_.current_size_bytes -= existing_it->second->second->size_bytes;
        existing_it->second->second = entry;
        stats_.current_size_bytes += entry->size_bytes;
        move_to_front(existing_it->second);
        return true;
    }
    
    // Ensure we have space
    evict_if_needed();
    
    // Add new entry
    lru_list_.push_front({key, entry});
    cache_map_[key] = lru_list_.begin();
    
    stats_.stores++;
    stats_.current_entries++;
    stats_.current_size_bytes += entry->size_bytes;
    
    SPDLOG_DEBUG("Cached response: {} (size: {} bytes, TTL: {}s)", 
                key.to_string(), entry->size_bytes, 
                std::chrono::duration_cast<std::chrono::seconds>(
                    entry->expires_at - entry->created_at).count());
    
    return true;
}

bool HttpCache::remove(const CacheKey& key) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto it = cache_map_.find(key);
    if (it == cache_map_.end()) {
        return false;
    }
    
    stats_.current_size_bytes -= it->second->second->size_bytes;
    stats_.current_entries--;
    lru_list_.erase(it->second);
    cache_map_.erase(it);
    
    return true;
}

void HttpCache::clear() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    lru_list_.clear();
    cache_map_.clear();
    stats_.current_entries = 0;
    stats_.current_size_bytes = 0;
    
    SPDLOG_INFO("HTTP cache cleared");
}

bool HttpCache::should_cache_request(const std::string& method, 
                                   const std::string& path,
                                   const std::unordered_map<std::string, std::string>& headers) const {
    // Check method
    if (std::find(config_.cacheable_methods.begin(), config_.cacheable_methods.end(), method) 
        == config_.cacheable_methods.end()) {
        return false;
    }
    
    // Check if path is explicitly excluded
    if (!is_path_cacheable(path)) {
        return false;
    }
    
    // Check for cache bypass headers
    for (const auto& bypass_header : config_.cache_bypass_headers) {
        if (headers.find(bypass_header) != headers.end()) {
            return false;
        }
    }
    
    return true;
}

bool HttpCache::should_cache_response(int status_code,
                                    const std::unordered_map<std::string, std::string>& headers,
                                    size_t content_length) const {
    // Check status code
    if (std::find(config_.cacheable_status_codes.begin(), 
                  config_.cacheable_status_codes.end(), status_code) 
        == config_.cacheable_status_codes.end()) {
        return false;
    }
    
    // Check content length
    if (content_length > config_.max_response_size) {
        return false;
    }
    
    // Check Cache-Control headers
    auto cache_control_it = headers.find("cache-control");
    if (cache_control_it != headers.end()) {
        auto directives = HttpCacheManager::parse_cache_control(cache_control_it->second);
        
        if (directives.no_store) {
            return false;
        }
        
        if (directives.is_private && !config_.cache_private_responses) {
            return false;
        }
        
        if (directives.no_cache && config_.respect_cache_control) {
            return false;
        }
    }
    
    return true;
}

std::chrono::seconds HttpCache::calculate_ttl(const std::unordered_map<std::string, std::string>& headers) const {
    std::chrono::seconds ttl = config_.default_ttl;
    
    if (!config_.respect_cache_control) {
        return std::clamp(ttl, config_.min_ttl, config_.max_ttl);
    }
    
    // Check Cache-Control max-age
    auto cache_control_it = headers.find("cache-control");
    if (cache_control_it != headers.end()) {
        auto directives = HttpCacheManager::parse_cache_control(cache_control_it->second);
        
        if (directives.max_age.has_value()) {
            ttl = directives.max_age.value();
        } else if (directives.s_maxage.has_value()) {
            ttl = directives.s_maxage.value(); // s-maxage takes precedence for proxies
        }
    }
    
    // Check Expires header if no Cache-Control max-age
    if (ttl == config_.default_ttl) {
        auto expires_it = headers.find("expires");
        if (expires_it != headers.end()) {
            auto expires_ttl = HttpCacheManager::parse_expires_header(expires_it->second);
            if (expires_ttl.count() > 0) {
                ttl = expires_ttl;
            }
        }
    }
    
    // Clamp to configured limits
    return std::clamp(ttl, config_.min_ttl, config_.max_ttl);
}

std::shared_ptr<CacheEntry> HttpCache::create_cache_entry(
    const std::string& response_data,
    int status_code,
    const std::unordered_map<std::string, std::string>& headers) const {
    
    auto entry = std::make_shared<CacheEntry>();
    auto now = std::chrono::steady_clock::now();
    
    entry->response_data = response_data;
    entry->status_code = status_code;
    entry->created_at = now;
    entry->size_bytes = response_data.size();
    
    // Calculate TTL and expiration
    auto ttl = calculate_ttl(headers);
    entry->expires_at = now + ttl;
    
    // Extract relevant headers
    auto content_type_it = headers.find("content-type");
    if (content_type_it != headers.end()) {
        entry->content_type = content_type_it->second;
    }
    
    auto etag_it = headers.find("etag");
    if (etag_it != headers.end()) {
        entry->etag = etag_it->second;
    }
    
    auto last_modified_it = headers.find("last-modified");
    if (last_modified_it != headers.end()) {
        entry->last_modified = last_modified_it->second;
    }
    
    auto content_length_it = headers.find("content-length");
    if (content_length_it != headers.end()) {
        entry->content_length = std::stoull(content_length_it->second);
    }
    
    // Parse Cache-Control directives
    auto cache_control_it = headers.find("cache-control");
    if (cache_control_it != headers.end()) {
        auto directives = HttpCacheManager::parse_cache_control(cache_control_it->second);
        entry->no_cache = directives.no_cache;
        entry->no_store = directives.no_store;
        entry->must_revalidate = directives.must_revalidate;
        entry->is_private = directives.is_private;
    }
    
    return entry;
}

CacheKey HttpCache::create_cache_key(const std::string& method,
                                   const std::string& url,
                                   const std::string& query_params,
                                   const std::unordered_map<std::string, std::string>& headers) const {
    CacheKey key;
    key.method = method;
    key.url = url;
    key.query_params = query_params;
    
    // Handle Vary header for response variation
    auto vary_it = headers.find("vary");
    if (vary_it != headers.end()) {
        key.vary_headers = extract_vary_headers(headers, vary_it->second);
    }
    
    return key;
}

bool HttpCache::needs_revalidation(const std::shared_ptr<CacheEntry>& entry,
                                 const std::unordered_map<std::string, std::string>& request_headers) const {
    if (!config_.enable_conditional_requests) {
        return false;
    }
    
    // Check must-revalidate directive
    if (entry->must_revalidate) {
        return true;
    }
    
    // Check If-None-Match (ETag)
    auto if_none_match_it = request_headers.find("if-none-match");
    if (if_none_match_it != request_headers.end() && !entry->etag.empty()) {
        return if_none_match_it->second != entry->etag;
    }
    
    // Check If-Modified-Since
    auto if_modified_since_it = request_headers.find("if-modified-since");
    if (if_modified_since_it != request_headers.end() && !entry->last_modified.empty()) {
        // Simple string comparison for now - could be improved with date parsing
        return if_modified_since_it->second != entry->last_modified;
    }
    
    return false;
}

std::string HttpCache::create_conditional_request_headers(const std::shared_ptr<CacheEntry>& entry) const {
    std::string headers;
    
    if (!entry->etag.empty()) {
        headers += "If-None-Match: " + entry->etag + "\r\n";
    }
    
    if (!entry->last_modified.empty()) {
        headers += "If-Modified-Since: " + entry->last_modified + "\r\n";
    }
    
    return headers;
}

void HttpCache::reset_stats() {
    stats_.hits = 0;
    stats_.misses = 0;
    stats_.stores = 0;
    stats_.evictions = 0;
    stats_.expired_entries = 0;
}

void HttpCache::update_config(const HttpCacheConfig& config) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    config_ = config;
    
    // Evict if new limits are exceeded
    evict_if_needed();
    
    SPDLOG_INFO("HTTP cache configuration updated");
}

void HttpCache::cleanup_expired_entries() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    // Only run cleanup periodically
    if (now - last_cleanup_ < std::chrono::minutes(1)) {
        return;
    }
    
    auto it = lru_list_.rbegin(); // Start from least recently used
    size_t removed = 0;
    
    while (it != lru_list_.rend()) {
        if (it->second->is_expired()) {
            auto to_remove = std::next(it).base();
            --it; // Move iterator before removal
            
            stats_.current_size_bytes -= to_remove->second->size_bytes;
            stats_.current_entries--;
            stats_.expired_entries++;
            cache_map_.erase(to_remove->first);
            lru_list_.erase(to_remove);
            removed++;
        } else {
            ++it;
        }
    }
    
    last_cleanup_ = now;
    
    if (removed > 0) {
        SPDLOG_DEBUG("Cleaned up {} expired cache entries", removed);
    }
}

void HttpCache::force_evict_lru(size_t count) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    for (size_t i = 0; i < count && !lru_list_.empty(); ++i) {
        evict_lru();
    }
}

size_t HttpCache::size() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return stats_.current_entries.load();
}

size_t HttpCache::memory_usage() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return stats_.current_size_bytes.load();
}

bool HttpCache::is_full() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return stats_.current_entries >= config_.max_entries || 
           stats_.current_size_bytes >= config_.max_size_bytes;
}

// Private methods
void HttpCache::move_to_front(LRUIterator it) {
    lru_list_.splice(lru_list_.begin(), lru_list_, it);
}

void HttpCache::evict_lru() {
    if (lru_list_.empty()) {
        return;
    }
    
    auto& back = lru_list_.back();
    stats_.current_size_bytes -= back.second->size_bytes;
    stats_.current_entries--;
    stats_.evictions++;
    
    cache_map_.erase(back.first);
    lru_list_.pop_back();
}

void HttpCache::evict_if_needed() {
    // Evict expired entries first
    cleanup_expired_entries();
    
    // Evict LRU entries if still over limits
    while ((stats_.current_entries >= config_.max_entries || 
            stats_.current_size_bytes >= config_.max_size_bytes) &&
           !lru_list_.empty()) {
        evict_lru();
    }
}

bool HttpCache::is_path_cacheable(const std::string& path) const {
    // Check if path should never be cached
    for (const auto& no_cache_path : config_.no_cache_paths) {
        if (path.find(no_cache_path) == 0) {
            return false;
        }
    }
    
    return true;
}

bool HttpCache::is_path_force_cached(const std::string& path) const {
    // Check if path should always be cached
    for (const auto& force_cache_path : config_.force_cache_paths) {
        if (path.find(force_cache_path) == 0) {
            return true;
        }
    }
    
    return false;
}

std::string HttpCache::extract_vary_headers(const std::unordered_map<std::string, std::string>& request_headers,
                                          const std::string& vary_header) const {
    std::istringstream iss(vary_header);
    std::string header_name;
    std::string result;
    
    while (std::getline(iss, header_name, ',')) {
        // Trim whitespace
        header_name.erase(0, header_name.find_first_not_of(" \t"));
        header_name.erase(header_name.find_last_not_of(" \t") + 1);
        
        // Convert to lowercase for case-insensitive lookup
        std::transform(header_name.begin(), header_name.end(), header_name.begin(), ::tolower);
        
        auto it = request_headers.find(header_name);
        if (it != request_headers.end()) {
            if (!result.empty()) {
                result += ";";
            }
            result += header_name + "=" + it->second;
        }
    }
    
    return result;
}

// HttpCacheManager Implementation
HttpCacheManager& HttpCacheManager::instance() {
    static HttpCacheManager instance;
    return instance;
}

void HttpCacheManager::initialize(const HttpCacheConfig& config) {
    std::lock_guard<std::mutex> lock(init_mutex_);
    
    if (!initialized_) {
        cache_ = std::make_shared<HttpCache>(config);
        initialized_ = true;
        SPDLOG_INFO("HTTP cache manager initialized");
    }
}

void HttpCacheManager::shutdown() {
    std::lock_guard<std::mutex> lock(init_mutex_);
    
    if (initialized_) {
        cache_->clear();
        cache_.reset();
        initialized_ = false;
        SPDLOG_INFO("HTTP cache manager shutdown");
    }
}

bool HttpCacheManager::is_cacheable_method(const std::string& method) {
    static const std::vector<std::string> cacheable = {"GET", "HEAD"};
    return std::find(cacheable.begin(), cacheable.end(), method) != cacheable.end();
}

bool HttpCacheManager::is_cacheable_status(int status_code) {
    static const std::vector<int> cacheable = {200, 203, 300, 301, 302, 404, 410};
    return std::find(cacheable.begin(), cacheable.end(), status_code) != cacheable.end();
}

std::unordered_map<std::string, std::string> HttpCacheManager::parse_http_headers(
    const boost::beast::http::fields& fields) {
    
    std::unordered_map<std::string, std::string> headers;
    
    for (const auto& field : fields) {
        std::string name = std::string(field.name_string());
        std::string value = std::string(field.value());
        
        // Convert header names to lowercase
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        headers[name] = value;
    }
    
    return headers;
}

HttpCacheManager::CacheControlDirectives HttpCacheManager::parse_cache_control(const std::string& cache_control_header) {
    CacheControlDirectives directives;
    
    std::istringstream iss(cache_control_header);
    std::string token;
    
    while (std::getline(iss, token, ',')) {
        // Trim whitespace
        token.erase(0, token.find_first_not_of(" \t"));
        token.erase(token.find_last_not_of(" \t") + 1);
        
        // Convert to lowercase
        std::transform(token.begin(), token.end(), token.begin(), ::tolower);
        
        if (token == "no-cache") {
            directives.no_cache = true;
        } else if (token == "no-store") {
            directives.no_store = true;
        } else if (token == "must-revalidate") {
            directives.must_revalidate = true;
        } else if (token == "private") {
            directives.is_private = true;
        } else if (token == "public") {
            directives.is_public = true;
        } else if (token.starts_with("max-age=")) {
            try {
                int seconds = std::stoi(token.substr(8));
                directives.max_age = std::chrono::seconds(seconds);
            } catch (...) {
                SPDLOG_WARN("Invalid max-age value in Cache-Control: {}", token);
            }
        } else if (token.starts_with("s-maxage=")) {
            try {
                int seconds = std::stoi(token.substr(9));
                directives.s_maxage = std::chrono::seconds(seconds);
            } catch (...) {
                SPDLOG_WARN("Invalid s-maxage value in Cache-Control: {}", token);
            }
        }
    }
    
    return directives;
}

std::chrono::seconds HttpCacheManager::parse_expires_header(const std::string& expires_header) {
    // Simplified Expires header parsing - in production, use proper HTTP date parsing
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    
    // For now, return default TTL - full implementation would parse HTTP date format
    // This is a placeholder for proper RFC 7231 date parsing
    SPDLOG_DEBUG("Expires header parsing not fully implemented: {}", expires_header);
    return std::chrono::seconds(300); // 5 minutes default
}

} // namespace azugate
