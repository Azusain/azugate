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

#### buf

```shell
  buf lint
  buf format -w
  buf generate
```

### Perf

```shell

  # CPU: 12th Gen Intel(R) Core(TM) i5-12600K
  # Cores: 2
  # Command: wrk -c400 -t4 -d5s http://172.17.0.2:8080/login/login.html

  # azugate (4 threads):
  Running 5s test @ http://172.17.0.2:8080/login/login.html
    4 threads and 400 connections
    Thread Stats   Avg      Stdev     Max   +/- Stdev
      Latency    11.56ms   14.33ms  64.61ms   81.98%
      Req/Sec    14.31k     3.10k   21.02k    63.00%
    285529 requests in 5.07s, 327.85MB read
  Requests/sec:  56302.33
  Transfer/sec:     64.65MB

  # nginx (4 workers):
  Running 5s test @ http://172.17.0.2:8080/
  4 threads and 400 connections
    Thread Stats   Avg      Stdev     Max   +/- Stdev
      Latency    13.21ms   16.36ms  65.07ms   81.22%
      Req/Sec    15.48k     4.30k   31.03k    70.50%
    307905 requests in 5.05s, 399.65MB read
  Requests/sec:  60924.57
  Transfer/sec:     79.08MB

  # azugate (1 thread):
  Running 1m test @ http://172.17.0.2:8080/login/login.html
    4 threads and 400 connections
    Thread Stats   Avg      Stdev     Max   +/- Stdev
      Latency    12.11ms    8.79ms 100.15ms   70.60%
      Req/Sec     4.07k     1.79k   12.60k    82.57%
    964912 requests in 1.00m, 1.08GB read
    Socket errors: connect 0, read 0, write 0, timeout 1
  Requests/sec:  16057.07
  Transfer/sec:     18.44MB

  # python simple http server (1 thread):
  # cmd:
  # python3 -m http.server --directory ./resources/ 8080
  Running 1m test @ http://172.17.0.2:8080/login/login.html
    4 threads and 400 connections
    Thread Stats   Avg      Stdev     Max   +/- Stdev
      Latency     8.19ms   37.32ms   1.76s    99.28%
      Req/Sec   312.99    215.00     1.24k    71.25%
    30136 requests in 1.00m, 37.62MB read
    Socket errors: connect 67, read 0, write 0, timeout 20
  Requests/sec:    501.57
  Transfer/sec:    641.17KB

  # golang net.http (4 threads):
  Running 5s test @ http://172.17.0.2:8080/login/login.html
    4 threads and 400 connections
    Thread Stats   Avg      Stdev     Max   +/- Stdev
      Latency    29.16ms   39.30ms 389.58ms   83.07%
      Req/Sec     9.96k     4.89k   57.80k    81.00%
    198598 requests in 5.08s, 247.73MB read
  Requests/sec:  39091.40
  Transfer/sec:     48.76MB

```