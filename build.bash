#!/bin/bash

# 🚀 azugate One-Click Build Script - vcpkg + CMake + Ninja ONLY
# No system packages! Only vcpkg for dependencies.
# Prerequisites: cmake (3.25+), ninja, git, gcc/g++, curl

set -euo pipefail

# Colors
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[0;33m'
BLUE='\033[0;34m'; PURPLE='\033[0;35m'; CYAN='\033[0;36m'; NC='\033[0m'

info() { echo -e "${BLUE}ℹ️  $1${NC}"; }
success() { echo -e "${GREEN}✅ $1${NC}"; }
warning() { echo -e "${YELLOW}⚠️  $1${NC}"; }
error() { echo -e "${RED}❌ $1${NC}"; }
step() { echo -e "${PURPLE}🔧 $1${NC}"; }

# Configuration
BUILD_TYPE="${BUILD_TYPE:-Release}"
BUILD_DIR="build"
PARALLEL_JOBS="${PARALLEL_JOBS:-$(nproc)}"
CLEAN_BUILD="${CLEAN_BUILD:-false}"
RUN_TESTS="${RUN_TESTS:-true}"

# Print banner
echo -e "${CYAN}"
cat << "EOF"
    __ _ _____   _  __ _  __ _ | |_  ___ 
   / _` ||_  /  | || |/ _` || __|/ _ \ 
  | (_| | / /   | || | (_| || |_|  __/
   \__,_|/___| _|_||_|\__, | \__|\___|
                      |___/           
  🚀 vcpkg + CMake + Ninja Build Script
EOF
echo -e "${NC}"

info "Build configuration:"
info "  • Build type: $BUILD_TYPE"
info "  • Parallel jobs: $PARALLEL_JOBS"
info "  • Clean build: $CLEAN_BUILD"
info "  • Run tests: $RUN_TESTS"

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to check required tools
check_required_tools() {
    step "Checking required build tools..."
    
    local required_tools=("cmake" "ninja" "git" "gcc" "g++")
    local missing_tools=()
    
    for tool in "${required_tools[@]}"; do
        if ! command_exists "$tool"; then
            missing_tools+=("$tool")
        fi
    done
    
    if [ ${#missing_tools[@]} -ne 0 ]; then
        error "Missing required tools: ${missing_tools[*]}"
        error "Please install the following before running this script:"
        error "  • cmake (3.25+)"
        error "  • ninja"
        error "  • git"
        error "  • gcc/g++ (with C++20 support)"
        error "  • curl (for vcpkg)"
        exit 1
    fi
    
    success "All required tools are available"
}

# Function to setup vcpkg
setup_vcpkg() {
    step "Setting up vcpkg..."
    
    if [ ! -d "vcpkg" ]; then
        info "Cloning vcpkg..."
        git clone https://github.com/Microsoft/vcpkg.git
    else
        info "vcpkg directory exists, pulling latest changes..."
        cd vcpkg
        git pull
        cd ..
    fi
    
    cd vcpkg
    if [ ! -f "vcpkg" ]; then
        info "Bootstrapping vcpkg..."
        ./bootstrap-vcpkg.sh
    else
        info "vcpkg already bootstrapped"
    fi
    cd ..
    
    # Set VCPKG_ROOT environment variable
    export VCPKG_ROOT="$(pwd)/vcpkg"
    info "VCPKG_ROOT set to: $VCPKG_ROOT"
    
    success "vcpkg setup complete"
}

# Function to check build requirements
check_requirements() {
    step "Checking build requirements..."
    
    # Check for required commands
    local required_commands=("cmake" "git")
    if [ "$USE_VCPKG" = "true" ]; then
        required_commands+=("ninja")
    fi
    
    for cmd in "${required_commands[@]}"; do
        if ! command_exists "$cmd"; then
            error "$cmd is not installed"
            exit 1
        fi
    done
    
    # Check CMake version
    local cmake_version=$(cmake --version | head -n1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+')
    local required_cmake="3.25.0"
    if ! printf '%s\n%s\n' "$required_cmake" "$cmake_version" | sort -V -C; then
        warning "CMake version $cmake_version is below required $required_cmake"
        warning "Build may fail or have issues"
    else
        success "CMake version $cmake_version meets requirements"
    fi
    
    # Check for C++20 support
    if command_exists g++; then
        local gcc_version=$(g++ --version | head -n1 | grep -oE '[0-9]+\.[0-9]+')
        info "Found GCC version: $gcc_version"
    fi
    
    if command_exists clang++; then
        local clang_version=$(clang++ --version | head -n1 | grep -oE '[0-9]+\.[0-9]+')
        info "Found Clang version: $clang_version"
    fi
    
    success "Build requirements check complete"
}

# Function to clean build directory
clean_build_dir() {
    if [ "$CLEAN_BUILD" = "true" ]; then
        step "Cleaning build directory..."
        rm -rf "$BUILD_DIR"
        rm -rf vcpkg_installed
        success "Build directory cleaned"
    fi
}

# Function to configure the build
configure_build() {
    step "Configuring build with vcpkg..."
    
    # Create build directory
    mkdir -p "$BUILD_DIR"
    
    # Ensure VCPKG_ROOT is set
    if [ -z "${VCPKG_ROOT:-}" ]; then
        export VCPKG_ROOT="$(pwd)/vcpkg"
    fi
    
    info "Using CMake preset 'default' with vcpkg toolchain..."
    cmake --preset default
    
    success "Build configured successfully"
}

# Function to build the project
build_project() {
    step "Building azugate..."
    
    info "Starting build with $PARALLEL_JOBS parallel jobs..."
    local start_time=$(date +%s)
    
    cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" --parallel "$PARALLEL_JOBS"
    
    local end_time=$(date +%s)
    local build_time=$((end_time - start_time))
    
    success "Build completed in ${build_time}s"
}

# Function to verify build artifacts
verify_build() {
    step "Verifying build artifacts..."
    
    local executable="$BUILD_DIR/azugate"
    
    if [ ! -f "$executable" ]; then
        error "azugate executable not found at $executable"
        exit 1
    fi
    
    # Check if executable is valid
    if file "$executable" | grep -q "executable"; then
        success "azugate executable is valid"
        
        # Get file info
        local file_info=$(file "$executable")
        local file_size=$(stat -c%s "$executable")
        local file_size_mb=$((file_size / 1024 / 1024))
        
        info "Executable details:"
        info "  • Path: $executable"
        info "  • Size: ${file_size_mb}MB ($file_size bytes)"
        info "  • Type: $file_info"
        
        # Check dependencies
        info "Checking runtime dependencies..."
        ldd "$executable" || echo "Static linking or dependencies check failed"
        
        # Test help command
        info "Testing executable..."
        timeout 5s "$executable" --help || warning "Help command test failed or timed out"
        
    else
        error "azugate executable appears to be invalid"
        exit 1
    fi
    
    success "Build verification complete"
}

# Function to run tests
run_tests() {
    if [ "$RUN_TESTS" = "true" ]; then
        step "Running tests..."
        
        cd "$BUILD_DIR"
        if ctest --output-on-failure --parallel "$PARALLEL_JOBS"; then
            success "All tests passed"
        else
            warning "Some tests failed or no tests configured"
        fi
        cd ..
    else
        info "Skipping tests (RUN_TESTS=false)"
    fi
}

# Function to show usage instructions
show_usage() {
    step "Build Summary"
    
    local executable="$BUILD_DIR/azugate"
    
    info "🎉 azugate built successfully!"
    info ""
    info "Usage:"
    info "  • Run: ./$executable"
    info "  • Help: ./$executable --help"
    info "  • With config: ./$executable --config config.yaml"
    info ""
    info "Next steps:"
    info "  1. Copy any required configuration files"
    info "  2. Test the application: cd $BUILD_DIR && ./azugate"
    info "  3. For deployment, copy the executable and any shared libraries"
    info ""
    
    if [ "$USE_VCPKG" = "true" ]; then
        info "vcpkg artifacts:"
        info "  • Installed packages: vcpkg_installed/"
        info "  • Build cache: $BUILD_DIR/vcpkg_installed/"
    fi
    
    info "Build artifacts in: $BUILD_DIR/"
}

# Function to handle cleanup on error
cleanup_on_error() {
    error "Build failed! Check the output above for details."
    exit 1
}

# Set trap for cleanup on error
trap cleanup_on_error ERR

# Main build process
main() {
    local start_time=$(date +%s)
    
    info "Starting azugate build process..."
    
    # Check if we're in the right directory
    if [ ! -f "CMakeLists.txt" ] || [ ! -f "vcpkg.json" ]; then
        error "Please run this script from the azugate project root directory"
        exit 1
    fi
    
    # Execute build steps
    check_required_tools
    setup_vcpkg
    clean_build_dir
    configure_build
    build_project
    verify_build
    run_tests
    show_usage
    
    local end_time=$(date +%s)
    local total_time=$((end_time - start_time))
    
    success "🎉 azugate build completed successfully in ${total_time}s!"
}

# Handle command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --clean)
            CLEAN_BUILD="true"
            shift
            ;;
        --no-vcpkg)
            USE_VCPKG="false"
            shift
            ;;
        --no-tests)
            RUN_TESTS="false"
            shift
            ;;
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --jobs=*)
            PARALLEL_JOBS="${1#*=}"
            shift
            ;;
        --help|-h)
            echo "azugate build script"
            echo ""
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --clean       Clean build directory before building"
            echo "  --no-vcpkg    Use system packages instead of vcpkg"
            echo "  --no-tests    Skip running tests"
            echo "  --debug       Build in Debug mode (default: Release)"
            echo "  --jobs=N      Use N parallel build jobs (default: nproc)"
            echo "  --help        Show this help message"
            echo ""
            echo "Environment variables:"
            echo "  BUILD_TYPE     Build type (Release|Debug)"
            echo "  PARALLEL_JOBS  Number of parallel build jobs"
            echo "  USE_VCPKG      Use vcpkg (true|false)"
            echo "  CLEAN_BUILD    Clean before build (true|false)"
            echo "  RUN_TESTS      Run tests after build (true|false)"
            exit 0
            ;;
        *)
            error "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Run main function
main "$@"
