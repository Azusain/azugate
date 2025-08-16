#include "../../include/circuit_breaker.hpp"
#include <algorithm>
#include <iomanip>
#include <shared_mutex>
#include <sstream>
#include <fmt/format.h>

namespace azugate {

// CircuitBreakerStats implementation
std::string CircuitBreakerStats::to_json() const {
    std::ostringstream oss;
    oss << "{"
        << R"("total_requests":)" << total_requests.load() << ","
        << R"("successful_requests":)" << successful_requests.load() << ","
        << R"("failed_requests":)" << failed_requests.load() << ","
        << R"("rejected_requests":)" << rejected_requests.load() << ","
        << R"("timeout_requests":)" << timeout_requests.load() << ","
        << R"("consecutive_failures":)" << consecutive_failures.load() << ","
        << R"("consecutive_successes":)" << consecutive_successes.load() << ","
        << R"("current_failure_rate":)" << std::fixed << std::setprecision(3) << current_failure_rate()
        << "}";
    return oss.str();
}

// CircuitBreaker implementation
CircuitBreaker::CircuitBreaker(const std::string& name, const CircuitBreakerConfig& config)
    : name_(name), config_(config), last_attempt_time_(std::chrono::steady_clock::now()) {
    
    stats_.last_state_change = std::chrono::steady_clock::now();
    request_history_.reserve(config_.minimum_requests * 2);
    
    SPDLOG_INFO("Circuit breaker '{}' created with failure_threshold={}, recovery_timeout={}ms", 
                name_, config_.failure_threshold, config_.recovery_timeout.count());
}

bool CircuitBreaker::can_proceed() {
    auto current_state = state_.load();
    auto now = std::chrono::steady_clock::now();
    
    switch (current_state) {
        case CircuitBreakerState::CLOSED:
            return true;
            
        case CircuitBreakerState::OPEN: {
            // Check if recovery timeout has elapsed
            auto recovery_timeout = calculate_recovery_timeout();
            if (now - stats_.last_state_change >= recovery_timeout) {
                transition_to_half_open();
                return true;
            }
            return false;
        }
        
        case CircuitBreakerState::HALF_OPEN: {
            // Allow limited requests in half-open state
            uint32_t current_half_open = half_open_requests_.load();
            if (current_half_open < config_.half_open_max_requests) {
                half_open_requests_++;
                return true;
            }
            return false;
        }
    }
    
    return false;
}

void CircuitBreaker::record_success(std::chrono::milliseconds response_time) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    stats_.total_requests++;
    stats_.successful_requests++;
    stats_.consecutive_successes++;
    stats_.consecutive_failures = 0;
    stats_.last_success_time = std::chrono::steady_clock::now();
    
    // Add to rolling window
    {
        std::lock_guard<std::mutex> history_lock(history_mutex_);
        request_history_.push_back({stats_.last_success_time, true});
    }
    
    auto current_state = state_.load();
    
    if (current_state == CircuitBreakerState::HALF_OPEN) {
        // Check if we should close the circuit
        if (should_close_circuit()) {
            transition_to_closed();
        }
    }
    
    // Clean old records periodically
    update_failure_rate();
    
    SPDLOG_DEBUG("Circuit breaker '{}': SUCCESS recorded (response_time={}ms, consecutive_successes={})", 
                name_, response_time.count(), stats_.consecutive_successes.load());
}

void CircuitBreaker::record_failure() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    stats_.total_requests++;
    stats_.failed_requests++;
    stats_.consecutive_failures++;
    stats_.consecutive_successes = 0;
    stats_.last_failure_time = std::chrono::steady_clock::now();
    
    // Add to rolling window
    {
        std::lock_guard<std::mutex> history_lock(history_mutex_);
        request_history_.push_back({stats_.last_failure_time, false});
    }
    
    auto current_state = state_.load();
    
    if (current_state == CircuitBreakerState::CLOSED) {
        // Check if we should open the circuit
        if (should_open_circuit()) {
            transition_to_open();
        }
    } else if (current_state == CircuitBreakerState::HALF_OPEN) {
        // Any failure in half-open state reopens the circuit
        transition_to_open();
    }
    
    update_failure_rate();
    
    SPDLOG_DEBUG("Circuit breaker '{}': FAILURE recorded (consecutive_failures={})", 
                name_, stats_.consecutive_failures.load());
}

