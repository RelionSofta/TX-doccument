#ifdef _WIN32
#define _WIN32_WINNT 0x0601
#endif  
#define ASIO_STANDALONE

#include<iostream>
#include<asio.hpp>
#include<asio/ts/buffer.hpp>
#include<asio/ts/internet.hpp>
#include<thread>
#include"Server.h"


using namespace std;


int main() {

    try
    {
        // 1) Choose worker thread count (common: HW threads)
        unsigned int threadCount =std::max(2u, std::thread::hardware_concurrency());
        if (threadCount == 0) threadCount = 4; // fallback

        asio::io_context io;

        // 2) Work guard keeps io.run() alive even if no pending tasks temporarily
        auto workGuard = asio::make_work_guard(io);

        // 3) Handle Ctrl+C / SIGTERM for clean shutdown
        asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&](const asio::error_code&, int) {
            std::cout << "\n[signal] stopping...\n";
            workGuard.reset(); // allow run() to exit once handlers are done
            io.stop();
            });

        // 4) Start server
        const uint16_t port = 9000;
        auto server = std::make_shared<Server>(io, port);
        server->start_accept();

        std::cout << "Server started on port " << port
            << " with " << threadCount << " worker threads.\n";

        // 5) Run io_context on a thread pool
        std::vector<std::thread> workers;
        workers.reserve(threadCount);

        for (unsigned int i = 0; i < threadCount; ++i)
        {
            workers.emplace_back([&io, i]() {
                try {
                    io.run();
                }
                catch (const std::exception& e) {
                    std::cerr << "[worker " << i << "] exception: " << e.what() << "\n";
                }
                });
        }

        // 6) Optional: main thread can do control work / UI / console
        // Here we just wait until all workers finish (signal stops them)
        for (auto& t : workers) t.join();

        std::cout << "Server stopped.\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
   
        
    return 0;
}
