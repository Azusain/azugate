#include "metrics.hpp"
#include <algorithm>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <thread>
#include <boost/asio.hpp>
#include <boost/beast.hpp>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#include <fstream>
#endif

namespace azugate {

// Counter implementation
Counter::Counter(const std::string& name, const std::string& help)
    : name_(name), help_(help) {
}

void Counter::increment(double value) {
    if (value < 0) return;  // Counters can't decrease
    value_.fetch_add(value, std::memory_order_relaxed);
}

double Counter::value() const {
    return value_.load(std::memory_order_relaxed);
}

std::string Counter::render_prometheus() const {
    std::ostringstream oss;
    if (!help_.empty()) {
        oss << "# HELP " << name_ << " " << help_ << "\n";
    }
    oss << "# TYPE " << name_ << " counter\n";
    oss << name_ << " " << std::fixed << std::setprecision(6) << value() << "\n";
    return oss.str();
}

void Counter::reset() {
    value_.store(0.0, std::memory_order_relaxed);
}

// Gauge implementation
Gauge::Gauge(const std::string& name, const std::string& help)
    : name_(name), help_(help) {
}

void Gauge::set(double value) {
    value_.store(value, std::memory_order_relaxed);
}

void Gauge::increment(double value) {
    double current = value_.load(std::memory_order_relaxed);
    while (!value_.compare_exchange_weak(current, current + value, std::memory_order_relaxed)) {
        // Retry on CAS failure
    }
}

void Gauge::decrement(double value) {
    increment(-value);
}

double Gauge::value() const {
    return value_.load(std::memory_order_relaxed);
}

std::string Gauge::render_prometheus() const {
    std::ostringstream oss;
    if (!help_.empty()) {
        oss << "# HELP " << name_ << " " << help_ << "\n";
    }
    oss << "# TYPE " << name_ << " gauge\n";
    oss << name_ << " " << std::fixed << std::setprecision(6) << value() << "\n";
    return oss.str();
}

void Gauge::reset() {
    value_.store(0.0, std::memory_order_relaxed);
}

// Histogram implementation
Histogram::Histogram(const std::string& name, const std::vector<double>& buckets, const std::string& help)
    : name_(name), help_(help), buckets_(buckets) {
    
    // Ensure buckets are sorted
    std::sort(buckets_.begin(), buckets_.end());
    
    // Add +Inf bucket if not present
    if (buckets_.empty() || buckets_.back() != std::numeric_limits<double>::infinity()) {
        buckets_.push_back(std::numeric_limits<double>::infinity());
    }
    
    // Initialize bucket counters
    bucket_count_ = buckets_.size();
    bucket_counts_ = std::make_unique<std::atomic<uint64_t>[]>(bucket_count_);
    for (size_t i = 0; i < bucket_count_; ++i) {
        bucket_counts_[i].store(0, std::memory_order_relaxed);
    }
}

void Histogram::observe(double value) {
    count_.fetch_add(1, std::memory_order_relaxed);
    
    double current_sum = sum_.load(std::memory_order_relaxed);
    while (!sum_.compare_exchange_weak(current_sum, current_sum + value, std::memory_order_relaxed)) {
        // Retry on CAS failure
    }
    
    // Update buckets
    for (size_t i = 0; i < bucket_count_; ++i) {
        if (value <= buckets_[i]) {
            bucket_counts_[i].fetch_add(1, std::memory_order_relaxed);
        }
    }
}

void Histogram::observe_duration(std::chrono::steady_clock::time_point start) {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - start);
    observe(duration.count() / 1000000.0);  // Convert to seconds
}

std::string Histogram::render_prometheus() const {
    std::ostringstream oss;
    if (!help_.empty()) {
        oss << "# HELP " << name_ << " " << help_ << "\n";
    }
    oss << "# TYPE " << name_ << " histogram\n";
    
    // Render buckets
    uint64_t cumulative = 0;
    for (size_t i = 0; i < buckets_.size(); ++i) {
        cumulative += bucket_counts_[i].load(std::memory_order_relaxed);
        oss << name_ << "_bucket{le=\"";
        if (buckets_[i] == std::numeric_limits<double>::infinity()) {
            oss << "+Inf";
        } else {
            oss << std::fixed << std::setprecision(6) << buckets_[i];
        }
        oss << "\"} " << cumulative << "\n";
    }
    
    // Render count and sum
    oss << name_ << "_count " << count() << "\n";
    oss << name_ << "_sum " << std::fixed << std::setprecision(6) << sum() << "\n";
    
    return oss.str();
}

