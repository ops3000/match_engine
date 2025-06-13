# //match_engine/Dockerfile
# Stage 1: Build the engine
FROM ubuntu:22.04 as builder

WORKDIR /app

# Install build dependencies
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    pkg-config \
    librdkafka-dev \
    libssl-dev \
    libsasl2-dev \
    libboost-dev

# Copy source code
COPY . .

# Build the project
RUN mkdir build && cd build && \
    cmake .. && \
    make -j$(nproc)

# Stage 2: Create the runtime image
FROM ubuntu:22.04

WORKDIR /usr/local/bin

# Install runtime dependencies for librdkafka
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    librdkafka1 \
    libssl3 \
    libsasl2-2 && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# Copy the compiled executable from the builder stage
COPY --from=builder /app/bin/exchange_main .

# Set the command to run the engine
CMD ["./exchange_main"]