void CircuitBreaker::record_timeout() {
    stats_.timeout_requests++;
    record_failure();  // Treat timeouts as failures
    
    SPDLOG_WARN("Circuit breaker '{}': TIMEOUT recorded", name_);
}

void CircuitBreaker::update_config(const CircuitBreakerConfig& new_config) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    config_ = new_config;
    
    SPDLOG_INFO("Circuit breaker '{}' configuration updated", name_);
}

void CircuitBreaker::reset() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    std::lock_guard<std::mutex> history_lock(history_mutex_);
    
    stats_ = CircuitBreakerStats{};
    stats_.last_state_change = std::chrono::steady_clock::now();
    
    request_history_.clear();
    half_open_requests_ = 0;
    current_backoff_count_ = 0;
    
    state_ = CircuitBreakerState::CLOSED;
    
    SPDLOG_INFO("Circuit breaker '{}' reset to CLOSED state", name_);
}

void CircuitBreaker::force_open() {
    transition_to_open();
    SPDLOG_WARN("Circuit breaker '{}' forced to OPEN state", name_);
}

void CircuitBreaker::force_close() {
    transition_to_closed();
    SPDLOG_INFO("Circuit breaker '{}' forced to CLOSED state", name_);
}

void CircuitBreaker::force_half_open() {
    transition_to_half_open();
    SPDLOG_INFO("Circuit breaker '{}' forced to HALF_OPEN state", name_);
}

// Private methods
void CircuitBreaker::transition_to_open() {
    auto old_state = state_.exchange(CircuitBreakerState::OPEN);
    
    if (old_state != CircuitBreakerState::OPEN) {
        stats_.last_state_change = std::chrono::steady_clock::now();
        half_open_requests_ = 0;
        
        if (config_.enable_exponential_backoff) {
            current_backoff_count_++;
        }
        
        if (config_.log_state_changes) {
            SPDLOG_WARN("Circuit breaker '{}' transitioned from {} to OPEN (failures: {}, failure_rate: {:.1f}%)", 
                       name_, static_cast<int>(old_state), 
                       stats_.consecutive_failures.load(), 
                       stats_.current_failure_rate() * 100);
        }
    }
}

void CircuitBreaker::transition_to_half_open() {
    auto old_state = state_.exchange(CircuitBreakerState::HALF_OPEN);
    
    if (old_state != CircuitBreakerState::HALF_OPEN) {
        stats_.last_state_change = std::chrono::steady_clock::now();
        half_open_requests_ = 0;
        
        if (config_.log_state_changes) {
            SPDLOG_INFO("Circuit breaker '{}' transitioned from {} to HALF_OPEN", 
                       name_, static_cast<int>(old_state));
        }
    }
}

void CircuitBreaker::transition_to_closed() {
    auto old_state = state_.exchange(CircuitBreakerState::CLOSED);
    
    if (old_state != CircuitBreakerState::CLOSED) {
        stats_.last_state_change = std::chrono::steady_clock::now();
        half_open_requests_ = 0;
        current_backoff_count_ = 0;  // Reset backoff on successful recovery
        
        if (config_.log_state_changes) {
            SPDLOG_INFO("Circuit breaker '{}' transitioned from {} to CLOSED (consecutive_successes: {})", 
                       name_, static_cast<int>(old_state), 
                       stats_.consecutive_successes.load());
        }
    }
}

