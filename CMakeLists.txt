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
find_package(jwt-cpp REQUIRED)
find_package(ZLIB REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(cxxopts CONFIG REQUIRED)
enable_testing()

set(COMMON_INCLUDES_DIR "include")
set(THIRD_PARTY_INCLUDES_DIR "third_party")
file(GLOB_RECURSE COMMON_SOURCES "src/common/*.cc")
file(GLOB_RECURSE THIRD_PARTY_SOURCES "third_party/*.cc" "third_party/*.c")

# library for all the apps.
add_library(common STATIC
${COMMON_SOURCES}
${THIRD_PARTY_SOURCES}
)

target_include_directories(common PUBLIC
${COMMON_INCLUDES_DIR} 
${THIRD_PARTY_INCLUDES_DIR}
)

target_link_libraries(common
spdlog::spdlog
ZLIB::ZLIB
Boost::asio
Boost::thread
Boost::url
yaml-cpp::yaml-cpp
nlohmann_json::nlohmann_json
cxxopts::cxxopts
${OPENSSL_LIBRARIES}
${jwt-cpp_LIBRARIES}
)

# Windows-specific libraries
if(WIN32)
    target_link_libraries(common ws2_32 wsock32 psapi)
endif()

# apps.
add_executable(azugate
"src/app/azugate.cc"
)

target_compile_definitions(azugate PRIVATE
  FMT_USE_CONSTEXPR=1
  FMT_CONSTEXPR=constexpr
  SPDLOG_FMT_EXTERNAL
)
target_link_libraries(azugate
common
Boost::system
)


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
    
    # MSVC runtime library consistency
    if(MSVC)
        set_property(TARGET common PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
        set_property(TARGET azugate PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    endif()
endif()

# Installation configuration
set(CMAKE_INSTALL_PREFIX "/usr/local" CACHE PATH "Install path prefix, prepended onto install directories.")

# Install binary
install(TARGETS azugate
    RUNTIME DESTINATION bin
    COMPONENT Runtime
)

# Install default configuration
install(FILES config.default.yaml
    DESTINATION etc/azugate
    RENAME azugate.yaml
    COMPONENT Configuration
)

# Install documentation
install(FILES README.md LICENSE
    DESTINATION share/doc/azugate
    COMPONENT Documentation
)

# Install man page (if exists)
if(EXISTS "${CMAKE_SOURCE_DIR}/docs/azugate.1")
    install(FILES docs/azugate.1
        DESTINATION share/man/man1
        COMPONENT Documentation
    )
endif()

# Create systemd service directory on Linux
if(UNIX AND NOT APPLE)
    install(DIRECTORY
        DESTINATION lib/systemd/system
        COMPONENT SystemdService
    )
endif()

# CPack configuration for creating packages
set(CPACK_PACKAGE_NAME "azugate")
set(CPACK_PACKAGE_VERSION "1.1.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "An unsafe and inefficient gateway")
set(CPACK_PACKAGE_DESCRIPTION "AzuGate is a high-performance reverse proxy and load balancer with file serving capabilities.")
set(CPACK_PACKAGE_VENDOR "AzuGate Project")
set(CPACK_PACKAGE_CONTACT "your-email@example.com")

# Package-specific settings
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libstdc++6, libssl3")
set(CPACK_DEBIAN_PACKAGE_SECTION "net")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")

set(CPACK_RPM_PACKAGE_GROUP "Applications/Internet")
set(CPACK_RPM_PACKAGE_REQUIRES "glibc, libstdc++, openssl-libs")

include(CPack)

