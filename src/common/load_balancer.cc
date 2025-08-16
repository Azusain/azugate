#include "../../include/load_balancer.hpp"
#include <algorithm>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <functional>
#include <numeric>

namespace azugate {

// Static registry for load balancers
static std::unordered_map<std::string, std::shared_ptr<LoadBalancer>> load_balancer_registry;
static std::mutex registry_mutex;

// UpstreamServer Implementation
UpstreamServer::UpstreamServer(const std::string& address, uint16_t port, int weight)
    : address_(address), port_(port), weight_(weight), 
      health_status_(HealthStatus::Unknown), active_connections_(0),
      consecutive_successes_(0), consecutive_failures_(0),
      avg_response_time_ms_(0.0), total_checks_(0), total_successes_(0) {
  last_check_time_ = std::chrono::steady_clock::now();
}

void UpstreamServer::increment_connections() {
  std::lock_guard<std::mutex> lock(mutex_);
  ++active_connections_;
}

void UpstreamServer::decrement_connections() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (active_connections_ > 0) {
    --active_connections_;
  }
}

void UpstreamServer::set_health_status(HealthStatus status) {
  std::lock_guard<std::mutex> lock(mutex_);
  health_status_ = status;
  last_check_time_ = std::chrono::steady_clock::now();
}

void UpstreamServer::record_health_check_success() {
  std::lock_guard<std::mutex> lock(mutex_);
  consecutive_successes_++;
  consecutive_failures_ = 0;
  total_checks_++;
  total_successes_++;
  last_check_time_ = std::chrono::steady_clock::now();
}

void UpstreamServer::record_health_check_failure() {
  std::lock_guard<std::mutex> lock(mutex_);
  consecutive_failures_++;
  consecutive_successes_ = 0;
  total_checks_++;
  last_check_time_ = std::chrono::steady_clock::now();
}

void UpstreamServer::update_response_time(std::chrono::milliseconds response_time) {
  std::lock_guard<std::mutex> lock(mutex_);
  double response_time_ms = static_cast<double>(response_time.count());
  
  // Exponential moving average with alpha = 0.3
  if (avg_response_time_ms_ == 0.0) {
    avg_response_time_ms_ = response_time_ms;
  } else {
    avg_response_time_ms_ = 0.7 * avg_response_time_ms_ + 0.3 * response_time_ms;
  }
}

bool UpstreamServer::is_available() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return health_status_ == HealthStatus::Healthy || 
         health_status_ == HealthStatus::Unknown ||
         health_status_ == HealthStatus::Recovering;
}

// HealthChecker Implementation
HealthChecker::HealthChecker(boost::asio::io_context& io_context, 
                           const HealthCheckConfig& config)
    : io_context_(io_context), config_(config), stopped_(false) {}

void HealthChecker::start_health_check(std::shared_ptr<UpstreamServer> server) {
  if (stopped_) return;
  
  auto timer = std::make_shared<boost::asio::steady_timer>(io_context_);
  timers_.push_back(timer);
  
  // Start immediate check
  perform_health_check(server);
}

void HealthChecker::stop_health_checks() {
  stopped_ = true;
  for (auto& timer : timers_) {
    timer->cancel();
  }
  timers_.clear();
}

void HealthChecker::perform_health_check(std::shared_ptr<UpstreamServer> server) {
  if (stopped_) return;
  
  auto start_time = std::chrono::steady_clock::now();
  
  // Create HTTP client for health check
  auto resolver = std::make_shared<boost::asio::ip::tcp::resolver>(io_context_);
  auto socket = std::make_shared<boost::asio::ip::tcp::socket>(io_context_);
  
  resolver->async_resolve(
    server->address(), std::to_string(server->port()),
    [this, server, socket, start_time](
        boost::system::error_code ec,
        boost::asio::ip::tcp::resolver::results_type results) {
      
      if (ec) {
        SPDLOG_WARN("Health check DNS resolution failed for {}:{}: {}", 
                   server->address(), server->port(), ec.message());
        server->record_health_check_failure();
        schedule_next_check(server);
        return;
      }
      
      // Connect to server
      boost::asio::async_connect(*socket, results,
        [this, server, socket, start_time](
            boost::system::error_code ec, 
            const boost::asio::ip::tcp::endpoint&) {
          
          if (ec) {
            SPDLOG_WARN("Health check connection failed for {}:{}: {}", 
                       server->address(), server->port(), ec.message());
            server->record_health_check_failure();
            schedule_next_check(server);
            return;
          }
          
          // Send HTTP GET request to health endpoint
          namespace http = boost::beast::http;
          
          http::request<http::empty_body> req{http::verb::get, config_.path, 11};
          req.set(http::field::host, server->address());
          req.set(http::field::user_agent, AZUGATE_VERSION_STRING);
          req.set(http::field::connection, "close");
          
          auto write_buffer = std::make_shared<boost::beast::flat_buffer>();
          auto read_buffer = std::make_shared<boost::beast::flat_buffer>();
          auto parser = std::make_shared<http::response_parser<http::string_body>>();
          
          // Write request
          http::async_write(*socket, req,
            [this, server, socket, start_time, read_buffer, parser](
                boost::system::error_code ec, std::size_t) {
              
              if (ec) {
                SPDLOG_WARN("Health check write failed for {}:{}: {}", 
                           server->address(), server->port(), ec.message());
                server->record_health_check_failure();
                schedule_next_check(server);
                return;
              }
              
              // Read response
              http::async_read(*socket, *read_buffer, *parser,
                [this, server, socket, start_time, parser](
                    boost::system::error_code ec, std::size_t) {
                  
                  auto end_time = std::chrono::steady_clock::now();
                  auto response_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                      end_time - start_time);
                  
                  if (ec && ec != boost::beast::http::error::end_of_stream) {
                    SPDLOG_WARN("Health check read failed for {}:{}: {}", 
                               server->address(), server->port(), ec.message());
                    server->record_health_check_failure();
                  } else {
                    auto& response = parser->get();
                    bool success = static_cast<int>(response.result()) == config_.expected_status;
                    
                    if (!config_.expected_body.empty()) {
                      success = success && (response.body() == config_.expected_body);
                    }
                    
                    if (success) {
                      server->record_health_check_success();
                      server->update_response_time(response_time);
                      SPDLOG_DEBUG("Health check success for {}:{} - {}ms", 
                                  server->address(), server->port(), response_time.count());
                    } else {
                      server->record_health_check_failure();
                      SPDLOG_WARN("Health check failed for {}:{} - status: {}, body: {}", 
                                 server->address(), server->port(), 
                                 static_cast<int>(response.result()), response.body());
                    }
                  }
                  
                  // Close socket
                  socket->close();
                  schedule_next_check(server);
                });
            });
        });
    });
}

