#!/bin/bash

# AzuGate Installation Script
# Automatically detects OS and installs AzuGate using the appropriate method

set -e

AZUGATE_VERSION="v1.1.1"
GITHUB_REPO="Azusain/azugate"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Detect OS
detect_os() {
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        if [ -f /etc/os-release ]; then
            . /etc/os-release
            OS=$NAME
            VER=$VERSION_ID
        elif type lsb_release >/dev/null 2>&1; then
            OS=$(lsb_release -si)
            VER=$(lsb_release -sr)
        else
            OS="Linux"
            VER="Unknown"
        fi
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        OS="macOS"
        VER=$(sw_vers -productVersion)
    else
        OS="Unknown"
        VER="Unknown"
    fi
}

# Check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Install via Homebrew
install_homebrew() {
    log_info "Installing AzuGate via Homebrew..."
    
    if ! command_exists brew; then
        log_warning "Homebrew not found. Installing Homebrew first..."
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    fi
    
    log_info "Installing AzuGate formula..."
    brew install https://raw.githubusercontent.com/${GITHUB_REPO}/main/Formula/azugate.rb
    
    log_success "AzuGate installed successfully via Homebrew!"
    log_info "Start service: brew services start azugate"
    log_info "View logs: tail -f /usr/local/var/log/azugate.log"
}

# Install via APT (Debian/Ubuntu)
install_apt() {
    log_info "Installing AzuGate via APT (.deb package)..."
    
    # Create temporary directory
    TEMP_DIR=$(mktemp -d)
    cd "$TEMP_DIR"
    
    # Download .deb package
    DEB_FILE="azugate_${AZUGATE_VERSION#v}-1_amd64.deb"
    DOWNLOAD_URL="https://github.com/${GITHUB_REPO}/releases/download/${AZUGATE_VERSION}/${DEB_FILE}"
    
    log_info "Downloading ${DEB_FILE}..."
    curl -LO "$DOWNLOAD_URL"
    
    # Install package
    log_info "Installing package..."
    sudo dpkg -i "$DEB_FILE"
    
    # Fix any dependency issues
    log_info "Fixing dependencies..."
    sudo apt-get update
    sudo apt-get install -f -y
    
    # Clean up
    cd - > /dev/null
    rm -rf "$TEMP_DIR"
    
    log_success "AzuGate installed successfully via APT!"
    log_info "Start service: sudo systemctl start azugate"
    log_info "Enable auto-start: sudo systemctl enable azugate"
    log_info "View logs: sudo journalctl -u azugate -f"
}

# Install manually (binary download)
install_manual() {
    log_info "Installing AzuGate manually (binary download)..."
    
    # Determine architecture
    ARCH=$(uname -m)
    case $ARCH in
        x86_64) ARCH_SUFFIX="x64" ;;
        aarch64|arm64) ARCH_SUFFIX="arm64" ;;
        *) 
            log_error "Unsupported architecture: $ARCH"
            exit 1
            ;;
    esac
    
    # Determine OS suffix for binary
    case "$OS" in
        *Ubuntu*|*Debian*|*"CentOS"*|*"Red Hat"*|*Linux*) 
            OS_SUFFIX="linux"
            ;;
        *macOS*|*Darwin*)
            OS_SUFFIX="macos"
            ;;
        *)
            log_error "Unsupported OS for manual installation: $OS"
            exit 1
            ;;
    esac
    
    # Create temporary directory
    TEMP_DIR=$(mktemp -d)
    cd "$TEMP_DIR"
    
    # Download binary
    BINARY_FILE="azugate-${OS_SUFFIX}-${ARCH_SUFFIX}.tar.gz"
    DOWNLOAD_URL="https://github.com/${GITHUB_REPO}/releases/download/${AZUGATE_VERSION}/${BINARY_FILE}"
    
    log_info "Downloading ${BINARY_FILE}..."
    curl -LO "$DOWNLOAD_URL"
    
    # Extract and install
    log_info "Extracting binary..."
    tar -xzf "$BINARY_FILE"
    
    log_info "Installing to /usr/local/bin/azugate..."
    sudo cp azugate /usr/local/bin/azugate
    sudo chmod +x /usr/local/bin/azugate
    
    # Clean up
    cd - > /dev/null
    rm -rf "$TEMP_DIR"
    
    log_success "AzuGate installed successfully!"
    log_info "Run azugate --help for usage information"
    log_info "Example: azugate --enable-file-proxy -d /path/to/serve"
}

# Main installation logic
main() {
    echo "=================================================="
    echo "         AzuGate Installation Script"
    echo "=================================================="
    
    detect_os
    log_info "Detected OS: $OS $VER"
    
    # Check for installation method preference
    if [[ -n "$1" ]]; then
        case "$1" in
            brew|homebrew)
                install_homebrew
                exit 0
                ;;
            apt|deb)
                install_apt
                exit 0
                ;;
            manual|binary)
                install_manual
                exit 0
                ;;
            *)
                log_error "Unknown installation method: $1"
                log_info "Available methods: brew, apt, manual"
                exit 1
                ;;
        esac
    fi
    
    # Auto-detect best installation method
    if command_exists brew; then
        log_info "Homebrew detected, using brew installation..."
        install_homebrew
    elif [[ "$OS" == *"Ubuntu"* ]] || [[ "$OS" == *"Debian"* ]]; then
        log_info "Debian/Ubuntu detected, using APT installation..."
        install_apt
    elif [[ "$OS" == *"macOS"* ]]; then
        log_info "macOS detected, using Homebrew installation..."
        install_homebrew
    else
        log_info "Using manual installation..."
        install_manual
    fi
    
    echo ""
    echo "=================================================="
    log_success "Installation completed successfully!"
    echo "=================================================="
    echo ""
    log_info "Next steps:"
    echo "  1. Generate config: azugate --generate-config azugate.yaml"
    echo "  2. Start server: azugate -c azugate.yaml"
    echo "  3. Or use file proxy: azugate --enable-file-proxy -d /path/to/serve"
    echo ""
    log_info "Documentation: https://github.com/${GITHUB_REPO}/blob/main/PACKAGING.md"
    log_info "Issues: https://github.com/${GITHUB_REPO}/issues"
}

# Run main function
main "$@"
