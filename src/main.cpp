#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <stdexcept>
#include "server/tcp_server.h"
#include "storage/kv_store.h"

static std::atomic<bool> gRunning{true};
static TCPServer*         gServer = nullptr;

void onSignal(int) {
    gRunning = false;
    if (gServer) gServer->stop();
}

int main(int argc, char* argv[]) {
    int port = 6380;
    if (argc > 1) {
        try { port = std::stoi(argv[1]); }
        catch (...) { std::cerr << "Invalid port\n"; return 1; }
    }

    std::signal(SIGINT,  onSignal);
    std::signal(SIGTERM, onSignal);

    std::cout << "+------------------------------+\n"
              << "| C++ KV Store   port=" << port << "     |\n"
              << "+------------------------------+\n";

    try {
        KVStore   kv(65536, 1024);
        TCPServer server(port, kv);
        gServer = &server;
        server.start();

        std::cout << "[Main] Ready. Test with: redis-cli -p " << port << "\n"
                  << "[Main] Commands: PING / SET k v / GET k / DEL k / EXPIRE k s / TTL k\n"
                  << "[Main] Press Ctrl+C to stop.\n";

        while (gRunning)
            std::this_thread::sleep_for(std::chrono::milliseconds(200));

        std::cout << "\n[Main] Shutting down...\n";
    } catch (const std::exception& e) {
        std::cerr << "[Fatal] " << e.what() << "\n";
        return 1;
    }
    return 0;
}