#include "../../include/config_manager.hpp"
#include <fstream>
#include <regex>
#include <sstream>
#include <iomanip>
#include <nlohmann/json.hpp>

namespace azugate {

// ValidationResult implementation
std::string ValidationResult::to_string() const {
    std::stringstream ss;
    ss << "Validation " << (valid ? "PASSED" : "FAILED") << "\n";
    
    if (!errors.empty()) {
        ss << "\nErrors:\n";
        for (const auto& error : errors) {
            ss << "  - " << error << "\n";
        }
    }
    
    if (!warnings.empty()) {
        ss << "\nWarnings:\n";
        for (const auto& warning : warnings) {
            ss << "  - " << warning << "\n";
        }
    }
    
    return ss.str();
}

// ConfigManager implementation
ConfigManager& ConfigManager::instance() {
    static ConfigManager instance;
    return instance;
}

bool ConfigManager::load_config(const std::string& config_path) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    try {
        YAML::Node config = YAML::LoadFile(config_path);
        ValidationResult validation = validate_config(config);
        
        if (!validation.valid) {
            SPDLOG_ERROR("Configuration validation failed for {}: {}", 
                        config_path, validation.to_string());
            return false;
        }
        
        if (validation.has_issues()) {
            SPDLOG_WARN("Configuration has warnings for {}: {}", 
                       config_path, validation.to_string());
        }
        
        current_config_ = config;
        config_path_ = config_path;
        
        // Get file modification time
        if (std::filesystem::exists(config_path)) {
            last_modified_ = std::filesystem::last_write_time(config_path);
        }
        
        last_validation_result_ = validation;
        
        SPDLOG_INFO("Successfully loaded configuration from {}", config_path);
        return true;
        
    } catch (const YAML::Exception& e) {
        SPDLOG_ERROR("YAML parsing error in {}: {}", config_path, e.what());
        return false;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Error loading configuration from {}: {}", config_path, e.what());
        return false;
    }
}

ValidationResult ConfigManager::validate_config(const std::string& config_path) {
    ValidationResult result;
    result.valid = true;
    
    try {
        YAML::Node config = YAML::LoadFile(config_path);
        return validate_config(config);
    } catch (const YAML::Exception& e) {
        result.add_error("YAML parsing error: " + std::string(e.what()));
        return result;
    } catch (const std::exception& e) {
        result.add_error("File error: " + std::string(e.what()));
        return result;
    }
}

ValidationResult ConfigManager::validate_config(const YAML::Node& config) {
    ValidationResult result;
    result.valid = true;
    
    // Validate main sections
    auto server_result = validate_server_config(config);
    auto routes_result = validate_routes_config(config);
    auto auth_result = validate_auth_config(config);
    auto cache_result = validate_cache_config(config);
    auto metrics_result = validate_metrics_config(config);
    auto cb_result = validate_circuit_breaker_config(config);
    auto lb_result = validate_load_balancer_config(config);
    
    // Merge results
    result.errors.insert(result.errors.end(), server_result.errors.begin(), server_result.errors.end());
    result.errors.insert(result.errors.end(), routes_result.errors.begin(), routes_result.errors.end());
    result.errors.insert(result.errors.end(), auth_result.errors.begin(), auth_result.errors.end());
    result.errors.insert(result.errors.end(), cache_result.errors.begin(), cache_result.errors.end());
    result.errors.insert(result.errors.end(), metrics_result.errors.begin(), metrics_result.errors.end());
    result.errors.insert(result.errors.end(), cb_result.errors.begin(), cb_result.errors.end());
    result.errors.insert(result.errors.end(), lb_result.errors.begin(), lb_result.errors.end());
    
    result.warnings.insert(result.warnings.end(), server_result.warnings.begin(), server_result.warnings.end());
    result.warnings.insert(result.warnings.end(), routes_result.warnings.begin(), routes_result.warnings.end());
    result.warnings.insert(result.warnings.end(), auth_result.warnings.begin(), auth_result.warnings.end());
    result.warnings.insert(result.warnings.end(), cache_result.warnings.begin(), cache_result.warnings.end());
    result.warnings.insert(result.warnings.end(), metrics_result.warnings.begin(), metrics_result.warnings.end());
    result.warnings.insert(result.warnings.end(), cb_result.warnings.begin(), cb_result.warnings.end());
    result.warnings.insert(result.warnings.end(), lb_result.warnings.begin(), lb_result.warnings.end());
    
    result.valid = result.errors.empty();
    return result;
}

