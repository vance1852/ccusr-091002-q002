# ============================================
# Multi-stage build for Industrial Inspection C++ App
# Supports ARM64 and AMD64
# ============================================

FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    libmysqlclient-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN mkdir -p build && cd build \
    && cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DMYSQL_INCLUDE_DIR=/usr/include/mysql \
    && make -j$(nproc)

# ============================================
# Runtime stage
# ============================================
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    libmysqlclient21 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /app/build/IndustrialInspection .
COPY --from=builder /app/build/IndustrialInspectionTest .
COPY sql/ /docker-entrypoint-initdb.d/

CMD ["./IndustrialInspection"]
