cmake_minimum_required(VERSION 3.25.0)
project(azugate)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Release)

find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(Boost COMPONENTS asio thread CONFIG)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(jwt-cpp REQUIRED)
find_package(ZLIB REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

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
yaml-cpp::yaml-cpp
gRPC::grpc++
protobuf::libprotobuf
nlohmann_json::nlohmann_json
${OPENSSL_LIBRARIES}
${jwt-cpp_LIBRARIES}
)

# apps.
add_executable(azugate
"src/app/azugate.cc"
)

target_link_libraries(azugate
common
)

# only for testing
add_executable(test
"src/app/test.cc"
)
target_link_libraries(test
${OPENSSL_LIBRARIES}
common
)