void HealthChecker::schedule_next_check(std::shared_ptr<UpstreamServer> server) {
  if (stopped_) return;
  
  auto timer = std::make_shared<boost::asio::steady_timer>(io_context_);
  timer->expires_after(config_.interval);
  
  timer->async_wait([this, server, timer](boost::system::error_code ec) {
    if (!ec && !stopped_) {
      perform_health_check(server);
    }
  });
  
  timers_.push_back(timer);
}

// LoadBalancer Implementation
LoadBalancer::LoadBalancer(boost::asio::io_context& io_context,
                         LoadBalancingStrategy strategy)
    : io_context_(io_context), strategy_(strategy), round_robin_index_(0),
      gen_(rd_()), health_checks_enabled_(false) {}

LoadBalancer::~LoadBalancer() {
  if (health_checker_) {
    health_checker_->stop_health_checks();
  }
}

void LoadBalancer::add_server(const std::string& address, uint16_t port, int weight) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  auto server = std::make_shared<UpstreamServer>(address, port, weight);
  servers_.push_back(server);
  weighted_current_weights_.push_back(0);
  
  if (health_checker_ && health_checks_enabled_) {
    health_checker_->start_health_check(server);
  }
  
  SPDLOG_INFO("Added upstream server {}:{} with weight {}", address, port, weight);
}

void LoadBalancer::remove_server(const std::string& address, uint16_t port) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  auto it = std::remove_if(servers_.begin(), servers_.end(),
    [&address, port](const std::shared_ptr<UpstreamServer>& server) {
      return server->address() == address && server->port() == port;
    });
  
  if (it != servers_.end()) {
    size_t removed_index = std::distance(servers_.begin(), it);
    servers_.erase(it, servers_.end());
    
    if (removed_index < weighted_current_weights_.size()) {
      weighted_current_weights_.erase(weighted_current_weights_.begin() + removed_index);
    }
    
    if (round_robin_index_ >= servers_.size() && !servers_.empty()) {
      round_robin_index_ = 0;
    }
    
    SPDLOG_INFO("Removed upstream server {}:{}", address, port);
  }
}

std::shared_ptr<UpstreamServer> LoadBalancer::get_server(const std::string& client_ip) {
  switch (strategy_) {
    case LoadBalancingStrategy::RoundRobin:
      return round_robin_select();
    case LoadBalancingStrategy::LeastConnections:
      return least_connections_select();
    case LoadBalancingStrategy::WeightedRoundRobin:
      return weighted_round_robin_select();
    case LoadBalancingStrategy::Random:
      return random_select();
    case LoadBalancingStrategy::IpHash:
      return ip_hash_select(client_ip);
    default:
      return round_robin_select();
  }
}

void LoadBalancer::set_health_check_config(const HealthCheckConfig& config) {
  if (health_checker_) {
    health_checker_->stop_health_checks();
  }
  
  health_checker_ = std::make_unique<HealthChecker>(io_context_, config);
  
  if (health_checks_enabled_) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& server : servers_) {
      health_checker_->start_health_check(server);
    }
  }
}

void LoadBalancer::enable_health_checks(bool enable) {
  health_checks_enabled_ = enable;
  
  if (enable && health_checker_) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& server : servers_) {
      health_checker_->start_health_check(server);
    }
  } else if (!enable && health_checker_) {
    health_checker_->stop_health_checks();
  }
}

