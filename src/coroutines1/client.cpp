#include "lib.hpp"

#include <algorithm>
#include <chrono>
#include <csignal>
#include <random>
#include <vector>

socket_task client_loop(const char* port_no)
{
   std::minstd_rand0 prng{std::random_device{}()};
   const auto res = co_await async_connect("localhost", port_no);
   if (!res) {
      std::cerr << "Connect failed\n";
      co_return;
   }

   std::cout << "connected with " << res.value() << '\n';

   while (true) {
      // Generate a random amount of bytes to write2
      const auto start_time = std::chrono::steady_clock::now();
      const char num_bytes_to_write = std::uniform_int_distribution<unsigned char>{1, 100}(prng);
      const auto res2 = co_await async_write(res.value(), &num_bytes_to_write, 1);
      if (!res2) {
         std::cerr << "Write 1 failed\n";
         co_return;
      }

      // Generate the random bytes to write
      std::vector<char> to_write(num_bytes_to_write);
      std::ranges::generate(to_write, [&]() { return prng(); });
      const auto res3 = co_await async_write(res.value(), to_write.data(), to_write.size());
      if (!res3) {
         std::cerr << "Write 2 failed\n";
         co_return;
      }

      // Read them back and verify that they're the same
      std::vector<char> read_bytes(num_bytes_to_write);
      const auto res4 = co_await async_read(res.value(), read_bytes.data(), read_bytes.size());
      if (!res4) {
         std::cerr << "Read failed\n";
         co_return;
      }

      const auto durr = std::chrono::steady_clock::now() - start_time;

      if (read_bytes != to_write) {
         std::cerr << "Round trip failed for socket " << res.value() << '\n';
         co_return;
      }
      else {
         std::cout << "Round trip OK for " << res.value() << ", took "
                   << duration_cast<std::chrono::milliseconds>(durr).count() << "ms\n";
      }
   }
}

int main(int argc, const char* argv[])
{
   std::signal(SIGPIPE, SIG_IGN);
   if (argc != 3) {
      std::cerr << "Usage:\n" << argv[0] << " port_number num_connections\n";
      return 2;
   }

   const auto port_no_test = std::atoi(argv[1]);
   if (port_no_test == 0) {
      std::cerr << "Error converting port number\n";
      return 2;
   }

   const auto num_conns = std::atoi(argv[2]);
   if (num_conns <= 0) {
      std::cerr << "Error converting number of connections\n";
      return 2;
   }

   std::vector<socket_task> tasks;
   for (int i = 0; i < num_conns; ++i) {
      tasks.emplace_back(client_loop(argv[1]));
   }
   socket_scheduler(tasks);
}