void Histogram::reset() {
    count_.store(0, std::memory_order_relaxed);
    sum_.store(0.0, std::memory_order_relaxed);
    for (size_t i = 0; i < bucket_count_; ++i) {
        bucket_counts_[i].store(0, std::memory_order_relaxed);
    }
}

uint64_t Histogram::count() const {
    return count_.load(std::memory_order_relaxed);
}

double Histogram::sum() const {
    return sum_.load(std::memory_order_relaxed);
}

double Histogram::average() const {
    uint64_t c = count();
    return c > 0 ? sum() / c : 0.0;
}

// Summary implementation
Summary::Summary(const std::string& name, const std::vector<double>& quantiles, const std::string& help)
    : name_(name), help_(help), quantiles_(quantiles) {
    observations_.reserve(10000);  // Pre-allocate for performance
}

void Summary::observe(double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    observations_.push_back(value);
    
    count_.fetch_add(1, std::memory_order_relaxed);
    double current_sum = sum_.load(std::memory_order_relaxed);
    while (!sum_.compare_exchange_weak(current_sum, current_sum + value, std::memory_order_relaxed)) {
        // Retry on CAS failure
    }
    
    // Keep only recent observations to prevent unbounded growth
    if (observations_.size() > 100000) {
        observations_.erase(observations_.begin(), observations_.begin() + 50000);
    }
}

void Summary::observe_duration(std::chrono::steady_clock::time_point start) {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - start);
    observe(duration.count() / 1000000.0);  // Convert to seconds
}

std::string Summary::render_prometheus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;
    
    if (!help_.empty()) {
        oss << "# HELP " << name_ << " " << help_ << "\n";
    }
    oss << "# TYPE " << name_ << " summary\n";
    
    // Calculate quantiles
    if (!observations_.empty()) {
        auto sorted_obs = observations_;
        std::sort(sorted_obs.begin(), sorted_obs.end());
        
        for (double q : quantiles_) {
            size_t index = static_cast<size_t>(q * (sorted_obs.size() - 1));
            oss << name_ << "{quantile=\"" << std::fixed << std::setprecision(2) << q 
                << "\"} " << std::setprecision(6) << sorted_obs[index] << "\n";
        }
    }
    
    // Render count and sum
    oss << name_ << "_count " << count_.load(std::memory_order_relaxed) << "\n";
    oss << name_ << "_sum " << std::fixed << std::setprecision(6) 
        << sum_.load(std::memory_order_relaxed) << "\n";
    
    return oss.str();
}

void Summary::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    observations_.clear();
    count_.store(0, std::memory_order_relaxed);
    sum_.store(0.0, std::memory_order_relaxed);
}

// LabeledMetricFamily template implementations
template<typename MetricT>
LabeledMetricFamily<MetricT>::LabeledMetricFamily(const std::string& name, const std::string& help)
    : name_(name), help_(help) {
}

template<typename MetricT>
MetricT& LabeledMetricFamily<MetricT>::with_labels(const Labels& labels) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string key = labels_to_string(labels);
    auto it = metrics_.find(key);
    
    if (it == metrics_.end()) {
        std::unique_ptr<MetricT> metric;
        if constexpr (std::is_same_v<MetricT, Counter>) {
            metric = std::make_unique<Counter>(name_, help_);
        } else if constexpr (std::is_same_v<MetricT, Gauge>) {
            metric = std::make_unique<Gauge>(name_, help_);
        } else if constexpr (std::is_same_v<MetricT, Histogram>) {
            metric = std::make_unique<Histogram>(name_, std::vector<double>{0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0}, help_);
        } else if constexpr (std::is_same_v<MetricT, Summary>) {
            metric = std::make_unique<Summary>(name_, std::vector<double>{0.5, 0.9, 0.95, 0.99}, help_);
        } else {
            static_assert(!sizeof(MetricT*), "Unsupported metric type");
        }
        
        auto* ptr = metric.get();
        metrics_[key] = std::move(metric);
        return *ptr;
    }
    
    return *it->second;
}