ValidationResult ConfigManager::validate_server_config(const YAML::Node& config) {
    ValidationResult result;
    result.valid = true;
    
    if (!config["server"]) {
        result.add_warning("No server configuration section found, using defaults");
        return result;
    }
    
    const auto& server = config["server"];
    
    // Validate port
    if (server["port"]) {
        int port = server["port"].as<int>();
        ConfigValidator::validate_port(port, "server.port", result);
    }
    
    // Validate host
    if (server["host"]) {
        std::string host = server["host"].as<std::string>();
        ConfigValidator::validate_host(host, "server.host", result);
    }
    
    // Validate SSL settings
    if (server["ssl"]) {
        const auto& ssl = server["ssl"];
        if (ssl["enabled"] && ssl["enabled"].as<bool>()) {
            if (ssl["cert_file"]) {
                std::string cert_file = ssl["cert_file"].as<std::string>();
                ConfigValidator::validate_file_exists(cert_file, "server.ssl.cert_file", result);
            }
            if (ssl["key_file"]) {
                std::string key_file = ssl["key_file"].as<std::string>();
                ConfigValidator::validate_file_exists(key_file, "server.ssl.key_file", result);
            }
        }
    }
    
    // Validate worker threads
    if (server["worker_threads"]) {
        int threads = server["worker_threads"].as<int>();
        if (threads < 1 || threads > 256) {
            result.add_error("server.worker_threads must be between 1 and 256");
        }
    }
    
    return result;
}

ValidationResult ConfigManager::validate_routes_config(const YAML::Node& config) {
    ValidationResult result;
    result.valid = true;
    
    if (!config["routes"]) {
        result.add_error("No routes configuration found - at least one route is required");
        return result;
    }
    
    const auto& routes = config["routes"];
    if (!routes.IsSequence()) {
        result.add_error("routes must be an array");
        return result;
    }
    
    if (routes.size() == 0) {
        result.add_error("At least one route must be configured");
        return result;
    }
    
    for (size_t i = 0; i < routes.size(); ++i) {
        const auto& route = routes[i];
        std::string route_prefix = "routes[" + std::to_string(i) + "]";
        
        // Validate required fields
        if (!ConfigValidator::require_field(route, "path", YAML::NodeType::Scalar, result)) {
            continue;
        }
        
        // Validate upstream configuration
        if (route["upstream"]) {
            const auto& upstream = route["upstream"];
            if (upstream["servers"] && upstream["servers"].IsSequence()) {
                for (size_t j = 0; j < upstream["servers"].size(); ++j) {
                    const auto& server = upstream["servers"][j];
                    std::string server_prefix = route_prefix + ".upstream.servers[" + std::to_string(j) + "]";
                    
                    if (server["host"]) {
                        ConfigValidator::validate_host(server["host"].as<std::string>(), 
                                                     server_prefix + ".host", result);
                    }
                    if (server["port"]) {
                        ConfigValidator::validate_port(server["port"].as<int>(), 
                                                     server_prefix + ".port", result);
                    }
                }
            }
        }
        
        // Validate file serving
        if (route["file_server"]) {
            const auto& file_server = route["file_server"];
            if (file_server["root"]) {
                std::string root = file_server["root"].as<std::string>();
                ConfigValidator::validate_directory_exists(root, route_prefix + ".file_server.root", result);
            }
        }
    }
    
    return result;
}

ValidationResult ConfigManager::validate_auth_config(const YAML::Node& config) {
    ValidationResult result;
    result.valid = true;
    
    if (config["auth"]) {
        const auto& auth = config["auth"];
        if (auth["jwt"]) {
            const auto& jwt = auth["jwt"];
            if (jwt["secret_key"]) {
                std::string secret = jwt["secret_key"].as<std::string>();
                if (secret.length() < 32) {
                    result.add_warning("JWT secret key should be at least 32 characters for security");
                }
            }
        }
    }
    
    return result;
}

ValidationResult ConfigManager::validate_cache_config(const YAML::Node& config) {
    ValidationResult result;
    result.valid = true;
    
    if (config["cache"]) {
        const auto& cache = config["cache"];
        if (cache["max_size"]) {
            std::string size_str = cache["max_size"].as<std::string>();
            // Basic size validation (could be enhanced)
            if (!std::regex_match(size_str, std::regex(R"(\d+[KMGT]?B?)", std::regex_constants::icase))) {
                result.add_error("cache.max_size must be in format like '100MB', '1GB', etc.");
            }
        }
        
        if (cache["ttl"]) {
            std::string ttl = cache["ttl"].as<std::string>();
            ConfigValidator::validate_duration(ttl, "cache.ttl", result);
        }
    }
    
    return result;
}