size_t LoadBalancer::total_servers() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return servers_.size();
}

size_t LoadBalancer::healthy_servers() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return std::count_if(servers_.begin(), servers_.end(),
    [](const std::shared_ptr<UpstreamServer>& server) {
      return server->is_available();
    });
}

std::vector<std::shared_ptr<UpstreamServer>> LoadBalancer::get_all_servers() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return servers_;
}

void LoadBalancer::on_request_start(std::shared_ptr<UpstreamServer> server) {
  if (server) {
    server->increment_connections();
  }
}

void LoadBalancer::on_request_complete(std::shared_ptr<UpstreamServer> server,
                                     std::chrono::milliseconds response_time,
                                     bool success) {
  if (server) {
    server->decrement_connections();
    if (success) {
      server->update_response_time(response_time);
    }
  }
}

// Private selection methods
std::shared_ptr<UpstreamServer> LoadBalancer::round_robin_select() {
  auto healthy_servers = get_healthy_servers();
  if (healthy_servers.empty()) return nullptr;
  
  std::lock_guard<std::mutex> lock(mutex_);
  if (round_robin_index_ >= healthy_servers.size()) {
    round_robin_index_ = 0;
  }
  
  auto selected = healthy_servers[round_robin_index_++];
  if (round_robin_index_ >= healthy_servers.size()) {
    round_robin_index_ = 0;
  }
  
  return selected;
}

std::shared_ptr<UpstreamServer> LoadBalancer::least_connections_select() {
  auto healthy_servers = get_healthy_servers();
  if (healthy_servers.empty()) return nullptr;
  
  return *std::min_element(healthy_servers.begin(), healthy_servers.end(),
    [](const std::shared_ptr<UpstreamServer>& a, const std::shared_ptr<UpstreamServer>& b) {
      return a->active_connections() < b->active_connections();
    });
}

std::shared_ptr<UpstreamServer> LoadBalancer::weighted_round_robin_select() {
  auto healthy_servers = get_healthy_servers();
  if (healthy_servers.empty()) return nullptr;
  
  std::lock_guard<std::mutex> lock(mutex_);
  
  // Ensure weights vector is properly sized
  while (weighted_current_weights_.size() < healthy_servers.size()) {
    weighted_current_weights_.push_back(0);
  }
  
  int total_weight = 0;
  std::shared_ptr<UpstreamServer> best = nullptr;
  int best_current_weight = -1;
  
  for (size_t i = 0; i < healthy_servers.size(); ++i) {
    auto server = healthy_servers[i];
    int weight = server->weight();
    
    weighted_current_weights_[i] += weight;
    total_weight += weight;
    
    if (weighted_current_weights_[i] > best_current_weight) {
      best_current_weight = weighted_current_weights_[i];
      best = server;
    }
  }
  
  if (best) {
    // Find the index of the best server and reduce its current weight
    for (size_t i = 0; i < healthy_servers.size(); ++i) {
      if (healthy_servers[i] == best) {
        weighted_current_weights_[i] -= total_weight;
        break;
      }
    }
  }
  
  return best;
}

std::shared_ptr<UpstreamServer> LoadBalancer::random_select() {
  auto healthy_servers = get_healthy_servers();
  if (healthy_servers.empty()) return nullptr;
  
  std::uniform_int_distribution<size_t> dis(0, healthy_servers.size() - 1);
  return healthy_servers[dis(gen_)];
}

std::shared_ptr<UpstreamServer> LoadBalancer::ip_hash_select(const std::string& client_ip) {
  auto healthy_servers = get_healthy_servers();
  if (healthy_servers.empty()) return nullptr;
  
  // Simple hash based on IP address
  std::hash<std::string> hasher;
  size_t hash = hasher(client_ip);
  size_t index = hash % healthy_servers.size();
  
  return healthy_servers[index];
}

std::vector<std::shared_ptr<UpstreamServer>> LoadBalancer::get_healthy_servers() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<std::shared_ptr<UpstreamServer>> healthy;
  
  std::copy_if(servers_.begin(), servers_.end(), std::back_inserter(healthy),
    [](const std::shared_ptr<UpstreamServer>& server) {
      return server->is_available();
    });
  
  return healthy;
}

// Utility functions
std::shared_ptr<LoadBalancer> get_load_balancer_for_route(const ConnectionInfo& route) {
  std::lock_guard<std::mutex> lock(registry_mutex);
  auto key = std::string(route.http_url);
  auto it = load_balancer_registry.find(key);
  return (it != load_balancer_registry.end()) ? it->second : nullptr;
}

void register_load_balancer(const std::string& route_key, 
                          std::shared_ptr<LoadBalancer> load_balancer) {
  std::lock_guard<std::mutex> lock(registry_mutex);
  load_balancer_registry[route_key] = load_balancer;
  SPDLOG_INFO("Registered load balancer for route: {}", route_key);
}

} // namespace azugate