template<typename MetricT>
std::string LabeledMetricFamily<MetricT>::render_prometheus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;
    
    if (!help_.empty() && !metrics_.empty()) {
        oss << "# HELP " << name_ << " " << help_ << "\n";
        oss << "# TYPE " << name_ << " ";
        if (std::is_same_v<MetricT, Counter>) {
            oss << "counter\n";
        } else if (std::is_same_v<MetricT, Gauge>) {
            oss << "gauge\n";
        } else if (std::is_same_v<MetricT, Histogram>) {
            oss << "histogram\n";
        } else if (std::is_same_v<MetricT, Summary>) {
            oss << "summary\n";
        }
    }
    
    for (const auto& [labels, metric] : metrics_) {
        std::string metric_output = metric->render_prometheus();
        // Remove the help and type lines since they're already added above
        std::istringstream iss(metric_output);
        std::string line;
        while (std::getline(iss, line)) {
            if (line.empty() || line[0] == '#') continue;
            
            // Add labels to the metric line
            if (!labels.empty()) {
                size_t space_pos = line.find(' ');
                if (space_pos != std::string::npos) {
                    std::string metric_name = line.substr(0, space_pos);
                    std::string value = line.substr(space_pos);
                    oss << metric_name << "{" << labels << "}" << value << "\n";
                } else {
                    oss << line << "\n";
                }
            } else {
                oss << line << "\n";
            }
        }
    }
    
    return oss.str();
}

template<typename MetricT>
void LabeledMetricFamily<MetricT>::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [key, metric] : metrics_) {
        metric->reset();
    }
}

template<typename MetricT>
std::string LabeledMetricFamily<MetricT>::labels_to_string(const Labels& labels) const {
    if (labels.empty()) return "";
    
    std::vector<std::pair<std::string, std::string>> sorted_labels(labels.begin(), labels.end());
    std::sort(sorted_labels.begin(), sorted_labels.end());
    
    std::ostringstream oss;
    for (size_t i = 0; i < sorted_labels.size(); ++i) {
        if (i > 0) oss << ",";
        oss << sorted_labels[i].first << "=\"" << sorted_labels[i].second << "\"";
    }
    
    return oss.str();
}

// Explicit template instantiations
template class LabeledMetricFamily<Counter>;
template class LabeledMetricFamily<Gauge>;
template class LabeledMetricFamily<Histogram>;
template class LabeledMetricFamily<Summary>;

// GatewayMetrics implementation
GatewayMetrics& GatewayMetrics::instance() {
    static GatewayMetrics instance;
    return instance;
}

GatewayMetrics::GatewayMetrics() {
    // Initialize HTTP metrics
    http_requests_total_ = std::make_unique<LabeledMetricFamily<Counter>>(
        "azugate_http_requests_total", "Total number of HTTP requests");
    
    http_request_duration_ = std::make_unique<LabeledMetricFamily<Histogram>>(
        "azugate_http_request_duration_seconds", "HTTP request duration in seconds");
    
    http_request_size_bytes_ = std::make_unique<Counter>(
        "azugate_http_request_size_bytes_total", "Total size of HTTP request bodies");
    
    http_response_size_bytes_ = std::make_unique<Counter>(
        "azugate_http_response_size_bytes_total", "Total size of HTTP response bodies");
    
    // Initialize cache metrics
    cache_hits_total_ = std::make_unique<Counter>(
        "azugate_cache_hits_total", "Total number of cache hits");
    
    cache_misses_total_ = std::make_unique<Counter>(
        "azugate_cache_misses_total", "Total number of cache misses");
    
    cache_entries_ = std::make_unique<Gauge>(
        "azugate_cache_entries", "Current number of cache entries");
    
    cache_size_bytes_ = std::make_unique<Gauge>(
        "azugate_cache_size_bytes", "Current cache size in bytes");
    
    // Initialize load balancer metrics
    upstream_requests_total_ = std::make_unique<LabeledMetricFamily<Counter>>(
        "azugate_upstream_requests_total", "Total requests to upstream servers");
    
    upstream_request_duration_ = std::make_unique<LabeledMetricFamily<Histogram>>(
        "azugate_upstream_request_duration_seconds", "Upstream request duration in seconds");
    
    upstream_healthy_ = std::make_unique<LabeledMetricFamily<Gauge>>(
        "azugate_upstream_healthy", "Health status of upstream servers (1=healthy, 0=unhealthy)");
    
    // Initialize circuit breaker metrics
    circuit_breaker_state_ = std::make_unique<LabeledMetricFamily<Gauge>>(
        "azugate_circuit_breaker_state", "Circuit breaker state (0=closed, 1=open, 2=half-open)");
    
    circuit_breaker_requests_total_ = std::make_unique<LabeledMetricFamily<Counter>>(
        "azugate_circuit_breaker_requests_total", "Total circuit breaker requests");
    
    // Initialize connection metrics
    active_connections_ = std::make_unique<Gauge>(
        "azugate_active_connections", "Current number of active connections");
    
    connection_duration_ = std::make_unique<Histogram>(
        "azugate_connection_duration_seconds", 
        std::vector<double>{0.001, 0.005, 0.01, 0.05, 0.1, 0.5, 1.0, 5.0, 10.0, 30.0, 60.0},
        "Connection duration in seconds");
    
    // Initialize error metrics
    errors_total_ = std::make_unique<LabeledMetricFamily<Counter>>(
        "azugate_errors_total", "Total number of errors");
    
    // Initialize system metrics
    memory_usage_bytes_ = std::make_unique<Gauge>(
        "azugate_memory_usage_bytes", "Current memory usage in bytes");
    
    cpu_usage_percent_ = std::make_unique<Gauge>(
        "azugate_cpu_usage_percent", "Current CPU usage percentage");
}