ValidationResult ConfigManager::validate_metrics_config(const YAML::Node& config) {
    ValidationResult result;
    result.valid = true;
    
    if (config["metrics"]) {
        const auto& metrics = config["metrics"];
        if (metrics["port"]) {
            int port = metrics["port"].as<int>();
            ConfigValidator::validate_port(port, "metrics.port", result);
        }
    }
    
    return result;
}

ValidationResult ConfigManager::validate_circuit_breaker_config(const YAML::Node& config) {
    ValidationResult result;
    result.valid = true;
    
    if (config["circuit_breaker"]) {
        const auto& cb = config["circuit_breaker"];
        if (cb["failure_threshold"]) {
            int threshold = cb["failure_threshold"].as<int>();
            if (threshold < 1) {
                result.add_error("circuit_breaker.failure_threshold must be at least 1");
            }
        }
        
        if (cb["timeout"]) {
            std::string timeout = cb["timeout"].as<std::string>();
            ConfigValidator::validate_duration(timeout, "circuit_breaker.timeout", result);
        }
    }
    
    return result;
}

ValidationResult ConfigManager::validate_load_balancer_config(const YAML::Node& config) {
    ValidationResult result;
    result.valid = true;
    
    if (config["load_balancer"]) {
        const auto& lb = config["load_balancer"];
        if (lb["strategy"]) {
            std::string strategy = lb["strategy"].as<std::string>();
            std::vector<std::string> valid_strategies = {"round_robin", "least_connections", "weighted", "ip_hash"};
            ConfigValidator::validate_enum(strategy, valid_strategies, "load_balancer.strategy", result);
        }
    }
    
    return result;
}

void ConfigManager::enable_hot_reload(bool enable) {
    if (enable == hot_reload_enabled_.load()) {
        return; // No change
    }
    
    hot_reload_enabled_.store(enable);
    
    if (enable) {
        start_file_watcher();
    } else {
        stop_file_watcher();
    }
}

void ConfigManager::register_change_callback(const std::string& name, ConfigChangeCallback callback) {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    change_callbacks_[name] = callback;
}

void ConfigManager::unregister_change_callback(const std::string& name) {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    change_callbacks_.erase(name);
}

void ConfigManager::start_file_watcher() {
    if (watcher_running_.load()) {
        return;
    }
    
    watcher_running_.store(true);
    watcher_thread_ = std::thread([this]() { file_watcher_thread(); });
    
    SPDLOG_INFO("Configuration hot-reload enabled for {}", config_path_);
}

void ConfigManager::stop_file_watcher() {
    watcher_running_.store(false);
    if (watcher_thread_.joinable()) {
        watcher_thread_.join();
    }
    
    SPDLOG_INFO("Configuration hot-reload disabled");
}

void ConfigManager::file_watcher_thread() {
    while (watcher_running_.load()) {
        try {
            if (check_file_modified()) {
                reload_config();
            }
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Error in configuration file watcher: {}", e.what());
        }
        
        std::this_thread::sleep_for(WATCH_INTERVAL);
    }
}

bool ConfigManager::check_file_modified() {
    if (config_path_.empty() || !std::filesystem::exists(config_path_)) {
        return false;
    }
    
    auto current_modified = std::filesystem::last_write_time(config_path_);
    return current_modified > last_modified_;
}

void ConfigManager::reload_config() {
    SPDLOG_INFO("Configuration file changed, reloading...");
    
    std::string old_config_path = config_path_;
    if (load_config(old_config_path)) {
        // Notify callbacks
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        for (const auto& [name, callback] : change_callbacks_) {
            try {
                callback(current_config_);
                SPDLOG_DEBUG("Configuration change callback '{}' executed successfully", name);
            } catch (const std::exception& e) {
                SPDLOG_ERROR("Configuration change callback '{}' failed: {}", name, e.what());
            }
        }
        
        SPDLOG_INFO("Configuration reloaded successfully");
    } else {
        SPDLOG_ERROR("Failed to reload configuration, keeping existing config");
    }
}

ValidationResult ConfigManager::get_last_validation() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return last_validation_result_;
}

