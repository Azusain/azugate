# azugate

### Features

- http file proxy
- https
- http gzip compression and chunked transfer
- tcp proxy
- rate limiting
- management via gRPC API or JSON API
- web UI

### Build & Run

You will need a compiler that supports c++20, along with CMake and vcpkg, to build this project. The project builds successfully on both M2 Mac and x86-64 Linux.
 
```shell
  # 1. Build with Cmake and vcpkg.
  mkdir build && cd build
  cmake ..
  cmake --build .
  
  # 2. Modify the provided template: resources/config.yaml
  
  # 3. Run
  ./azugate

```

### Dev Tools
#### wrk

```shell
  # it should be noted that wrk reuses TCP connections.
  # azugate hasn't supported HTTP keep-alive.
  wrk -t1 -c20 -d10s http://localhost:5080
```

#### ab

```shell
  wrk -t1 -c20 -d10s http://localhost:5080
```

#### keycloak
```shell
  https://www.keycloak.org/getting-started/getting-started-docker
```