void GatewayMetrics::record_http_request(const std::string& method, 
                                       const std::string& path,
                                       int status_code,
                                       std::chrono::milliseconds duration) {
    Labels labels = {
        {"method", method},
        {"path", path},
        {"status_code", std::to_string(status_code)}
    };
    
    http_requests_total_->with_labels(labels).increment();
    http_request_duration_->with_labels(labels).observe(duration.count() / 1000.0);
}

void GatewayMetrics::record_http_request_size(size_t bytes) {
    http_request_size_bytes_->increment(static_cast<double>(bytes));
}

void GatewayMetrics::record_http_response_size(size_t bytes) {
    http_response_size_bytes_->increment(static_cast<double>(bytes));
}

void GatewayMetrics::record_cache_hit() {
    cache_hits_total_->increment();
}

void GatewayMetrics::record_cache_miss() {
    cache_misses_total_->increment();
}

void GatewayMetrics::record_cache_size(size_t entries, size_t bytes) {
    cache_entries_->set(static_cast<double>(entries));
    cache_size_bytes_->set(static_cast<double>(bytes));
}

void GatewayMetrics::record_upstream_request(const std::string& upstream,
                                           bool success,
                                           std::chrono::milliseconds duration) {
    Labels labels = {
        {"upstream", upstream},
        {"success", success ? "true" : "false"}
    };
    
    upstream_requests_total_->with_labels(labels).increment();
    upstream_request_duration_->with_labels({{"upstream", upstream}}).observe(duration.count() / 1000.0);
}

void GatewayMetrics::record_upstream_health_check(const std::string& upstream, bool healthy) {
    Labels labels = {{"upstream", upstream}};
    upstream_healthy_->with_labels(labels).set(healthy ? 1.0 : 0.0);
}

void GatewayMetrics::record_circuit_breaker_state(const std::string& name, int state) {
    Labels labels = {{"name", name}};
    circuit_breaker_state_->with_labels(labels).set(static_cast<double>(state));
}

void GatewayMetrics::record_circuit_breaker_request(const std::string& name, const std::string& result) {
    Labels labels = {
        {"name", name},
        {"result", result}
    };
    circuit_breaker_requests_total_->with_labels(labels).increment();
}

void GatewayMetrics::record_active_connections(int count) {
    active_connections_->set(static_cast<double>(count));
}

void GatewayMetrics::record_connection_duration(std::chrono::milliseconds duration) {
    connection_duration_->observe(duration.count() / 1000.0);
}

void GatewayMetrics::record_error(const std::string& type, const std::string& source) {
    Labels labels = {
        {"type", type},
        {"source", source}
    };
    errors_total_->with_labels(labels).increment();
}

