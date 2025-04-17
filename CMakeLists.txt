cmake_minimum_required(VERSION 3.25.0)
project(azugate)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_BUILD_TYPE Debug)
# used for distribution.
# set(CMAKE_BUILD_TYPE Release)
# set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -flto -DNDEBUG -s")
# set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

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
${OPENSSL_LIBRARIES}
${jwt-cpp_LIBRARIES}
)

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


# I/O Perf (uses kqueue thus cannot run on Linux).
# add_executable(io_perf
# "src/app/io_perf.cc"
# )
# target_link_libraries(io_perf
# ${OPENSSL_LIBRARIES}
# common
# )

# Perf.
add_executable(azugate_
"src/app/azugate_.cc"
)
target_link_libraries(azugate_
${OPENSSL_LIBRARIES}
common
Boost::system
)
