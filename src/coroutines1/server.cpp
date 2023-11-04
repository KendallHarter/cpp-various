#include "lib.hpp"

#include <csignal>
#include <vector>

socket_task server_task(int sock_handle)
{
   while (true) {
      // Read 1 byte that's the number of bytes to read
      char bytes_to_read_raw;
      const auto res1 = co_await async_read(sock_handle, &bytes_to_read_raw, 1);
      if (!res1) {
         co_return;
      }

      // Read that many bytes now
      const unsigned char bytes_to_read = bytes_to_read_raw;
      std::vector<char> buffer(bytes_to_read);
      const auto res2 = co_await async_read(sock_handle, buffer.data(), buffer.size());
      if (!res2) {
         co_return;
      }

      // Write those bytes back
      const auto res3 = co_await async_write(sock_handle, buffer.data(), buffer.size());
      if (!res3) {
         co_return;
      }
   }
}

socket_task server_accept_loop(int socket_handle, std::vector<socket_task>& tasks)
{
   while (true) {
      const auto result = co_await async_accept(socket_handle);
      if (!result) {
         std::cerr << "Accepting errored with " << result.error() << "\n";
      }
      else {
         tasks.push_back(server_task(result.value()));
      }
   }
}

int main(int argc, const char* argv[])
{
   std::signal(SIGPIPE, SIG_IGN);
   if (argc != 2) {
      std::cerr << "Usage:\n" << argv[0] << " port_number\n";
      return 2;
   }
   const auto port_no = std::atoi(argv[1]);
   if (port_no <= 0) {
      std::cerr << "Error parsing port number\n";
      return 2;
   }

   constexpr int max_listen_queue = 50;
   const int listen_socket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
   if (listen_socket < 0) {
      std::cerr << "Creating socket failed\n";
      return 1;
   }
   sockaddr_in addr;
   std::memset(&addr, 0, sizeof(addr));
   addr.sin_family = AF_INET;
   addr.sin_addr.s_addr = htonl(INADDR_ANY);
   addr.sin_port = htons(port_no);

   const auto bind_res = bind(listen_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
   if (bind_res < 0) {
      std::cerr << "Binding socket failed\n";
      return 1;
   }

   const auto listen_res = listen(listen_socket, max_listen_queue);
   if (listen_res < 0) {
      std::cerr << "Listen socket failed\n";
      return 1;
   }

   std::vector<socket_task> tasks;
   tasks.push_back(server_accept_loop(listen_socket, tasks));
   socket_scheduler(tasks);
}