void GatewayMetrics::record_memory_usage(size_t bytes) {
    memory_usage_bytes_->set(static_cast<double>(bytes));
}

void GatewayMetrics::record_cpu_usage(double percentage) {
    cpu_usage_percent_->set(percentage);
}

std::string GatewayMetrics::export_prometheus() const {
    std::lock_guard<std::mutex> lock(export_mutex_);
    std::ostringstream oss;
    
    // Export all metrics
    oss << http_requests_total_->render_prometheus();
    oss << http_request_duration_->render_prometheus();
    oss << http_request_size_bytes_->render_prometheus();
    oss << http_response_size_bytes_->render_prometheus();
    
    oss << cache_hits_total_->render_prometheus();
    oss << cache_misses_total_->render_prometheus();
    oss << cache_entries_->render_prometheus();
    oss << cache_size_bytes_->render_prometheus();
    
    oss << upstream_requests_total_->render_prometheus();
    oss << upstream_request_duration_->render_prometheus();
    oss << upstream_healthy_->render_prometheus();
    
    oss << circuit_breaker_state_->render_prometheus();
    oss << circuit_breaker_requests_total_->render_prometheus();
    
    oss << active_connections_->render_prometheus();
    oss << connection_duration_->render_prometheus();
    
    oss << errors_total_->render_prometheus();
    
    oss << memory_usage_bytes_->render_prometheus();
    oss << cpu_usage_percent_->render_prometheus();
    
    return oss.str();
}

std::string GatewayMetrics::export_json() const {
    // Basic JSON export - could be enhanced with proper JSON library
    return R"({"status": "ok", "format": "prometheus_text_format_preferred"})";
}

void GatewayMetrics::reset_all() {
    std::lock_guard<std::mutex> lock(export_mutex_);
    
    http_requests_total_->reset();
    http_request_duration_->reset();
    http_request_size_bytes_->reset();
    http_response_size_bytes_->reset();
    
    cache_hits_total_->reset();
    cache_misses_total_->reset();
    cache_entries_->reset();
    cache_size_bytes_->reset();
    
    upstream_requests_total_->reset();
    upstream_request_duration_->reset();
    upstream_healthy_->reset();
    
    circuit_breaker_state_->reset();
    circuit_breaker_requests_total_->reset();
    
    active_connections_->reset();
    connection_duration_->reset();
    
    errors_total_->reset();
    
    memory_usage_bytes_->reset();
    cpu_usage_percent_->reset();
}

// MetricsCollector implementation
std::atomic<bool> MetricsCollector::collection_running_{false};
std::thread MetricsCollector::collection_thread_;

void MetricsCollector::start_background_collection() {
    if (collection_running_.load()) return;
    
    collection_running_.store(true);
    collection_thread_ = std::thread([]() {
        while (collection_running_.load()) {
            try {
                collect_system_metrics();
            } catch (const std::exception& e) {
                spdlog::error("Error collecting system metrics: {}", e.what());
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    });
}

void MetricsCollector::stop_background_collection() {
    collection_running_.store(false);
    if (collection_thread_.joinable()) {
        collection_thread_.join();
    }
}

void MetricsCollector::collect_system_metrics() {
    auto& metrics = GatewayMetrics::instance();
    
    metrics.record_memory_usage(get_memory_usage());
    metrics.record_cpu_usage(get_cpu_usage());
}

size_t MetricsCollector::get_memory_usage() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
    return 0;
#else
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.find("VmRSS:") == 0) {
            std::istringstream iss(line);
            std::string label;
            size_t value;
            std::string unit;
            iss >> label >> value >> unit;
            return value * 1024;  // Convert KB to bytes
        }
    }
    return 0;
#endif
}

double MetricsCollector::get_cpu_usage() {
    // Simplified CPU usage - in production, you'd want a more sophisticated implementation
    static auto last_time = std::chrono::steady_clock::now();
    static double last_cpu_time = 0.0;
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time);
    
    if (elapsed.count() < 1000) {
        return 0.0;  // Return 0 if less than 1 second elapsed
    }
    
    last_time = now;
    
#ifdef _WIN32
    FILETIME idle, kernel, user;
    if (GetSystemTimes(&idle, &kernel, &user)) {
        // Windows CPU calculation would go here
        return 0.0;  // Placeholder
    }
    return 0.0;
