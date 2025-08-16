#ifndef __CIRCUIT_BREAKER_HPP
#define __CIRCUIT_BREAKER_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <spdlog/spdlog.h>

namespace azugate {

// Circuit breaker states following the classic pattern
enum class CircuitBreakerState {
    CLOSED = 0,    // Normal operation - requests pass through
    OPEN = 1,      // Failing fast - requests are immediately rejected
    HALF_OPEN = 2  // Testing if service has recovered - limited requests allowed
};

// Configuration for circuit breaker behavior
struct CircuitBreakerConfig {
    // Failure threshold configuration
    uint32_t failure_threshold = 5;              // Number of failures to open circuit
    uint32_t success_threshold = 3;              // Number of successes to close circuit in HALF_OPEN
    double failure_rate_threshold = 0.5;         // Failure rate (0.0-1.0) to open circuit
    uint32_t minimum_requests = 10;              // Minimum requests before considering failure rate
    
    // Timing configuration
    std::chrono::milliseconds timeout{5000};     // Request timeout (failure if exceeded)
    std::chrono::milliseconds recovery_timeout{30000};  // Time to wait before HALF_OPEN
    std::chrono::milliseconds metrics_window{60000};    // Rolling window for metrics
    
    // Half-open state configuration
    uint32_t half_open_max_requests = 5;         // Max requests allowed in HALF_OPEN
    std::chrono::milliseconds half_open_timeout{10000}; // Timeout for half-open requests
    
    // Advanced configuration
    bool enable_exponential_backoff = true;      // Exponential backoff for recovery
    double backoff_multiplier = 2.0;             // Backoff multiplier
    std::chrono::milliseconds max_recovery_timeout{300000}; // Max recovery time (5 minutes)
    
    // Monitoring
    bool log_state_changes = true;               // Log when circuit state changes
    std::vector<int> failure_status_codes = {500, 502, 503, 504}; // HTTP codes considered failures
};

// Statistics for monitoring circuit breaker behavior
struct CircuitBreakerStats {
    std::atomic<uint64_t> total_requests{0};
    std::atomic<uint64_t> successful_requests{0};
    std::atomic<uint64_t> failed_requests{0};
    std::atomic<uint64_t> rejected_requests{0};     // Requests rejected due to open circuit
    std::atomic<uint64_t> timeout_requests{0};
    
    std::atomic<uint32_t> consecutive_failures{0};
    std::atomic<uint32_t> consecutive_successes{0};
    
    std::chrono::steady_clock::time_point last_failure_time;
    std::chrono::steady_clock::time_point last_success_time;
    std::chrono::steady_clock::time_point last_state_change;
    
    double current_failure_rate() const {
        uint64_t total = total_requests.load();
        if (total == 0) return 0.0;
        return static_cast<double>(failed_requests.load()) / total;
    }
    
    std::string to_json() const;
};

// Result of a circuit breaker protected call
enum class CircuitBreakerResult {
    SUCCESS = 0,
    FAILURE = 1,
    TIMEOUT = 2,
    CIRCUIT_OPEN = 3,      // Request rejected because circuit is open
    CIRCUIT_HALF_OPEN_LIMIT = 4  // Request rejected due to half-open limit
};

// Individual circuit breaker instance
class CircuitBreaker {
public:
    explicit CircuitBreaker(const std::string& name, 
                           const CircuitBreakerConfig& config = CircuitBreakerConfig{});
    
    ~CircuitBreaker() = default;
    
    // Main circuit breaker operations
    bool can_proceed();  // Check if request can proceed
    void record_success(std::chrono::milliseconds response_time = std::chrono::milliseconds(0));
    void record_failure();
    void record_timeout();
    
    // Template method for automatic result handling
    template<typename Func>
    auto execute(Func&& func) -> decltype(func()) {
        if (!can_proceed()) {
            stats_.rejected_requests++;
            throw std::runtime_error("Circuit breaker is OPEN - request rejected");
        }
        
        auto start_time = std::chrono::steady_clock::now();
        
        try {
            auto result = func();
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - start_time);
            
            if (duration > config_.timeout) {
                record_timeout();
            } else {
                record_success(duration);
            }
            
            return result;
        } catch (const std::exception& e) {
            record_failure();
            throw;
        }
    }
    
    // State and configuration
    CircuitBreakerState get_state() const { return state_.load(); }
    const std::string& get_name() const { return name_; }
    const CircuitBreakerConfig& get_config() const { return config_; }
    const CircuitBreakerStats& get_stats() const { return stats_; }
    
    // Configuration updates
    void update_config(const CircuitBreakerConfig& new_config);
    void reset();  // Reset all stats and close circuit
    
