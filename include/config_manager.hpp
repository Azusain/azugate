#ifndef __CONFIG_MANAGER_HPP
#define __CONFIG_MANAGER_HPP

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>
#include <filesystem>
#include <chrono>
#include <unordered_map>
#include <yaml-cpp/yaml.h>
#include <spdlog/spdlog.h>

namespace azugate {

// Configuration validation result
struct ValidationResult {
    bool valid = false;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    
    void add_error(const std::string& error) {
        errors.push_back(error);
        valid = false;
    }
    
    void add_warning(const std::string& warning) {
        warnings.push_back(warning);
    }
    
    bool has_issues() const {
        return !errors.empty() || !warnings.empty();
    }
    
    std::string to_string() const;
};

// Configuration change callback type
using ConfigChangeCallback = std::function<void(const YAML::Node& new_config)>;

// Configuration manager class
class ConfigManager {
public:
    static ConfigManager& instance();
    
    // Load and validate configuration
    bool load_config(const std::string& config_path);
    
    // Validate configuration without loading
    ValidationResult validate_config(const std::string& config_path);
    ValidationResult validate_config(const YAML::Node& config);
    
    // Generate sample configuration
    static bool generate_sample_config(const std::string& output_path);
    
    // Hot-reload functionality
    void enable_hot_reload(bool enable = true);
    void register_change_callback(const std::string& name, ConfigChangeCallback callback);
    void unregister_change_callback(const std::string& name);
    
    // Configuration access
    const YAML::Node& get_config() const { return current_config_; }
    std::string get_config_path() const { return config_path_; }
    std::chrono::system_clock::time_point get_last_modified() const { return last_modified_; }
    ValidationResult get_last_validation() const;
    
    // Configuration status for metrics endpoint
    std::string get_config_status_json() const;
    
    ~ConfigManager();

private:
    ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    
    // Validation helpers
    ValidationResult validate_server_config(const YAML::Node& config);
    ValidationResult validate_routes_config(const YAML::Node& config);
    ValidationResult validate_auth_config(const YAML::Node& config);
    ValidationResult validate_cache_config(const YAML::Node& config);
    ValidationResult validate_metrics_config(const YAML::Node& config);
    ValidationResult validate_circuit_breaker_config(const YAML::Node& config);
    ValidationResult validate_load_balancer_config(const YAML::Node& config);
    
    // File watching
    void start_file_watcher();
    void stop_file_watcher();
    void file_watcher_thread();
    bool check_file_modified();
    void reload_config();
    
    // Internal state
    mutable std::mutex config_mutex_;
    YAML::Node current_config_;
    std::string config_path_;
    std::chrono::system_clock::time_point last_modified_;
    ValidationResult last_validation_result_;
    
    // Hot-reload state
    std::atomic<bool> hot_reload_enabled_{false};
    std::atomic<bool> watcher_running_{false};
    std::thread watcher_thread_;
    std::unordered_map<std::string, ConfigChangeCallback> change_callbacks_;
    mutable std::mutex callbacks_mutex_;
    
    // File watching interval
    static constexpr std::chrono::milliseconds WATCH_INTERVAL{1000};
};

// Configuration validation utilities
class ConfigValidator {
public:
    // Validate common types
    static bool validate_port(int port, const std::string& field_name, ValidationResult& result);
    static bool validate_host(const std::string& host, const std::string& field_name, ValidationResult& result);
    static bool validate_path(const std::string& path, const std::string& field_name, ValidationResult& result);
    static bool validate_file_exists(const std::string& file_path, const std::string& field_name, ValidationResult& result);
    static bool validate_directory_exists(const std::string& dir_path, const std::string& field_name, ValidationResult& result);
    static bool validate_positive_number(double value, const std::string& field_name, ValidationResult& result);
    static bool validate_duration(const std::string& duration_str, const std::string& field_name, ValidationResult& result);
    
    // Validate required fields
    static bool require_field(const YAML::Node& config, const std::string& field_name, ValidationResult& result);
    static bool require_field(const YAML::Node& config, const std::string& field_name, YAML::NodeType::value expected_type, ValidationResult& result);
    
    // Validate enum values
    static bool validate_enum(const std::string& value, const std::vector<std::string>& valid_values, const std::string& field_name, ValidationResult& result);
};

// Configuration template generator
class ConfigTemplateGenerator {
public:
    static std::string generate_full_template();
    static std::string generate_minimal_template();
    static std::string generate_development_template();
    static std::string generate_production_template();
    
private:
    static std::string add_section_header(const std::string& title, const std::string& description = "");
    static std::string add_commented_field(const std::string& field, const std::string& value, const std::string& description = "", int indent = 0);
};

} // namespace azugate

#endif // __CONFIG_MANAGER_HPP