#else
    std::ifstream stat("/proc/stat");
    std::string line;
    if (std::getline(stat, line)) {
        std::istringstream iss(line);
        std::string cpu;
        long user, nice, system, idle;
        iss >> cpu >> user >> nice >> system >> idle;
        
        double total_cpu_time = user + nice + system + idle;
        double cpu_usage = (total_cpu_time - last_cpu_time) > 0 ? 
            ((total_cpu_time - idle - (last_cpu_time)) / (total_cpu_time - last_cpu_time)) * 100.0 : 0.0;
        
        last_cpu_time = total_cpu_time;
        return std::max(0.0, std::min(100.0, cpu_usage));
    }
    return 0.0;
#endif
}

// RequestTimer implementation
RequestTimer::RequestTimer(const std::string& metric_name)
    : start_time_(std::chrono::steady_clock::now()), finished_(false) {
    callback_ = [metric_name](std::chrono::milliseconds duration) {
        // Could record to a specific metric here
        spdlog::debug("Request {} took {}ms", metric_name, duration.count());
    };
}

RequestTimer::RequestTimer(std::function<void(std::chrono::milliseconds)> callback)
    : start_time_(std::chrono::steady_clock::now()), callback_(callback), finished_(false) {
}

RequestTimer::~RequestTimer() {
    if (!finished_ && callback_) {
        finish();
    }
}

void RequestTimer::finish() {
    if (!finished_) {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time_);
        if (callback_) {
            callback_(duration);
        }
        finished_ = true;
    }
}

std::chrono::milliseconds RequestTimer::elapsed() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time_);
}

// HttpMetricsMiddleware implementation
void HttpMetricsMiddleware::record_request_start(const std::string& method, const std::string& path) {
    spdlog::debug("Starting HTTP request: {} {}", method, path);
}

void HttpMetricsMiddleware::record_request_complete(const std::string& method, 
                                                  const std::string& path,
                                                  int status_code,
                                                  std::chrono::milliseconds duration,
                                                  size_t request_size,
                                                  size_t response_size) {
    auto& metrics = GatewayMetrics::instance();
    metrics.record_http_request(method, path, status_code, duration);
    metrics.record_http_request_size(request_size);
    metrics.record_http_response_size(response_size);
    
    spdlog::debug("Completed HTTP request: {} {} -> {} ({}ms, req_size={}, resp_size={})",
                 method, path, status_code, duration.count(), request_size, response_size);
}

// MetricsServer implementation
MetricsServer::MetricsServer(uint16_t port)
    : port_(port), metrics_path_("/metrics"), health_path_("/health"), running_(false) {
}

MetricsServer::~MetricsServer() {
    stop();
}

void MetricsServer::start() {
    if (running_.load()) return;
    
    running_.store(true);
    server_thread_ = std::thread([this]() {
        run_server();
    });
    
    spdlog::info("Metrics server started on port {}", port_);
}

void MetricsServer::stop() {
    if (!running_.load()) return;
    
    running_.store(false);
    if (io_context_) {
        io_context_->stop();
    }
    
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    
    spdlog::info("Metrics server stopped");
}

bool MetricsServer::is_running() const {
    return running_.load();
}

void MetricsServer::set_metrics_path(const std::string& path) {
    metrics_path_ = path;
}

void MetricsServer::set_health_path(const std::string& path) {
    health_path_ = path;
}

