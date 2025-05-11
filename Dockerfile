# ============== Build ==============
FROM ubuntu:latest AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    curl \
    pkg-config \
    zip \
    unzip

# Install vcpkg.
WORKDIR /opt
RUN git clone https://github.com/microsoft/vcpkg.git \
    && ./vcpkg/bootstrap-vcpkg.sh
ENV VCPKG_ROOT=/opt/vcpkg
ENV PATH="${VCPKG_ROOT}:${PATH}"

WORKDIR /project

# Build project.
COPY . .
RUN mkdir build \
    && cd build \
    && cmake --preset=default -DCMAKE_BUILD_TYPE=Release -DVCPKG_BUILD_TYPE=release ..
RUN cd build && cmake --build .

# ============== Runtime ==============
FROM ubuntu:latest
RUN apt-get update && apt-get install -y libstdc++6 && rm -rf /var/lib/apt/lists/*
COPY --from=builder /project/build/azugate /app/bin/azugate
COPY --from=builder /project/resources /app/resources
WORKDIR /app/bin
EXPOSE 8080 50051
ENTRYPOINT ["./azugate"]