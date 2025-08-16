#ifndef __METRICS_HPP
#define __METRICS_HPP

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <thread>
#include <spdlog/spdlog.h>
#include <boost/asio.hpp>
#include <boost/beast.hpp>

namespace azugate {

// Forward declarations
class MetricsRegistry;

// Metric types following Prometheus standards
enum class MetricType {
    COUNTER,     // Monotonically increasing counter
    GAUGE,       // Current value that can go up or down
    HISTOGRAM,   // Distribution of values with buckets
    SUMMARY      // Quantiles over sliding time windows
};

// Labels for metric dimensions
using Labels = std::unordered_map<std::string, std::string>;

// Base metric interface
class Metric {
public:
    virtual ~Metric() = default;
    virtual std::string name() const = 0;
    virtual MetricType type() const = 0;
    virtual std::string render_prometheus() const = 0;
    virtual void reset() = 0;
};

// Counter metric - monotonically increasing
class Counter : public Metric {
public:
    explicit Counter(const std::string& name, const std::string& help = "");
    
    void increment(double value = 1.0);
    double value() const;
    
    std::string name() const override { return name_; }
    MetricType type() const override { return MetricType::COUNTER; }
    std::string render_prometheus() const override;
    void reset() override;

private:
    std::string name_;
    std::string help_;
    std::atomic<double> value_{0.0};
};

// Gauge metric - can go up or down
class Gauge : public Metric {
public:
    explicit Gauge(const std::string& name, const std::string& help = "");
    
    void set(double value);
    void increment(double value = 1.0);
    void decrement(double value = 1.0);
    double value() const;
    
    std::string name() const override { return name_; }
    MetricType type() const override { return MetricType::GAUGE; }
    std::string render_prometheus() const override;
    void reset() override;

private:
    std::string name_;
    std::string help_;
    std::atomic<double> value_{0.0};
};

// Histogram metric - tracks distribution of values
class Histogram : public Metric {
public:
    explicit Histogram(const std::string& name, 
                      const std::vector<double>& buckets = {0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0},
                      const std::string& help = "");
    
    void observe(double value);
    void observe_duration(std::chrono::steady_clock::time_point start);
    
    std::string name() const override { return name_; }
    MetricType type() const override { return MetricType::HISTOGRAM; }
    std::string render_prometheus() const override;
    void reset() override;
    
    // Statistics
    uint64_t count() const;
    double sum() const;
    double average() const;

private:
    std::string name_;
    std::string help_;
    std::vector<double> buckets_;
    std::unique_ptr<std::atomic<uint64_t>[]> bucket_counts_;
    size_t bucket_count_;
    std::atomic<uint64_t> count_{0};
    std::atomic<double> sum_{0.0};
    mutable std::mutex mutex_;
};

// Summary metric - tracks quantiles
class Summary : public Metric {
public:
    explicit Summary(const std::string& name, 
                    const std::vector<double>& quantiles = {0.5, 0.9, 0.95, 0.99},
                    const std::string& help = "");
    
    void observe(double value);
    void observe_duration(std::chrono::steady_clock::time_point start);
    
    std::string name() const override { return name_; }
    MetricType type() const override { return MetricType::SUMMARY; }
    std::string render_prometheus() const override;
    void reset() override;

private:
    std::string name_;
    std::string help_;
    std::vector<double> quantiles_;
    std::vector<double> observations_;
    std::atomic<uint64_t> count_{0};
    std::atomic<double> sum_{0.0};
    mutable std::mutex mutex_;
};

// Labeled metric family - supports multiple time series with labels
template<typename MetricT>
class LabeledMetricFamily {
public:
    explicit LabeledMetricFamily(const std::string& name, const std::string& help = "");
    
    MetricT& with_labels(const Labels& labels);
    std::string render_prometheus() const;
    void reset();

private:
    std::string name_;
    std::string help_;
    std::unordered_map<std::string, std::unique_ptr<MetricT>> metrics_;
    mutable std::mutex mutex_;
    
    std::string labels_to_string(const Labels& labels) const;
};

// Gateway-specific metrics collector
class GatewayMetrics {
public:
    static GatewayMetrics& instance();
    
    // HTTP request metrics
    void record_http_request(const std::string& method, 
                           const std::string& path,
                           int status_code,
                           std::chrono::milliseconds duration);
    
    void record_http_request_size(size_t bytes);
    void record_http_response_size(size_t bytes);
    
    // Cache metrics
    void record_cache_hit();
    void record_cache_miss();
    void record_cache_size(size_t entries, size_t bytes);
    
    // Load balancer metrics
    void record_upstream_request(const std::string& upstream,
                                bool success,
                                std::chrono::milliseconds duration);
    
    void record_upstream_health_check(const std::string& upstream, bool healthy);
    
    // Circuit breaker metrics
    void record_circuit_breaker_state(const std::string& name, int state);
    void record_circuit_breaker_request(const std::string& name, const std::string& result);
    
    // Connection metrics
    void record_active_connections(int count);
    void record_connection_duration(std::chrono::milliseconds duration);
    
    // Error metrics
    void record_error(const std::string& type, const std::string& source);
    
    // System metrics
    void record_memory_usage(size_t bytes);
    void record_cpu_usage(double percentage);
    
    // Export metrics
    std::string export_prometheus() const;
    std::string export_json() const;
    void reset_all();
    