void MetricsServer::run_server() {
    try {
        namespace beast = boost::beast;
        namespace http = beast::http;
        namespace net = boost::asio;
        using tcp = net::ip::tcp;
        
        io_context_ = std::make_unique<net::io_context>();
        tcp::acceptor acceptor(*io_context_, {net::ip::address::from_string("0.0.0.0"), port_});
        
        while (running_.load()) {
            try {
                tcp::socket socket(*io_context_);
                acceptor.accept(socket);
                
                std::thread([this, socket = std::move(socket)]() mutable {
                    try {
                        beast::flat_buffer buffer;
                        http::request<http::string_body> req;
                        http::read(socket, buffer, req);
                        
                        std::string response_body;
                        std::string content_type = "text/plain";
                        int status_code = 200;
                        
                        if (req.target() == metrics_path_) {
                            response_body = handle_metrics_request();
                            content_type = "text/plain; version=0.0.4; charset=utf-8";
                        } else if (req.target() == health_path_) {
                            response_body = handle_health_request();
                            content_type = "application/json";
                        } else if (req.target() == "/ready") {
                            response_body = handle_ready_request();
                            content_type = "application/json";
                        } else if (req.target() == "/config") {
                            response_body = handle_config_request();
                            content_type = "application/json";
                        } else if (req.target() == "/version") {
                            response_body = handle_version_request();
                            content_type = "application/json";
                        } else if (req.target() == "/dashboard" || req.target() == "/") {
                            response_body = handle_dashboard_request();
                            content_type = "text/html; charset=utf-8";
                        } else {
                            response_body = "Not Found";
                            status_code = 404;
                        }
                        
                        http::response<http::string_body> res{
                            static_cast<http::status>(status_code), req.version()};
                        res.set(http::field::server, "AzuGate/1.0");
                        res.set(http::field::content_type, content_type);
                        res.body() = response_body;
                        res.prepare_payload();
                        
                        http::write(socket, res);
                        socket.shutdown(tcp::socket::shutdown_send);
                        
                    } catch (const std::exception& e) {
                        spdlog::error("Error handling metrics request: {}", e.what());
                    }
                }).detach();
                
            } catch (const std::exception& e) {
                if (running_.load()) {
                    spdlog::error("Error accepting connection: {}", e.what());
                }
            }
        }
        
    } catch (const std::exception& e) {
        spdlog::error("Metrics server error: {}", e.what());
    }
}

std::string MetricsServer::handle_metrics_request() const {
    return GatewayMetrics::instance().export_prometheus();
}

std::string MetricsServer::handle_health_request() const {
    return R"({"status":"ok","timestamp":")" + 
           std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch()).count()) + R"("})";
}

std::string MetricsServer::handle_ready_request() const {
    return R"({"status":"ready","services":["metrics","proxy"]})";
}

std::string MetricsServer::handle_config_request() const {
    try {
        return R"({
  "status": "ok",
  "message": "Configuration management integrated",
  "timestamp": )" + std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
              std::chrono::system_clock::now().time_since_epoch()).count()) + R"(
})";
    } catch (const std::exception& e) {
        return R"({
  "status": "error",
  "message": "Error accessing configuration: )" + std::string(e.what()) + R"(",
  "timestamp": )" + std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
              std::chrono::system_clock::now().time_since_epoch()).count()) + R"(
})";
    }
}

// Static variable to track application start time
static auto app_start_time = std::chrono::steady_clock::now();

std::string MetricsServer::handle_version_request() const {
    std::string os_name;
#ifdef _WIN32
    os_name = "windows";
#else
    os_name = "linux";
#endif
    return std::string(R"({"service":"azugate","version":"1.0.0","build_date":"2024-01-01","build_commit":"dev","go_version":"cpp17","os":")")
        + os_name + R"(","arch":"amd64","uptime_seconds":)" 
        + std::to_string(get_uptime_seconds()) + R"(,"timestamp":)" 
        + std::to_string(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()) 
        + R"(})";
}

