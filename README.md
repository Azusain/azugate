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

### Perf

```shell
  # cmd: wrk -c400 -t4 -d5s http://localhost:5080/welcome.html
  Running 5s test @ http://localhost:5080/welcome.html
    4 threads and 400 connections
    Thread Stats   Avg      Stdev     Max   +/- Stdev
      Latency     7.91ms   16.84ms 113.00ms   90.03%
      Req/Sec    32.60k    12.62k   43.66k    83.33%
    645297 requests in 5.06s, 191.39MB read
  Requests/sec: 127594.74
  Transfer/sec:     37.84MB
```