    // Getter methods for dashboard
    double get_http_requests_total() const;
    double get_active_connections() const;
    double get_avg_response_time() const;
    double get_cache_hit_rate() const;
    double get_cache_entries() const;
    double get_cache_size_bytes() const;
    double get_memory_usage_bytes() const;
    double get_cpu_usage_percent() const;

private:
    GatewayMetrics();
    
    // HTTP metrics
    std::unique_ptr<LabeledMetricFamily<Counter>> http_requests_total_;
    std::unique_ptr<LabeledMetricFamily<Histogram>> http_request_duration_;
    std::unique_ptr<Counter> http_request_size_bytes_;
    std::unique_ptr<Counter> http_response_size_bytes_;
    
    // Cache metrics
    std::unique_ptr<Counter> cache_hits_total_;
    std::unique_ptr<Counter> cache_misses_total_;
    std::unique_ptr<Gauge> cache_entries_;
    std::unique_ptr<Gauge> cache_size_bytes_;
    
    // Load balancer metrics
    std::unique_ptr<LabeledMetricFamily<Counter>> upstream_requests_total_;
    std::unique_ptr<LabeledMetricFamily<Histogram>> upstream_request_duration_;
    std::unique_ptr<LabeledMetricFamily<Gauge>> upstream_healthy_;
    
    // Circuit breaker metrics
    std::unique_ptr<LabeledMetricFamily<Gauge>> circuit_breaker_state_;
    std::unique_ptr<LabeledMetricFamily<Counter>> circuit_breaker_requests_total_;
    
    // Connection metrics
    std::unique_ptr<Gauge> active_connections_;
    std::unique_ptr<Histogram> connection_duration_;
    
    // Error metrics
    std::unique_ptr<LabeledMetricFamily<Counter>> errors_total_;
    
    // System metrics
    std::unique_ptr<Gauge> memory_usage_bytes_;
    std::unique_ptr<Gauge> cpu_usage_percent_;
    
    mutable std::mutex export_mutex_;
};

// Metrics collection utilities
class MetricsCollector {
public:
    static void start_background_collection();
    static void stop_background_collection();
    
    // System metrics collection
    static void collect_system_metrics();
    static size_t get_memory_usage();
    static double get_cpu_usage();

private:
    static std::atomic<bool> collection_running_;
    static std::thread collection_thread_;
};

// RAII helper for measuring request durations
class RequestTimer {
public:
    explicit RequestTimer(const std::string& metric_name);
    explicit RequestTimer(std::function<void(std::chrono::milliseconds)> callback);
    ~RequestTimer();
    
    void finish();
    std::chrono::milliseconds elapsed() const;

private:
    std::chrono::steady_clock::time_point start_time_;
    std::function<void(std::chrono::milliseconds)> callback_;
    bool finished_;
};

// HTTP metrics middleware
class HttpMetricsMiddleware {
public:
    static void record_request_start(const std::string& method, const std::string& path);
    static void record_request_complete(const std::string& method, 
                                      const std::string& path,
                                      int status_code,
                                      std::chrono::milliseconds duration,
                                      size_t request_size,
                                      size_t response_size);
};

// Metrics server for exposing /metrics endpoint
class MetricsServer {
public:
    explicit MetricsServer(uint16_t port = 9090);
    ~MetricsServer();
    
    void start();
    void stop();
    bool is_running() const;
    
    void set_metrics_path(const std::string& path);
    void set_health_path(const std::string& path);

private:
    void run_server();
    std::string handle_metrics_request() const;
    std::string handle_health_request() const;
    std::string handle_ready_request() const;
    std::string handle_config_request() const;
    std::string handle_version_request() const;
    std::string handle_dashboard_request() const;
    
    // Helper methods for dashboard
    std::string format_uptime(uint64_t seconds) const;
    std::string format_bytes(uint64_t bytes) const;
    std::string format_duration(double seconds) const;
    std::string format_percentage(double value) const;
    uint64_t get_uptime_seconds() const;
    
    uint16_t port_;
    std::string metrics_path_;
    std::string health_path_;
    std::atomic<bool> running_;
    std::thread server_thread_;
    std::unique_ptr<boost::asio::io_context> io_context_;
};

// Macros for easy metric recording
#define RECORD_HTTP_REQUEST(method, path, status, duration) \
    GatewayMetrics::instance().record_http_request(method, path, status, duration)

#define RECORD_CACHE_HIT() \
    GatewayMetrics::instance().record_cache_hit()

#define RECORD_CACHE_MISS() \
    GatewayMetrics::instance().record_cache_miss()

#define RECORD_UPSTREAM_REQUEST(upstream, success, duration) \
    GatewayMetrics::instance().record_upstream_request(upstream, success, duration)

#define RECORD_ERROR(type, source) \
    GatewayMetrics::instance().record_error(type, source)

#define MEASURE_REQUEST_DURATION(name) \
    RequestTimer _timer(name)

// Configuration for metrics collection
struct MetricsConfig {
    bool enabled = true;
    uint16_t server_port = 9090;
    std::string metrics_path = "/metrics";
    std::string health_path = "/health";
    std::string ready_path = "/ready";
    
    // Collection settings
    std::chrono::seconds collection_interval{10};
    bool collect_system_metrics = true;
    bool collect_go_metrics = false;  // For compatibility
    
    // Retention settings
    std::chrono::minutes metrics_retention{60};
    size_t max_label_combinations = 1000;
};

} // namespace azugate

#endif // __METRICS_HPP
