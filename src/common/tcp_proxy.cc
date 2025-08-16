#include "../../include/services.hpp"
#include <boost/asio/buffer.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/bind/bind.hpp>
#include <memory>
#include <spdlog/spdlog.h>

namespace azugate {

// Async TCP proxy handler class for bidirectional data forwarding
class AsyncTcpProxy : public std::enable_shared_from_this<AsyncTcpProxy> {
public:
    AsyncTcpProxy(
        boost::shared_ptr<boost::asio::io_context> io_context_ptr,
        boost::shared_ptr<boost::asio::ip::tcp::socket> source_sock_ptr,
        const std::string& target_host,
        uint16_t target_port
    ) : io_context_ptr_(io_context_ptr), 
        source_sock_ptr_(source_sock_ptr),
        target_sock_ptr_(boost::make_shared<boost::asio::ip::tcp::socket>(*io_context_ptr)),
        target_host_(target_host),
        target_port_(target_port),
        source_buffer_(std::make_unique<std::array<char, 8192>>()),
        target_buffer_(std::make_unique<std::array<char, 8192>>()) {
    }

    void Start() {
        // Connect to target server
        ConnectToTarget();
    }

private:
    void ConnectToTarget() {
        using namespace boost::asio;
        
        // Resolve target address
        auto resolver = boost::make_shared<ip::tcp::resolver>(*io_context_ptr_);
        resolver->async_resolve(
            target_host_, std::to_string(target_port_),
            [this, resolver](boost::system::error_code ec, ip::tcp::resolver::results_type endpoints) {
                if (ec) {
                    SPDLOG_ERROR("Failed to resolve target {}:{} - {}", target_host_, target_port_, ec.message());
                    return;
                }
                
                // Connect to target
                async_connect(
                    *target_sock_ptr_, endpoints,
                    [this](boost::system::error_code ec, ip::tcp::endpoint) {
                        if (ec) {
                            SPDLOG_ERROR("Failed to connect to target {}:{} - {}", target_host_, target_port_, ec.message());
                            return;
                        }
                        
                        SPDLOG_INFO("TCP proxy established: client -> {}:{}", target_host_, target_port_);
                        
                        // Start bidirectional forwarding
                        StartForwarding();
                    }
                );
            }
        );
    }

    void StartForwarding() {
        // Start reading from both source and target simultaneously
        ReadFromSource();
        ReadFromTarget();
    }

    void ReadFromSource() {
        source_sock_ptr_->async_read_some(
            boost::asio::buffer(*source_buffer_),
            [this, self = shared_from_this()](boost::system::error_code ec, std::size_t bytes_read) {
                if (ec) {
                    if (ec != boost::asio::error::eof) {
                        SPDLOG_DEBUG("Source read error: {}", ec.message());
                    }
                    Shutdown();
                    return;
                }

                // Forward data to target
                WriteToTarget(bytes_read);
            }
        );
    }

    void ReadFromTarget() {
        target_sock_ptr_->async_read_some(
            boost::asio::buffer(*target_buffer_),
            [this, self = shared_from_this()](boost::system::error_code ec, std::size_t bytes_read) {
                if (ec) {
                    if (ec != boost::asio::error::eof) {
                        SPDLOG_DEBUG("Target read error: {}", ec.message());
                    }
                    Shutdown();
                    return;
                }

                // Forward data to source
                WriteToSource(bytes_read);
            }
        );
    }

    void WriteToTarget(std::size_t bytes_to_write) {
        boost::asio::async_write(
            *target_sock_ptr_,
            boost::asio::buffer(*source_buffer_, bytes_to_write),
            [this, self = shared_from_this()](boost::system::error_code ec, std::size_t bytes_written) {
                if (ec) {
                    SPDLOG_ERROR("Target write error: {}", ec.message());
                    Shutdown();
                    return;
                }

                // Continue reading from source
                ReadFromSource();
            }
        );
    }

    void WriteToSource(std::size_t bytes_to_write) {
        boost::asio::async_write(
            *source_sock_ptr_,
            boost::asio::buffer(*target_buffer_, bytes_to_write),
            [this, self = shared_from_this()](boost::system::error_code ec, std::size_t bytes_written) {
                if (ec) {
                    SPDLOG_ERROR("Source write error: {}", ec.message());
                    Shutdown();
                    return;
                }

                // Continue reading from target
                ReadFromTarget();
            }
        );
    }

    void Shutdown() {
        boost::system::error_code ec;
        
        // Shutdown source socket
        if (source_sock_ptr_ && source_sock_ptr_->is_open()) {
            source_sock_ptr_->shutdown(boost::asio::socket_base::shutdown_both, ec);
            if (ec && ec != boost::asio::error::not_connected) {
                SPDLOG_DEBUG("Source shutdown error: {}", ec.message());
            }
            source_sock_ptr_->close(ec);
        }

        // Shutdown target socket
        if (target_sock_ptr_ && target_sock_ptr_->is_open()) {
            target_sock_ptr_->shutdown(boost::asio::socket_base::shutdown_both, ec);
            if (ec && ec != boost::asio::error::not_connected) {
                SPDLOG_DEBUG("Target shutdown error: {}", ec.message());
            }
            target_sock_ptr_->close(ec);
        }

        SPDLOG_DEBUG("TCP proxy connection closed");
    }

private:
    boost::shared_ptr<boost::asio::io_context> io_context_ptr_;
    boost::shared_ptr<boost::asio::ip::tcp::socket> source_sock_ptr_;
    boost::shared_ptr<boost::asio::ip::tcp::socket> target_sock_ptr_;
    std::string target_host_;
    uint16_t target_port_;
    
    // Separate buffers for bidirectional communication
    std::unique_ptr<std::array<char, 8192>> source_buffer_;
    std::unique_ptr<std::array<char, 8192>> target_buffer_;
};

// Implementation of the TcpProxyHandler function
void TcpProxyHandler(
    const boost::shared_ptr<boost::asio::io_context> io_context_ptr,
    const boost::shared_ptr<boost::asio::ip::tcp::socket>& source_sock_ptr,
    std::optional<azugate::ConnectionInfo> target_connection_info_opt) {
    
    if (!target_connection_info_opt) {
        SPDLOG_ERROR("No target connection info provided for TCP proxy");
        return;
    }

    const auto& target_info = *target_connection_info_opt;
    
    if (target_info.address.empty()) {
        SPDLOG_ERROR("Empty target address for TCP proxy");
        return;
    }

    if (target_info.port == 0) {
        SPDLOG_ERROR("Invalid target port for TCP proxy: {}", target_info.port);
        return;
    }

    SPDLOG_INFO("Starting TCP proxy to {}:{}", target_info.address, target_info.port);

    // Create and start the async TCP proxy
    auto proxy = std::make_shared<AsyncTcpProxy>(
        io_context_ptr, 
        source_sock_ptr, 
        target_info.address, 
        target_info.port
    );
    
    proxy->Start();
}

} // namespace azugate
