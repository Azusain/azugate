cmake_minimum_required(VERSION 3.25.0)
project(azugate)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# set(CMAKE_BUILD_TYPE Debug)
# used for distribution.
set(CMAKE_BUILD_TYPE Release)
# Platform-specific compiler flags
if(UNIX)
    # opts only available on Linux.
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -flto -DNDEBUG -s")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
elseif(MSVC)
    # Windows/MSVC flags
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG /bigobj")
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi /bigobj")
else()
    # Other Windows compilers (MinGW, etc.)
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
endif()

find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(Boost COMPONENTS asio thread system url CONFIG)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(jwt-cpp REQUIRED)
find_package(ZLIB REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(cxxopts CONFIG REQUIRED)
enable_testing()

set(COMMON_INCLUDES_DIR "include")
set(THIRD_PARTY_INCLUDES_DIR "third_party")
file(GLOB_RECURSE COMMON_SOURCES "src/common/*.cc")
file(GLOB_RECURSE API_SOURCES "src/api/*.cc")
file(GLOB_RECURSE API_INCLUDES_DIR "src/api")
file(GLOB_RECURSE THIRD_PARTY_SOURCES "third_party/*.cc" "third_party/*.c")

# library for all the apps.
add_library(common STATIC
${COMMON_SOURCES}
${THIRD_PARTY_SOURCES}
${API_SOURCES}
)

target_include_directories(common PUBLIC
${COMMON_INCLUDES_DIR} 
${THIRD_PARTY_INCLUDES_DIR}
${API_INCLUDES_DIR}
)

target_link_libraries(common
spdlog::spdlog
ZLIB::ZLIB
Boost::asio
Boost::thread
Boost::url
yaml-cpp::yaml-cpp
gRPC::grpc++
gRPC::grpc++_reflection
protobuf::libprotobuf
nlohmann_json::nlohmann_json
cxxopts::cxxopts
${OPENSSL_LIBRARIES}
${jwt-cpp_LIBRARIES}
)

# Windows-specific libraries
if(WIN32)
    target_link_libraries(common ws2_32 wsock32)
    # Add missing MSVC runtime symbols for gRPC compatibility
    if(MSVC)
        target_link_libraries(common legacy_stdio_definitions.lib)
    endif()
endif()

# apps.
add_executable(azugate
"src/app/azugate.cc"
)

target_compile_definitions(azugate PRIVATE
  FMT_USE_CONSTEXPR=1
  FMT_CONSTEXPR=constexpr
  SPDLOG_FMT_EXTERNAL
  ENABLE_GRPC_WORKER
)
target_link_libraries(azugate
common
Boost::system
)

# Platform-specific linker flags
if(WIN32 AND MSVC)
    # Add linker flags to handle missing gRPC symbols on Windows
    target_link_options(azugate PRIVATE "/FORCE:UNRESOLVED")
endif()


# Enable gRPC worker for all platforms
target_compile_definitions(common PRIVATE ENABLE_GRPC_WORKER)

# Windows-specific preprocessor definitions and runtime library settings
if(WIN32)
    target_compile_definitions(common PRIVATE
        WIN32_LEAN_AND_MEAN
        _WIN32_WINNT=0x0601
        NOMINMAX
    )
    target_compile_definitions(azugate PRIVATE
        WIN32_LEAN_AND_MEAN
        _WIN32_WINNT=0x0601
        NOMINMAX
    )
    
    # MSVC runtime library consistency - use dynamic runtime to match gRPC
    if(MSVC)
        set_property(TARGET common PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
        set_property(TARGET azugate PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    endif()
endif()