void CircuitBreaker::update_failure_rate() {
    std::lock_guard<std::mutex> lock(history_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto cutoff_time = now - config_.metrics_window;
    
    // Remove old records
    request_history_.erase(
        std::remove_if(request_history_.begin(), request_history_.end(),
                      [cutoff_time](const RequestRecord& record) {
                          return record.timestamp < cutoff_time;
                      }),
        request_history_.end());
}

bool CircuitBreaker::should_open_circuit() {
    // Check consecutive failures threshold
    if (stats_.consecutive_failures >= config_.failure_threshold) {
        return true;
    }
    
    // Check failure rate threshold (only if we have minimum requests)
    if (stats_.total_requests >= config_.minimum_requests) {
        std::lock_guard<std::mutex> lock(history_mutex_);
        
        if (request_history_.size() >= config_.minimum_requests) {
            size_t failures = std::count_if(request_history_.begin(), request_history_.end(),
                                          [](const RequestRecord& record) { return !record.success; });
            
            double failure_rate = static_cast<double>(failures) / request_history_.size();
            return failure_rate >= config_.failure_rate_threshold;
        }
    }
    
    return false;
}

bool CircuitBreaker::should_close_circuit() {
    return stats_.consecutive_successes >= config_.success_threshold;
}

std::chrono::milliseconds CircuitBreaker::calculate_recovery_timeout() {
    if (!config_.enable_exponential_backoff) {
        return config_.recovery_timeout;
    }
    
    // Calculate exponential backoff
    double multiplier = std::pow(config_.backoff_multiplier, current_backoff_count_);
    auto backoff_timeout = std::chrono::milliseconds(
        static_cast<long long>(config_.recovery_timeout.count() * multiplier));
    
    return std::min(backoff_timeout, config_.max_recovery_timeout);
}

// CircuitBreakerRegistry implementation
CircuitBreakerRegistry& CircuitBreakerRegistry::instance() {
    static CircuitBreakerRegistry instance;
    return instance;
}

std::shared_ptr<CircuitBreaker> CircuitBreakerRegistry::get_or_create(
    const std::string& name, const CircuitBreakerConfig& config) {
    
    std::shared_lock<std::shared_mutex> read_lock(registry_mutex_);
    
    auto it = breakers_.find(name);
    if (it != breakers_.end()) {
        return it->second;
    }
    
    read_lock.unlock();
    std::unique_lock<std::shared_mutex> write_lock(registry_mutex_);
    
    // Double-check pattern
    it = breakers_.find(name);
    if (it != breakers_.end()) {
        return it->second;
    }
    
    auto breaker = std::make_shared<CircuitBreaker>(name, config);
    breakers_[name] = breaker;
    
    SPDLOG_DEBUG("Created new circuit breaker: {}", name);
    return breaker;
}

std::shared_ptr<CircuitBreaker> CircuitBreakerRegistry::get(const std::string& name) {
    std::shared_lock<std::shared_mutex> lock(registry_mutex_);
    
    auto it = breakers_.find(name);
    return (it != breakers_.end()) ? it->second : nullptr;
}

bool CircuitBreakerRegistry::remove(const std::string& name) {
    std::unique_lock<std::shared_mutex> lock(registry_mutex_);
    
    auto removed_count = breakers_.erase(name);
    if (removed_count > 0) {
        SPDLOG_INFO("Removed circuit breaker: {}", name);
        return true;
    }
    return false;
}

void CircuitBreakerRegistry::clear() {
    std::unique_lock<std::shared_mutex> lock(registry_mutex_);
    
    size_t count = breakers_.size();
    breakers_.clear();
    
    SPDLOG_INFO("Cleared all {} circuit breakers", count);
}

std::vector<std::string> CircuitBreakerRegistry::get_all_names() const {
    std::shared_lock<std::shared_mutex> lock(registry_mutex_);
    
    std::vector<std::string> names;
    names.reserve(breakers_.size());
    
    for (const auto& pair : breakers_) {
        names.push_back(pair.first);
    }
    
    return names;
}

std::unordered_map<std::string, CircuitBreakerStats> CircuitBreakerRegistry::get_all_stats() const {
    std::shared_lock<std::shared_mutex> lock(registry_mutex_);
    
    std::unordered_map<std::string, CircuitBreakerStats> stats;
    
    for (const auto& pair : breakers_) {
        stats[pair.first] = pair.second->get_stats();
    }
    
    return stats;
}

void CircuitBreakerRegistry::set_default_config(const CircuitBreakerConfig& config) {
    std::unique_lock<std::shared_mutex> lock(registry_mutex_);
    default_config_ = config;
}

const CircuitBreakerConfig& CircuitBreakerRegistry::get_default_config() const {
    std::shared_lock<std::shared_mutex> lock(registry_mutex_);
    return default_config_;
}

size_t CircuitBreakerRegistry::count() const {
    std::shared_lock<std::shared_mutex> lock(registry_mutex_);
    return breakers_.size();
}

std::string CircuitBreakerRegistry::get_health_report() const {
    std::shared_lock<std::shared_mutex> lock(registry_mutex_);
    
    std::ostringstream oss;
    oss << "Circuit Breaker Health Report:\n";
    oss << "Total breakers: " << breakers_.size() << "\n";
    
    size_t open_count = 0, half_open_count = 0, closed_count = 0;
    
    for (const auto& pair : breakers_) {
        auto state = pair.second->get_state();
        switch (state) {
            case CircuitBreakerState::OPEN: open_count++; break;
            case CircuitBreakerState::HALF_OPEN: half_open_count++; break;
            case CircuitBreakerState::CLOSED: closed_count++; break;
        }
        
        auto stats = pair.second->get_stats();
        oss << fmt::format("- {}: {} (requests: {}, failures: {}, failure_rate: {:.1f}%)\n",
                          pair.first,
                          static_cast<int>(state) == 0 ? "CLOSED" : 
                          static_cast<int>(state) == 1 ? "OPEN" : "HALF_OPEN",
                          stats.total_requests.load(),
                          stats.failed_requests.load(),
                          stats.current_failure_rate() * 100);
    }
    
    oss << fmt::format("Summary: {} CLOSED, {} HALF_OPEN, {} OPEN\n", 
                      closed_count, half_open_count, open_count);
    
    return oss.str();
}

std::string CircuitBreakerRegistry::export_metrics_json() const {
    std::shared_lock<std::shared_mutex> lock(registry_mutex_);
    
    std::ostringstream oss;
    oss << "{\"circuit_breakers\":{";
    
    bool first = true;
    for (const auto& pair : breakers_) {
        if (!first) oss << ",";
        first = false;
        
        oss << "\"" << pair.first << "\":"
            << "{"
            << "\"state\":" << static_cast<int>(pair.second->get_state()) << ","
            << "\"stats\":" << pair.second->get_stats().to_json()
            << "}";
    }
    
    oss << "}}";
    return oss.str();
}

// HttpCircuitBreaker implementation
HttpCircuitBreaker::HttpCircuitBreaker(const std::string& name, const CircuitBreakerConfig& config) {
    breaker_ = CircuitBreakerRegistry::instance().get_or_create(name, config);
}

CircuitBreakerResult HttpCircuitBreaker::handle_http_response(int status_code, 
                                                             std::chrono::milliseconds response_time) {
    if (!breaker_->can_proceed()) {
        return CircuitBreakerResult::CIRCUIT_OPEN;
    }
    
    // Check if response time exceeded timeout
    if (response_time > breaker_->get_config().timeout) {
        breaker_->record_timeout();
        return CircuitBreakerResult::TIMEOUT;
    }
    
    // Check if status code indicates failure
    const auto& failure_codes = breaker_->get_config().failure_status_codes;
    bool is_failure = std::find(failure_codes.begin(), failure_codes.end(), status_code) != failure_codes.end();
    
    if (is_failure) {
        breaker_->record_failure();
        return CircuitBreakerResult::FAILURE;
    } else {
        breaker_->record_success(response_time);
        return CircuitBreakerResult::SUCCESS;
    }
}

bool HttpCircuitBreaker::should_retry(CircuitBreakerResult result, int attempt_count) {
    switch (result) {
        case CircuitBreakerResult::SUCCESS:
            return false;  // No retry needed
            
        case CircuitBreakerResult::TIMEOUT:
        case CircuitBreakerResult::FAILURE:
            return attempt_count < 3;  // Retry up to 3 times
            
        case CircuitBreakerResult::CIRCUIT_OPEN:
        case CircuitBreakerResult::CIRCUIT_HALF_OPEN_LIMIT:
            return false;  // Don't retry when circuit is protecting
    }
    
    return false;
}

std::string HttpCircuitBreaker::create_breaker_name(const std::string& host, uint16_t port) {
    return fmt::format("http_{}_{}", host, port);
}

std::string HttpCircuitBreaker::create_breaker_name(const std::string& service_name) {
    return fmt::format("service_{}", service_name);
}

// CircuitBreakerGuard implementation
CircuitBreakerGuard::CircuitBreakerGuard(std::shared_ptr<CircuitBreaker> breaker)
    : breaker_(breaker), start_time_(std::chrono::steady_clock::now()), 
      result_recorded_(false), proceeded_(false) {
    
    proceeded_ = breaker_->can_proceed();
}

CircuitBreakerGuard::~CircuitBreakerGuard() {
    if (proceeded_ && !result_recorded_) {
        // Default to success if no explicit result was recorded
        mark_success();
    }
}

void CircuitBreakerGuard::mark_success() {
    if (!proceeded_ || result_recorded_) return;
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time_);
    
    breaker_->record_success(duration);
    result_recorded_ = true;
}

