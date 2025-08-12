# 🏗️ Build Instructions for azugate Project

## Prerequisites

### Required Software
1. **Visual Studio 2022** (or Build Tools 2022)
   - Install via: `winget install Microsoft.VisualStudio.2022.BuildTools`
   - Or download from: https://visualstudio.microsoft.com/downloads/

2. **vcpkg Package Manager**
   - Clone: `git clone https://github.com/Microsoft/vcpkg.git`
   - Bootstrap: `.\vcpkg\bootstrap-vcpkg.bat`
   - Integrate: `.\vcpkg\vcpkg integrate install`

3. **CMake** (3.20 or later)
   - Install via Visual Studio installer or standalone

4. **Git** (for vcpkg and source control)

## Project Setup

### 1. Clone the Repository
```bash
git clone <your-repo-url>
cd azugate
```

### 2. vcpkg Configuration
The project uses **vcpkg manifest mode** with local installation. This is why dependencies are installed in your project folder rather than globally.

**Why local installation?**
- **Isolation**: Each project has its own dependency versions
- **Reproducibility**: Anyone can build with exact same dependencies
- **Version Control**: Can be committed for exact reproducibility
- **No Global Pollution**: Doesn't affect other projects

The `vcpkg-configuration.json` specifies:
```json
{
  "default-triplet": "x64-windows",  // Dynamic linking for Windows
  "default-registry": {
    "kind": "git",
    "baseline": "cd124b84feb0c02a24a2d90981e8358fdee0e077",
    "repository": "https://github.com/microsoft/vcpkg"
  }
}
```

The `vcpkg.json` manifest defines dependencies:
```json
{
  "dependencies": [
    "grpc",
    "boost-beast",
    "spdlog",
    "yaml-cpp"
  ]
}
```

### 3. Build Process

#### Option A: Using CMake Presets (Recommended)
```bash
# Configure the project
cmake --preset windows-vs

# Build the project
cmake --build build --config Release
```

#### Option B: Manual CMake Configuration
```bash
# Create build directory
mkdir build
cd build

# Configure with vcpkg
cmake .. -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build . --config Release
```

## Build Artifacts

After successful build, you'll find:

### Main Executable
- `build/Release/azugate.exe` - Main application

### Runtime Dependencies
- `build/vcpkg_installed/x64-windows/bin/*.dll` - Required DLLs
- `vcpkg_installed/x64-windows/bin/*.dll` - Full development DLLs

### Directory Structure After Build
```
azugate/
├── build/                          # CMake build output
│   ├── Release/
│   │   └── azugate.exe            # Main executable
│   └── vcpkg_installed/           # Runtime DLLs (cleaned)
│       └── x64-windows/bin/
├── vcpkg_installed/               # Full vcpkg installation (9.84GB)
│   ├── x64-windows/
│   │   ├── bin/                   # Runtime DLLs
│   │   ├── lib/                   # Static libraries
│   │   ├── include/               # Header files
│   │   ├── share/                 # CMake configs
│   │   └── debug/                 # Debug binaries
│   └── vcpkg/                     # vcpkg metadata
└── ...
```

## Why Two vcpkg_installed Directories?

1. **`vcpkg_installed/` (Project Root)**: Full development installation
   - Contains headers, libraries, debug symbols, documentation
   - Used for development and rebuilding
   - Size: ~9.84GB

2. **`build/vcpkg_installed/` (Build Output)**: Runtime-only subset
   - Contains only essential DLLs for execution
   - Cleaned for deployment
   - Size: ~280MB

## Running the Application

### From Build Directory
```bash
cd build/Release
./azugate.exe
```

### Expected Output
```
[info] gRPC server is listening on port 50051
[info] azugate is listening on port 8080
[info] Health check will be performed every 3 seconds
[info] server is running with 4 thread(s)
```

## Deployment

For deployment, you need:
1. `azugate.exe`
2. DLLs from `build/vcpkg_installed/x64-windows/bin/`
3. Any configuration files your application uses

## Troubleshooting

### Common Issues

1. **"vcpkg not found"**
   - Ensure vcpkg is properly integrated: `vcpkg integrate install`
   - Check VCPKG_ROOT environment variable

2. **Compiler version mismatch**
   - Update Visual Studio to latest version
   - Clear build directory and reconfigure

3. **Missing DLLs at runtime**
   - Copy DLLs from `vcpkg_installed/x64-windows/bin/` to executable directory
   - Or add the DLL path to your system PATH

4. **Link errors with gRPC**
   - Ensure you're using `x64-windows` triplet (not `x64-windows-static`)
   - Check that Visual Studio toolchain matches vcpkg compiler

### Clean Rebuild
```bash
# Remove build directory
rm -rf build

# Remove vcpkg cache (if needed)
rm -rf vcpkg_installed

# Reconfigure and rebuild
cmake --preset windows-vs
cmake --build build --config Release
```

## Project Dependencies

- **gRPC**: Remote procedure call framework
- **Boost.Beast**: HTTP/WebSocket library
- **spdlog**: Fast C++ logging library
- **yaml-cpp**: YAML parser and emitter
- **OpenSSL**: Cryptography library (transitive dependency)
- **Protobuf**: Serialization library (via gRPC)

## Build Configuration

The project uses:
- **Triplet**: `x64-windows` (dynamic linking)
- **Runtime**: MultiThreadedDLL (release), MultiThreadedDebugDLL (debug)
- **Compiler**: MSVC 2022 (14.43+)
- **Standard**: C++17
- **Architecture**: x64

---
*Last Updated: 2025-08-13*
*Platform: Windows 11, Visual Studio 2022*
