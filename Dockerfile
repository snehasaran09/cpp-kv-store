FROM ubuntu:22.04

RUN apt-get update && \
    apt-get install -y g++ cmake make && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN mkdir -p build && \
    cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc)

EXPOSE 6380
CMD ["./build/kvstore", "6380"]