std::string ConfigManager::get_config_status_json() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    nlohmann::json status;
    status["config_path"] = config_path_;
    status["hot_reload_enabled"] = hot_reload_enabled_.load();
    
    if (last_modified_.time_since_epoch().count() > 0) {
        auto time_t = std::chrono::system_clock::to_time_t(last_modified_);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
        status["last_modified"] = ss.str();
    }
    
    status["validation"]["valid"] = last_validation_result_.valid;
    status["validation"]["errors"] = last_validation_result_.errors;
    status["validation"]["warnings"] = last_validation_result_.warnings;
    
    return status.dump(2);
}

ConfigManager::~ConfigManager() {
    enable_hot_reload(false);
}

// ConfigValidator implementation
bool ConfigValidator::validate_port(int port, const std::string& field_name, ValidationResult& result) {
    if (port < 1 || port > 65535) {
        result.add_error(field_name + " must be between 1 and 65535");
        return false;
    }
    return true;
}

bool ConfigValidator::validate_host(const std::string& host, const std::string& field_name, ValidationResult& result) {
    if (host.empty()) {
        result.add_error(field_name + " cannot be empty");
        return false;
    }
    
    // Basic host validation (could be enhanced with proper regex)
    if (host.find_first_of(" \t\n\r") != std::string::npos) {
        result.add_error(field_name + " contains invalid characters");
        return false;
    }
    
    return true;
}

bool ConfigValidator::validate_path(const std::string& path, const std::string& field_name, ValidationResult& result) {
    if (path.empty()) {
        result.add_error(field_name + " cannot be empty");
        return false;
    }
    
    // Check for invalid path characters
    if (path.find('\0') != std::string::npos) {
        result.add_error(field_name + " contains null characters");
        return false;
    }
    
    return true;
}

bool ConfigValidator::validate_file_exists(const std::string& file_path, const std::string& field_name, ValidationResult& result) {
    if (!validate_path(file_path, field_name, result)) {
        return false;
    }
    
    if (!std::filesystem::exists(file_path)) {
        result.add_error(field_name + " file does not exist: " + file_path);
        return false;
    }
    
    if (!std::filesystem::is_regular_file(file_path)) {
        result.add_error(field_name + " is not a regular file: " + file_path);
        return false;
    }
    
    return true;
}

bool ConfigValidator::validate_directory_exists(const std::string& dir_path, const std::string& field_name, ValidationResult& result) {
    if (!validate_path(dir_path, field_name, result)) {
        return false;
    }
    
    if (!std::filesystem::exists(dir_path)) {
        result.add_error(field_name + " directory does not exist: " + dir_path);
        return false;
    }
    
    if (!std::filesystem::is_directory(dir_path)) {
        result.add_error(field_name + " is not a directory: " + dir_path);
        return false;
    }
    
    return true;
}

bool ConfigValidator::validate_positive_number(double value, const std::string& field_name, ValidationResult& result) {
    if (value <= 0) {
        result.add_error(field_name + " must be a positive number");
        return false;
    }
    return true;
}

bool ConfigValidator::validate_duration(const std::string& duration_str, const std::string& field_name, ValidationResult& result) {
    // Basic duration validation for formats like "5s", "10m", "1h"
    std::regex duration_regex(R"(\d+[smhd]?)", std::regex_constants::icase);
    if (!std::regex_match(duration_str, duration_regex)) {
        result.add_error(field_name + " must be in format like '30s', '5m', '1h', '2d'");
        return false;
    }
    return true;
}

bool ConfigValidator::require_field(const YAML::Node& config, const std::string& field_name, ValidationResult& result) {
    if (!config[field_name]) {
        result.add_error("Required field missing: " + field_name);
        return false;
    }
    return true;
}

bool ConfigValidator::require_field(const YAML::Node& config, const std::string& field_name, YAML::NodeType::value expected_type, ValidationResult& result) {
    if (!require_field(config, field_name, result)) {
        return false;
    }
    
    if (config[field_name].Type() != expected_type) {
        result.add_error("Field " + field_name + " has wrong type");
        return false;
    }
    
    return true;
}

bool ConfigValidator::validate_enum(const std::string& value, const std::vector<std::string>& valid_values, const std::string& field_name, ValidationResult& result) {
    if (std::find(valid_values.begin(), valid_values.end(), value) == valid_values.end()) {
        std::string valid_list;
        for (size_t i = 0; i < valid_values.size(); ++i) {
            if (i > 0) valid_list += ", ";
            valid_list += "'" + valid_values[i] + "'";
        }
        result.add_error(field_name + " must be one of: " + valid_list);
        return false;
    }
    return true;
}

} // namespace azugate