    // Manual state control (for testing/emergency)
    void force_open();
    void force_close();
    void force_half_open();

private:
    void transition_to_open();
    void transition_to_half_open();
    void transition_to_closed();
    void update_failure_rate();
    bool should_open_circuit();
    bool should_close_circuit();
    std::chrono::milliseconds calculate_recovery_timeout();
    
    std::string name_;
    CircuitBreakerConfig config_;
    std::atomic<CircuitBreakerState> state_{CircuitBreakerState::CLOSED};
    
    mutable std::mutex stats_mutex_;
    CircuitBreakerStats stats_;
    
    std::atomic<uint32_t> half_open_requests_{0};
    std::chrono::steady_clock::time_point last_attempt_time_;
    uint32_t current_backoff_count_{0};
    
    // Rolling window for failure rate calculation
    struct RequestRecord {
        std::chrono::steady_clock::time_point timestamp;
        bool success;
    };
    std::vector<RequestRecord> request_history_;
    mutable std::mutex history_mutex_;
};

// Circuit breaker registry for managing multiple circuit breakers
class CircuitBreakerRegistry {
public:
    static CircuitBreakerRegistry& instance();
    
    // Circuit breaker management
    std::shared_ptr<CircuitBreaker> get_or_create(const std::string& name,
                                                 const CircuitBreakerConfig& config = CircuitBreakerConfig{});
    
    std::shared_ptr<CircuitBreaker> get(const std::string& name);
    
    bool remove(const std::string& name);
    void clear();
    
    // Bulk operations
    std::vector<std::string> get_all_names() const;
    std::unordered_map<std::string, CircuitBreakerStats> get_all_stats() const;
    
    // Global configuration
    void set_default_config(const CircuitBreakerConfig& config);
    const CircuitBreakerConfig& get_default_config() const;
    
    // Monitoring and diagnostics
    size_t count() const;
    std::string get_health_report() const;
    
    // JSON export for monitoring endpoints
    std::string export_metrics_json() const;

private:
    CircuitBreakerRegistry() = default;
    
    mutable std::shared_mutex registry_mutex_;
    std::unordered_map<std::string, std::shared_ptr<CircuitBreaker>> breakers_;
    CircuitBreakerConfig default_config_;
};

// Utility class for HTTP-specific circuit breaking
class HttpCircuitBreaker {
public:
    HttpCircuitBreaker(const std::string& name, const CircuitBreakerConfig& config = CircuitBreakerConfig{});
    
    // HTTP-specific result handling
    CircuitBreakerResult handle_http_response(int status_code, 
                                             std::chrono::milliseconds response_time);
    
    bool should_retry(CircuitBreakerResult result, int attempt_count = 0);
    
    // Integration helpers
    static std::string create_breaker_name(const std::string& host, uint16_t port);
    static std::string create_breaker_name(const std::string& service_name);
    
    std::shared_ptr<CircuitBreaker> get_breaker() { return breaker_; }

private:
    std::shared_ptr<CircuitBreaker> breaker_;
};

// RAII helper for automatic success/failure recording
class CircuitBreakerGuard {
public:
    CircuitBreakerGuard(std::shared_ptr<CircuitBreaker> breaker);
    ~CircuitBreakerGuard();
    
    void mark_success();
    void mark_failure();
    void mark_timeout();
    
    bool can_proceed() const;

private:
    std::shared_ptr<CircuitBreaker> breaker_;
    std::chrono::steady_clock::time_point start_time_;
    bool result_recorded_;
    bool proceeded_;
};

// Factory functions for common use cases
namespace circuit_breaker_factory {
    
// Create circuit breaker for HTTP upstream server
std::shared_ptr<CircuitBreaker> create_for_upstream(const std::string& host, 
                                                   uint16_t port,
                                                   const CircuitBreakerConfig& config = CircuitBreakerConfig{});

// Create circuit breaker for service endpoint
std::shared_ptr<CircuitBreaker> create_for_service(const std::string& service_name,
                                                  const CircuitBreakerConfig& config = CircuitBreakerConfig{});

// Create circuit breaker with database-specific config
std::shared_ptr<CircuitBreaker> create_for_database(const std::string& db_name,
                                                   const CircuitBreakerConfig& config = CircuitBreakerConfig{});

// Create circuit breaker for external API
std::shared_ptr<CircuitBreaker> create_for_external_api(const std::string& api_name,
                                                       const CircuitBreakerConfig& config = CircuitBreakerConfig{});

} // namespace circuit_breaker_factory

// Macros for easy integration
#define CIRCUIT_BREAKER_EXECUTE(breaker_name, func) \
    CircuitBreakerRegistry::instance().get_or_create(breaker_name)->execute([&]() { return func; })

#define CIRCUIT_BREAKER_GUARD(breaker_name) \
    CircuitBreakerGuard _cb_guard(CircuitBreakerRegistry::instance().get_or_create(breaker_name))

} // namespace azugate

#endif // __CIRCUIT_BREAKER_HPP