void CircuitBreakerGuard::mark_failure() {
    if (!proceeded_ || result_recorded_) return;
    
    breaker_->record_failure();
    result_recorded_ = true;
}

void CircuitBreakerGuard::mark_timeout() {
    if (!proceeded_ || result_recorded_) return;
    
    breaker_->record_timeout();
    result_recorded_ = true;
}

bool CircuitBreakerGuard::can_proceed() const {
    return proceeded_;
}

// Factory functions
namespace circuit_breaker_factory {

std::shared_ptr<CircuitBreaker> create_for_upstream(const std::string& host, 
                                                   uint16_t port,
                                                   const CircuitBreakerConfig& config) {
    std::string name = fmt::format("upstream_{}_{}", host, port);
    return CircuitBreakerRegistry::instance().get_or_create(name, config);
}

std::shared_ptr<CircuitBreaker> create_for_service(const std::string& service_name,
                                                  const CircuitBreakerConfig& config) {
    std::string name = fmt::format("service_{}", service_name);
    return CircuitBreakerRegistry::instance().get_or_create(name, config);
}

std::shared_ptr<CircuitBreaker> create_for_database(const std::string& db_name,
                                                   const CircuitBreakerConfig& config) {
    CircuitBreakerConfig db_config = config;
    db_config.timeout = std::chrono::milliseconds(10000);  // Longer timeout for DB
    db_config.failure_threshold = 3;                       // Lower threshold for DB
    
    std::string name = fmt::format("database_{}", db_name);
    return CircuitBreakerRegistry::instance().get_or_create(name, db_config);
}

std::shared_ptr<CircuitBreaker> create_for_external_api(const std::string& api_name,
                                                       const CircuitBreakerConfig& config) {
    CircuitBreakerConfig api_config = config;
    api_config.timeout = std::chrono::milliseconds(15000);  // Longer timeout for external APIs
    api_config.recovery_timeout = std::chrono::milliseconds(60000);  // Longer recovery
    
    std::string name = fmt::format("external_api_{}", api_name);
    return CircuitBreakerRegistry::instance().get_or_create(name, api_config);
}

} // namespace circuit_breaker_factory

} // namespace azugate