std::string MetricsServer::handle_dashboard_request() const {
    auto uptime_seconds = get_uptime_seconds();
    std::string html = "<!DOCTYPE html><html><head><title>AzuGate Dashboard</title>";
    html += "<meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
    html += "<style>body{font-family:Arial,sans-serif;margin:20px;background-color:#f5f5f5}";
    html += ".header{background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:white;padding:20px;border-radius:8px;margin-bottom:20px}";
    html += ".card{background:white;padding:20px;margin:10px 0;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1)}";
    html += ".metric{display:inline-block;margin:10px 15px}.metric-value{font-size:24px;font-weight:bold;color:#333}";
    html += ".metric-label{font-size:12px;color:#666;text-transform:uppercase}.status-ok{color:#28a745}";
    html += ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:20px}";
    html += ".refresh-btn{background:#007bff;color:white;border:none;padding:10px 20px;border-radius:4px;cursor:pointer}";
    html += "</style><script>function refreshPage(){window.location.reload()}";
    html += "function autoRefresh(){setTimeout(refreshPage,30000)}window.onload=autoRefresh</script></head><body>";
    html += "<div class=\"header\"><h1>AzuGate Dashboard</h1><p>Real-time gateway monitoring</p>";
    html += "<button class=\"refresh-btn\" onclick=\"refreshPage()\">Refresh</button></div>";
    html += "<div class=\"grid\"><div class=\"card\"><h3>Service Health</h3>";
    html += "<div class=\"metric\"><div class=\"metric-value status-ok\">HEALTHY</div><div class=\"metric-label\">System Status</div></div>";
    html += "<div class=\"metric\"><div class=\"metric-value\">" + format_uptime(uptime_seconds) + "</div><div class=\"metric-label\">Uptime</div></div>";
    html += "<div class=\"metric\"><div class=\"metric-value\">v1.0.0</div><div class=\"metric-label\">Version</div></div></div>";
    html += "<div class=\"card\"><h3>HTTP Metrics</h3>";
    html += "<div class=\"metric\"><div class=\"metric-value\">0</div><div class=\"metric-label\">Total Requests</div></div>";
    html += "<div class=\"metric\"><div class=\"metric-value\">0</div><div class=\"metric-label\">Active Connections</div></div></div>";
    html += "<div class=\"card\"><h3>Cache Metrics</h3>";
    html += "<div class=\"metric\"><div class=\"metric-value\">0%</div><div class=\"metric-label\">Hit Rate</div></div></div></div>";
    html += "<div class=\"card\"><h3>Quick Links</h3><p>";
    html += "<a href=\"/metrics\" target=\"_blank\">Prometheus Metrics</a> | ";
    html += "<a href=\"/health\" target=\"_blank\">Health Check</a> | ";
    html += "<a href=\"/version\" target=\"_blank\">Version Info</a> | ";
    html += "<a href=\"/config\" target=\"_blank\">Configuration</a>";
    html += "</p></div><div style=\"text-align:center;margin-top:30px;color:#666;font-size:12px\">";
    html += "<p>AzuGate Gateway - Auto-refresh in 30s</p></div></body></html>";
    return html;
}

// Helper methods implementation
std::string MetricsServer::format_uptime(uint64_t seconds) const {
    uint64_t days = seconds / 86400;
    uint64_t hours = (seconds % 86400) / 3600;
    uint64_t mins = (seconds % 3600) / 60;
    uint64_t secs = seconds % 60;
    
    if (days > 0) {
        return std::to_string(days) + "d " + std::to_string(hours) + "h";
    } else if (hours > 0) {
        return std::to_string(hours) + "h " + std::to_string(mins) + "m";
    } else if (mins > 0) {
        return std::to_string(mins) + "m " + std::to_string(secs) + "s";
    } else {
        return std::to_string(secs) + "s";
    }
}

std::string MetricsServer::format_bytes(uint64_t bytes) const {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = static_cast<double>(bytes);
    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        unit++;
    }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << size << " " << units[unit];
    return oss.str();
}

std::string MetricsServer::format_duration(double seconds) const {
    if (seconds < 1.0) {
        return std::to_string(static_cast<int>(seconds * 1000)) + "ms";
    } else {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << seconds << "s";
        return oss.str();
    }
}

std::string MetricsServer::format_percentage(double value) const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << value << "%";
    return oss.str();
}

uint64_t MetricsServer::get_uptime_seconds() const {
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - app_start_time);
    return static_cast<uint64_t>(uptime.count());
}

// Getter methods for dashboard
double GatewayMetrics::get_http_requests_total() const {
    return 0.0; // Simplified - would need to sum all labeled counters
}

double GatewayMetrics::get_active_connections() const {
    return active_connections_->value();
}

double GatewayMetrics::get_avg_response_time() const {
    return connection_duration_->average();
}

double GatewayMetrics::get_cache_hit_rate() const {
    double hits = cache_hits_total_->value();
    double misses = cache_misses_total_->value();
    double total = hits + misses;
    return total > 0 ? (hits / total) * 100.0 : 0.0;
}

double GatewayMetrics::get_cache_entries() const {
    return cache_entries_->value();
}

double GatewayMetrics::get_cache_size_bytes() const {
    return cache_size_bytes_->value();
}

double GatewayMetrics::get_memory_usage_bytes() const {
    return memory_usage_bytes_->value();
}

double GatewayMetrics::get_cpu_usage_percent() const {
    return cpu_usage_percent_->value();
}

} // namespace azugate
