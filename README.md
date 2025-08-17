# Azugate

A lightweight HTTP proxy server with built-in file serving capabilities. Azugate provides a simple yet powerful solution for HTTP proxying and static file serving with directory listing support.

> **Latest Version**: v2.0.0 - Simplified architecture with command-line configuration (gRPC removed)  
> **Legacy Version**: v1.x available in `legacy` branch with gRPC support

## Features

- **HTTP Proxy Server**: Forward HTTP requests to upstream servers
- **HTTPS Support**: Enable secure connections with TLS
- **File Proxy Server**: Serve static files from local directories
- **Directory Listing**: Nginx-style directory index pages with file information
- **HTTP Compression**: Built-in gzip compression support
- **Rate Limiting**: Configurable request rate limiting
- **Command-line Configuration**: Easy configuration through command-line arguments
- **Cross-platform**: Built with modern C++ for Windows and Linux

## Installation

### Package Managers (Recommended)

#### Homebrew (macOS/Linux)
```bash
# Install from custom tap
brew tap azusain/azugate
brew install azugate

# Or install directly from formula URL
brew install https://raw.githubusercontent.com/Azusain/azugate/main/Formula/azugate.rb
```

#### APT (Ubuntu/Debian)
```bash
# Download and install .deb package
wget https://github.com/Azusain/azugate/releases/latest/download/azugate_1.0.0-1_amd64.deb
sudo dpkg -i azugate_1.0.0-1_amd64.deb
sudo apt-get install -f  # Fix dependencies if needed
```

#### Manual Installation
```bash
# Download pre-built binaries
curl -LO https://github.com/Azusain/azugate/releases/latest/download/azugate-linux-x64.tar.gz
tar -xzf azugate-linux-x64.tar.gz
sudo cp azugate /usr/local/bin/
```

### Building from Source

#### Prerequisites

- C++ compiler with C++20 support
- CMake 3.25 or later
- vcpkg package manager

### Dependencies

The project uses the following libraries (automatically managed by vcpkg):
- `cpprestsdk`: HTTP client/server framework
- `cxxopts`: Command-line argument parsing
- `fmt`: String formatting library

### Building

1. Clone the repository:
```bash
git clone <repository-url>
cd azugate
```

2. Install dependencies using vcpkg:
```bash
vcpkg install
```

3. Build the project:
```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=[path-to-vcpkg]/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

## Usage

### Basic HTTP Proxy

Start a basic HTTP proxy server on port 8080:
```bash
./azugate --port 8080
```

### HTTPS Proxy with Compression

Enable HTTPS and compression:
```bash
./azugate --port 8443 --enable-https --enable-compression
```

### File Proxy Server

Serve files from a local directory with directory listing:
```bash
./azugate --port 8080 --enable-file-proxy --proxy-directory "/path/to/serve"
```

### Advanced Configuration

Combine multiple features with rate limiting:
```bash
./azugate \
  --port 8080 \
  --enable-https \
  --enable-compression \
  --enable-rate-limiter \
  --enable-file-proxy \
  --proxy-directory "/var/www/html"
```

## Command-line Options

| Option | Description | Default |
|--------|-------------|---------|
| `--port` | Server port number | 8080 |
| `--enable-https` | Enable HTTPS support | false |
| `--enable-compression` | Enable HTTP compression | false |
| `--enable-rate-limiter` | Enable request rate limiting | false |
| `--enable-file-proxy` | Enable file proxy mode | false |
| `--proxy-directory` | Directory to serve files from | "" |
| `--help` | Show help message | - |

## File Proxy Mode

When file proxy mode is enabled, Azugate serves static files from the specified directory. Features include:

- **Static File Serving**: Serve files with appropriate MIME types
- **Directory Listing**: Auto-generate directory index pages
- **File Information**: Display file sizes and modification dates
- **Nginx-style Interface**: Familiar directory listing format
- **Path Navigation**: Click-through directory navigation
- **Security**: Prevents directory traversal attacks

### Directory Listing Example

When accessing a directory, you'll see a formatted listing like:
```
Index of /examples/

[DIR]  assets/                     -        2024-01-15 10:30:00
[FILE] index.html              2.5KB       2024-01-15 09:45:00
[FILE] script.js               1.2KB       2024-01-15 09:50:00
[FILE] styles.css                834B      2024-01-15 09:48:00
```

## Configuration Examples

### Development Server
Perfect for local development with file serving:
```bash
./azugate --enable-file-proxy --proxy-directory "./public" --port 3000
```

### Production Proxy
High-performance proxy with security features:
```bash
./azugate --port 80 --enable-https --enable-compression --enable-rate-limiter
```

### Static Site Hosting
Host a static website with HTTPS:
```bash
./azugate \
  --enable-file-proxy \
  --proxy-directory "/var/www/mysite" \
  --port 443 \
  --enable-https \
  --enable-compression
```

## API Endpoints

When running as an HTTP proxy, all requests are forwarded to upstream servers. In file proxy mode:

- `GET /`: Returns directory listing or file content
- `GET /path/to/file`: Returns the requested file
- `GET /path/to/directory/`: Returns directory listing for the path

## Security Considerations

- File proxy mode includes path traversal protection
- Directory listings can be disabled by serving an `index.html` file
- HTTPS support provides encryption for sensitive data
- Rate limiting helps prevent abuse

## Troubleshooting

### Build Issues
- Ensure vcpkg is properly configured and dependencies are installed
- Check that your compiler supports C++17 features
- Verify CMake version compatibility

### Runtime Issues
- Check that the specified port is not in use
- Ensure the proxy directory exists and is readable
- Verify file permissions for the directory being served

### Connection Issues
- Confirm the server is listening on the correct port
- Check firewall settings if accessing from remote machines
- Verify network connectivity and DNS resolution

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Submit a pull request

## License

[Specify your license here]

## Version History

### v2.0.0 (Latest)
- Removed gRPC dependencies for simplified architecture
- Added command-line configuration
- Implemented file proxy server with directory listing
- Enhanced build system and cross-platform support

### v1.x (Legacy Branch)
- gRPC-based configuration (preserved in `legacy` branch)
- Basic HTTP proxy functionality

