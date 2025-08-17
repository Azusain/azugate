# AzuGate Package Installation Guide

This document provides comprehensive instructions for installing AzuGate using various package managers and building from source.

## Quick Installation

### Homebrew (macOS/Linux)

```bash
# Add the AzuGate tap (once available)
brew tap azusain/azugate
brew install azugate

# Or install directly from formula URL
brew install https://raw.githubusercontent.com/Azusain/azugate/main/Formula/azugate.rb
```

### APT (Ubuntu/Debian)

```bash
# Download and install the .deb package from GitHub releases
wget https://github.com/Azusain/azugate/releases/latest/download/azugate_1.0.0-1_amd64.deb
sudo dpkg -i azugate_1.0.0-1_amd64.deb
sudo apt-get install -f  # Fix any missing dependencies
```

### Manual Installation (All Platforms)

```bash
# Download pre-built binaries from GitHub releases
curl -LO https://github.com/Azusain/azugate/releases/latest/download/azugate-linux-x64.tar.gz
tar -xzf azugate-linux-x64.tar.gz
sudo cp azugate /usr/local/bin/
```

## Package Manager Details

### Homebrew

The Homebrew formula provides:
- Automatic dependency resolution
- Service management with `brew services`
- Configuration file management
- Easy updates with `brew upgrade`

**Installation:**
```bash
brew install azugate
```

**Configuration:**
- Config file: `/usr/local/etc/azugate/azugate.yaml`
- Logs: `/usr/local/var/log/azugate.log`

**Service Management:**
```bash
# Start service
brew services start azugate

# Stop service  
brew services stop azugate

# Restart service
brew services restart azugate

# View logs
tail -f /usr/local/var/log/azugate.log
```

### APT (Debian/Ubuntu)

The Debian package provides:
- System integration with systemd
- User and group creation
- Automatic service startup
- Configuration file protection

**Installation:**
```bash
# Install from .deb file
sudo dpkg -i azugate_1.0.0-1_amd64.deb
sudo apt-get install -f

# Or build and install locally
sudo apt-get install build-essential cmake ninja-build
dpkg-buildpackage -b
sudo dpkg -i ../azugate_1.0.0-1_amd64.deb
```

**Configuration:**
- Config file: `/etc/azugate/azugate.yaml`
- User: `azugate`
- Logs: `/var/log/azugate/` and `journalctl -u azugate`

**Service Management:**
```bash
# Start service
sudo systemctl start azugate

# Enable auto-start
sudo systemctl enable azugate

# Check status
sudo systemctl status azugate

# View logs
sudo journalctl -u azugate -f

# Reload configuration
sudo systemctl reload azugate
```

## Building from Source

### Prerequisites

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    libssl-dev \
    zlib1g-dev \
    libboost-all-dev \
    libyaml-cpp-dev \
    nlohmann-json3-dev \
    libspdlog-dev \
    libfmt-dev \
    git
```

**macOS:**
```bash
brew install cmake ninja boost yaml-cpp nlohmann-json spdlog fmt cxxopts openssl@3
```

**Windows:**
```powershell
# Install vcpkg and dependencies (see GitHub Actions workflow for details)
```

### Build Process

```bash
# Clone repository
git clone https://github.com/Azusain/azugate.git
cd azugate

# Install vcpkg dependencies
git clone https://github.com/Microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh  # or .bat on Windows
./vcpkg/vcpkg install cxxopts jwt-cpp gtest

# Configure and build
cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake \
    -G Ninja

cmake --build build --config Release

# Install system-wide
sudo cmake --install build
```

### Creating Packages

**Debian Package:**
```bash
# Install packaging tools
sudo apt-get install debhelper dh-systemd

# Build package
dpkg-buildpackage -b --no-sign

# Install package
sudo dpkg -i ../azugate_1.0.0-1_amd64.deb
```

**Generic Package (CPack):**
```bash
cd build
cpack -G TGZ  # Create .tar.gz
cpack -G DEB  # Create .deb (Linux only)
cpack -G ZIP  # Create .zip (Windows)
```

## Configuration

### Default Configuration

AzuGate comes with a default configuration that can be generated:

```bash
# Generate default config
azugate --generate-config azugate.yaml

# Validate configuration
azugate --validate-config azugate.yaml
```

### Configuration Locations

| Platform | Config File | User | Logs |
|----------|-------------|------|------|
| Homebrew | `/usr/local/etc/azugate/azugate.yaml` | Current user | `/usr/local/var/log/azugate.log` |
| APT | `/etc/azugate/azugate.yaml` | `azugate` | `journalctl -u azugate` |
| Manual | `./azugate.yaml` | Current user | stdout/stderr |

### Basic Configuration

```yaml
server:
  port: 8080
  enable_https: false

routes:
  - match: "/*"
    target: "http://localhost:3000"

file_proxy:
  enabled: true
  directory: "/var/www/html"
  enable_directory_listing: true

logging:
  level: "info"
  file: "/var/log/azugate/azugate.log"
```

## Troubleshooting

### Common Issues

**Permission Denied:**
```bash
# Check file permissions
ls -la /etc/azugate/azugate.yaml
sudo chown root:azugate /etc/azugate/azugate.yaml
sudo chmod 640 /etc/azugate/azugate.yaml
```

**Service Won't Start:**
```bash
# Check service status
sudo systemctl status azugate

# Check logs
sudo journalctl -u azugate -n 50

# Validate configuration
azugate --validate-config /etc/azugate/azugate.yaml
```

**Dependency Issues (APT):**
```bash
# Fix missing dependencies
sudo apt-get update
sudo apt-get install -f

# Check dependencies
dpkg -I azugate_1.0.0-1_amd64.deb
```

**Homebrew Issues:**
```bash
# Update Homebrew
brew update

# Reinstall
brew uninstall azugate
brew install azugate

# Check for conflicts
brew doctor
```

### Debug Mode

Run AzuGate in debug mode for troubleshooting:

```bash
# Stop service
sudo systemctl stop azugate  # APT
brew services stop azugate   # Homebrew

# Run manually in debug mode
sudo -u azugate azugate -c /etc/azugate/azugate.yaml --log-level=debug
```

## Package Maintenance

### For Maintainers

**Updating Homebrew Formula:**
1. Update version and URL in `Formula/azugate.rb`
2. Calculate new SHA256: `shasum -a 256 azugate-1.0.0.tar.gz`
3. Test formula: `brew install --build-from-source ./Formula/azugate.rb`
4. Submit PR to homebrew-core or maintain in custom tap

**Updating Debian Package:**
1. Update `debian/changelog` with new version
2. Update dependencies in `debian/control` if needed
3. Build and test: `dpkg-buildpackage -b`
4. Upload to package repository or GitHub releases

**Creating New Release:**
1. Tag release: `git tag -a v1.0.1 -m "Release v1.0.1"`
2. Push tags: `git push origin --tags`
3. GitHub Actions will automatically build and create release
4. Update package manager formulas with new checksums

### Automated Releases

The project uses GitHub Actions for automated releases:
- Triggered on version tags (`v*`)
- Builds for Linux, macOS, and Windows
- Creates GitHub release with artifacts
- Can trigger package manager updates

## Support

For installation issues:
1. Check this documentation
2. Search existing [GitHub Issues](https://github.com/Azusain/azugate/issues)
3. Create new issue with:
   - Operating system and version
   - Installation method attempted
   - Full error messages
   - Configuration file (sanitized)

For package-specific issues:
- **Homebrew**: Check `brew doctor` output
- **APT**: Include `dpkg -l | grep azugate` output
- **Build from source**: Include full build logs
