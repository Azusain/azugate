#ifndef __LOAD_BALANCER_HPP
#define __LOAD_BALANCER_HPP

#include "config.h"
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast/http.hpp>
#include <chrono>
#include <memory>
#include <mutex>
#include <random>
#include <spdlog/spdlog.h>
#include <unordered_map>
#include <vector>

namespace azugate {

// Health status of an upstream server
enum class HealthStatus {
  Unknown = 0,
  Healthy = 1,
  Unhealthy = 2,
  Recovering = 3  // In recovery state, limited traffic
};

// Load balancing strategies
enum class LoadBalancingStrategy {
  RoundRobin = 0,
  LeastConnections = 1,
  WeightedRoundRobin = 2,
  Random = 3,
  IpHash = 4
};

// Configuration for health checks
struct HealthCheckConfig {
  std::string path = "/health";  // Health check endpoint
  std::chrono::milliseconds interval{5000};  // Check interval
  std::chrono::milliseconds timeout{2000};   // Request timeout
  int healthy_threshold = 2;     // Consecutive successes to mark healthy
  int unhealthy_threshold = 3;   // Consecutive failures to mark unhealthy
  int expected_status = 200;     // Expected HTTP status code
  std::string expected_body = "";  // Expected response body (optional)
};

// Individual upstream server
class UpstreamServer {
public:
  UpstreamServer(const std::string& address, uint16_t port, int weight = 1);
  
  // Getters
  const std::string& address() const { return address_; }
  uint16_t port() const { return port_; }
  int weight() const { return weight_; }
  HealthStatus health_status() const { 
    std::lock_guard<std::mutex> lock(mutex_);
    return health_status_; 
  }
  int active_connections() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_connections_;
  }
  
  // Connection management
  void increment_connections();
  void decrement_connections();
  
  // Health status management
  void set_health_status(HealthStatus status);
  void record_health_check_success();
  void record_health_check_failure();
  
  // Statistics
  std::chrono::steady_clock::time_point last_check_time() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_check_time_;
  }
  double response_time_ms() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return avg_response_time_ms_;
  }
  
  void update_response_time(std::chrono::milliseconds response_time);
  
  bool is_available() const;
  
private:
  std::string address_;
  uint16_t port_;
  int weight_;
  
  mutable std::mutex mutex_;
  HealthStatus health_status_;
  int active_connections_;
  int consecutive_successes_;
  int consecutive_failures_;
  std::chrono::steady_clock::time_point last_check_time_;
  double avg_response_time_ms_;  // Exponential moving average
  int total_checks_;
  int total_successes_;
};

// Health checker for performing active health checks
class HealthChecker {
public:
  HealthChecker(boost::asio::io_context& io_context, 
                const HealthCheckConfig& config);
  
  void start_health_check(std::shared_ptr<UpstreamServer> server);
  void stop_health_checks();
  
private:
  void perform_health_check(std::shared_ptr<UpstreamServer> server);
  void schedule_next_check(std::shared_ptr<UpstreamServer> server);
  
  boost::asio::io_context& io_context_;
  HealthCheckConfig config_;
  std::vector<std::shared_ptr<boost::asio::steady_timer>> timers_;
  bool stopped_;
};

// Main load balancer class
class LoadBalancer {
public:
  LoadBalancer(boost::asio::io_context& io_context,
               LoadBalancingStrategy strategy = LoadBalancingStrategy::RoundRobin);
  
  ~LoadBalancer();
  
  // Server management
  void add_server(const std::string& address, uint16_t port, int weight = 1);
  void remove_server(const std::string& address, uint16_t port);
  
  // Get next server for load balancing
  std::shared_ptr<UpstreamServer> get_server(const std::string& client_ip = "");
  
  // Configuration
  void set_strategy(LoadBalancingStrategy strategy) { strategy_ = strategy; }
  void set_health_check_config(const HealthCheckConfig& config);
  void enable_health_checks(bool enable);
  
  // Statistics
  size_t total_servers() const;
  size_t healthy_servers() const;
  std::vector<std::shared_ptr<UpstreamServer>> get_all_servers() const;
  
  // Connection tracking
  void on_request_start(std::shared_ptr<UpstreamServer> server);
  void on_request_complete(std::shared_ptr<UpstreamServer> server,
                          std::chrono::milliseconds response_time,
                          bool success);

private:
  std::shared_ptr<UpstreamServer> round_robin_select();
  std::shared_ptr<UpstreamServer> least_connections_select();
  std::shared_ptr<UpstreamServer> weighted_round_robin_select();
  std::shared_ptr<UpstreamServer> random_select();
  std::shared_ptr<UpstreamServer> ip_hash_select(const std::string& client_ip);
  
  std::vector<std::shared_ptr<UpstreamServer>> get_healthy_servers() const;
  
  boost::asio::io_context& io_context_;
  LoadBalancingStrategy strategy_;
  
  mutable std::mutex mutex_;
  std::vector<std::shared_ptr<UpstreamServer>> servers_;
  
  // Round robin state
  size_t round_robin_index_;
  
  // Weighted round robin state
  std::vector<int> weighted_current_weights_;
  
  // Random number generator
  mutable std::random_device rd_;
  mutable std::mt19937 gen_;
  
  // Health checking
  std::unique_ptr<HealthChecker> health_checker_;
  bool health_checks_enabled_;
};

// Utility functions for integration with routing
std::shared_ptr<LoadBalancer> get_load_balancer_for_route(const ConnectionInfo& route);
void register_load_balancer(const std::string& route_key, 
                          std::shared_ptr<LoadBalancer> load_balancer);

} // namespace azugate

#endif // __LOAD_BALANCER_HPP
