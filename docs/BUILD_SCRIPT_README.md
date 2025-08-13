# 🚀 One-Click Build Script for azugate

## Overview

The `build.bash` script is a comprehensive one-click build solution for the azugate project on Linux systems. It automates the entire build process from dependency installation to final binary verification.

## Features

- **🔧 Automatic dependency installation** for Ubuntu/Debian, CentOS/RHEL/Fedora, and Arch Linux
- **📦 vcpkg integration** with fallback to system packages
- **🎨 Colored output** with progress indicators and status messages
- **⚡ Parallel building** using all available CPU cores
- **✅ Build verification** including dependency checking and executable testing
- **🧪 Automated testing** with CTest integration
- **🛡️ Error handling** with proper cleanup and detailed error messages
- **📊 Build timing** and performance metrics

## Quick Start

### Prerequisites

- Linux system (Ubuntu 20.04+, CentOS 8+, Arch Linux, etc.)
- `sudo` access for package installation
- Internet connection for downloading dependencies

### Basic Usage

```bash
# Make the script executable (on Linux)
chmod +x build.bash

# Run the build (installs everything automatically)
./build.bash
```

That's it! The script will:
1. Install all required system dependencies
2. Set up vcpkg and download packages
3. Configure the build with CMake
4. Build azugate with optimal settings
5. Run tests and verify the build
6. Show usage instructions

## Advanced Usage

### Command Line Options

```bash
# Clean build (removes build directory first)
./build.bash --clean

# Use system packages instead of vcpkg
./build.bash --no-vcpkg

# Skip running tests
./build.bash --no-tests

# Debug build instead of Release
./build.bash --debug

# Use specific number of parallel jobs
./build.bash --jobs=8

# Show help
./build.bash --help
```

### Environment Variables

You can also control the build using environment variables:

```bash
# Build configuration
export BUILD_TYPE=Debug           # Release (default) or Debug
export PARALLEL_JOBS=8           # Number of parallel jobs (default: nproc)
export USE_VCPKG=false          # Use system packages instead
export CLEAN_BUILD=true         # Clean before building
export RUN_TESTS=false         # Skip tests

./build.bash
```

## Build Process Details

The script follows this process:

1. **Requirements Check** - Verifies CMake, Git, compilers
2. **System Dependencies** - Installs build tools, libraries
3. **vcpkg Setup** - Clones, bootstraps, configures vcpkg
4. **Build Configuration** - Runs CMake with appropriate settings
5. **Compilation** - Builds with parallel jobs for speed
6. **Verification** - Checks executable, dependencies, basic functionality
7. **Testing** - Runs CTest suite if available
8. **Summary** - Shows build results and usage instructions

## Supported Systems

### Package Managers

- **apt** (Ubuntu, Debian, Linux Mint)
- **yum** (CentOS, RHEL, Fedora)
- **pacman** (Arch Linux, Manjaro)

### Compilers

- **GCC 11+** (preferred for Linux builds)
- **Clang 14+** (alternative option)
- Both support C++20 features required by azugate

## Build Configurations

### Default (vcpkg + Release)

```bash
./build.bash
```

- Uses vcpkg for dependency management
- Release build with optimizations (`-O3 -march=native -flto`)
- Parallel compilation
- Full testing suite

### System Packages Only

```bash
./build.bash --no-vcpkg
```

- Uses system-installed packages
- Faster setup if packages are already installed
- May have version compatibility issues on older systems

### Debug Build

```bash
./build.bash --debug
```

- Debug symbols included
- No optimizations
- Useful for development and debugging

## Output Structure

After a successful build:

```
azugate/
├── build/
│   ├── azugate              # Main executable
│   ├── vcpkg_installed/     # Runtime libraries (if using vcpkg)
│   └── ...                  # Other build artifacts
├── vcpkg/                   # vcpkg installation
├── vcpkg_installed/         # Full vcpkg packages
└── build.bash               # This script
```

## Troubleshooting

### Common Issues

**"CMake version too old"**
```bash
# Install newer CMake manually
wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | sudo tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null
sudo apt-add-repository 'deb https://apt.kitware.com/ubuntu/ focal main'
sudo apt update && sudo apt install cmake
```

**"vcpkg bootstrap failed"**
```bash
# Clean and retry
rm -rf vcpkg
./build.bash --clean
```

**"Permission denied"**
```bash
# Make script executable
chmod +x build.bash
```

**"Package not found"**
```bash
# Update package database
sudo apt update        # Ubuntu/Debian
sudo yum update        # CentOS/RHEL
sudo pacman -Syu       # Arch Linux
```

### Clean Rebuild

If you encounter issues, try a clean rebuild:

```bash
./build.bash --clean
```

This removes:
- `build/` directory
- `vcpkg_installed/` directory
- Cached build files

## Performance Tuning

### Parallel Jobs

```bash
# Use specific number of jobs
./build.bash --jobs=16

# Or set environment variable
export PARALLEL_JOBS=16
./build.bash
```

### Build Type Selection

- **Release**: Optimized for performance (`-O3 -march=native -flto`)
- **Debug**: Optimized for debugging (`-g -O0`)

## Integration with CI/CD

The script is designed to work in CI/CD environments:

```yaml
# GitHub Actions example
- name: Build azugate
  run: |
    chmod +x build.bash
    ./build.bash --no-tests  # Tests run separately in CI
```

## Comparison with Manual Build

| Aspect | Manual Build | build.bash |
|--------|--------------|-------------|
| Setup Time | 30-60 minutes | 5-15 minutes |
| Error Prone | High | Low |
| Reproducible | Depends on user | Always |
| Documentation | Requires reading docs | Self-documented |
| Multi-platform | Manual adaptation | Automatic detection |

## Related Files

- `BUILD.README.md` - Detailed build instructions
- `LINUX_BUILD.md` - Linux-specific build guide
- `CMakeLists.txt` - Build configuration
- `vcpkg.json` - Dependency manifest
- `CMakePresets.json` - CMake presets

## Contributing

To improve the build script:

1. Test on your Linux distribution
2. Report issues with system info
3. Suggest additional package managers
4. Contribute platform-specific fixes

---

**Last Updated**: 2025-08-13  
**Compatible With**: azugate v1.x, Linux builds only  
**Tested On**: Ubuntu 22.04, CentOS 8, Arch Linux
