#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cassert>
#include <coroutine>
#include <cstdint>
#include <cstring>
#include <exception>
#include <expected>
#include <iostream>

struct connect_awaiter;
struct read_awaiter;
struct write_awaiter;

struct socket_task {
   struct promise_type;

   using handle_type = std::coroutine_handle<promise_type>;

   struct promise_type {
      std::exception_ptr exception_;
      // This is probably a bad way to handle this, but it works
      // and this is mainly for learning how to make coroutines work at all
      connect_awaiter* connect_handle_ = nullptr;
      read_awaiter* read_handle_ = nullptr;
      write_awaiter* write_handle_ = nullptr;

      void return_void() {}

      socket_task get_return_object() { return socket_task{std::coroutine_handle<promise_type>::from_promise(*this)}; }

      std::suspend_always initial_suspend() noexcept { return {}; }

      std::suspend_always final_suspend() noexcept { return {}; }

      void unhandled_exception() { exception_ = std::current_exception(); }
   };

   ~socket_task()
   {
      if (handle_) {
         handle_.destroy();
      }
   }

   void resume() noexcept
   {
      if (handle_ && !handle_.done()) {
         handle_.resume();
      }
   }

   auto handle() { return handle_; }

private:
   explicit socket_task(handle_type h) : handle_{h} {}

   handle_type handle_;
};

struct connect_awaiter {
   int sock_handle;
   int err;
   const char* addr;
   const char* port;

   bool await_ready() noexcept { return false; }

   void await_suspend(std::coroutine_handle<socket_task::promise_type> h) noexcept
   {
      addrinfo hints;
      std::memset(&hints, 0, sizeof(hints));
      hints.ai_family = AF_UNSPEC;
      hints.ai_socktype = SOCK_STREAM;
      hints.ai_protocol = IPPROTO_IP;
      addrinfo* result;
      const auto resaddr = getaddrinfo(addr, port, &hints, &result);
      if (resaddr != 0) {
         err = resaddr;
      }
      else {
         // This only considers the first addresss that doesn't fail right away
         // Changing that is an issue that will not be solved
         for (auto ptr = result; ptr; ptr = ptr->ai_next) {
            sock_handle = ::socket(ptr->ai_family, ptr->ai_socktype | SOCK_NONBLOCK, ptr->ai_protocol);
            if (sock_handle == -1) {
               err = errno;
               continue;
            }
            else {
               const auto res = ::connect(sock_handle, ptr->ai_addr, ptr->ai_addrlen);
               if (res == -1) {
                  if (errno != EINPROGRESS) {
                     close(sock_handle);
                     err = errno;
                     continue;
                  }
                  h.promise().connect_handle_ = this;
                  break;
               }
            }
         }
         freeaddrinfo(result);
      }
   }

   bool is_ready() noexcept
   {
      assert(sock_handle != -1);
      pollfd poll_info;
      poll_info.fd = sock_handle;
      poll_info.events = POLLOUT;
      poll(&poll_info, 1, 0);
      if ((poll_info.revents & POLLOUT) != 0) {
         int error;
         socklen_t errsize = sizeof(error);
         getsockopt(sock_handle, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &errsize);
         err = error;
         return true;
      }
      return false;
   }

   std::expected<int, int> await_resume() noexcept
   {
      if (err == 0) {
         return sock_handle;
      }
      return std::unexpected(err);
   }
};

connect_awaiter async_connect(const char* addr, const char* port) noexcept
{
   return {.sock_handle = -1, .err = 0, .addr = addr, .port = port};
}

struct read_awaiter {
   int sock_handle;
   char* buffer;
   std::size_t buf_size;
   int err;
   int num_bytes;

   bool await_ready() noexcept
   {
      retry_read();
      return false;
   }

   void await_suspend(std::coroutine_handle<socket_task::promise_type> h) noexcept { h.promise().read_handle_ = this; }

   std::expected<int, int> await_resume() noexcept
   {
      if (err == 0) {
         return num_bytes;
      }
      return std::unexpected(err);
   }

   void retry_read() noexcept
   {
      num_bytes = read(sock_handle, buffer, buf_size);
      if (num_bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
         err = num_bytes;
      }
   }

   bool is_ready() noexcept { return num_bytes >= 0 || err != 0; }
};

read_awaiter async_read(int sock_handle, char* buffer, std::size_t buf_size) noexcept
{
   return {.sock_handle = sock_handle, .buffer = buffer, .buf_size = buf_size, .err = 0, .num_bytes = 0};
}

struct write_awaiter {
   int sock_handle;
   const char* buffer;
   std::size_t buf_size;
   int err;
   int num_bytes;

   bool await_ready() noexcept
   {
      retry_write();
      return false;
   }

   void await_suspend(std::coroutine_handle<socket_task::promise_type> h) noexcept { h.promise().write_handle_ = this; }

   void retry_write() noexcept
   {
      num_bytes = write(sock_handle, buffer, buf_size);
      if (num_bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
         err = num_bytes;
      }
   }

   std::expected<int, int> await_resume() noexcept
   {
      if (err == 0) {
         return num_bytes;
      }
      return std::unexpected(err);
   }

   bool is_ready() noexcept { return num_bytes >= 0 || err != 0; }
};

write_awaiter async_write(int sock_handle, const char* buffer, std::size_t buf_size) noexcept
{
   return write_awaiter{.sock_handle = sock_handle, .buffer = buffer, .buf_size = buf_size, .err = 0, .num_bytes = 0};
}

socket_task test_async()
{
   const auto res = co_await async_connect("example.com", "80");
   if (res) {
      const char get_request[]
         = "GET / HTTP/1.1\r\n"
           "Host: example.com\r\n"
           "User-Agent: coroutine-test\r\n"
           "Connection: close\r\n"
           "\r\n";
      const auto res2 = co_await async_write(res.value(), get_request, std::size(get_request));
      if (res2) {
         std::array<char, 2048> buffer;
         const auto res3 = co_await async_read(res.value(), buffer.data(), buffer.size());
         if (res3) {
            std::cout.write(buffer.data(), res3.value());
            std::cout << '\n';
         }
      }
   }
}

int main()
{
   auto h = test_async();
   h.resume();
   while (!h.handle().promise().connect_handle_->is_ready()) {}
   h.resume();
   while (!h.handle().promise().write_handle_->is_ready()) {
      h.handle().promise().write_handle_->retry_write();
   }
   h.resume();
   while (!h.handle().promise().read_handle_->is_ready()) {
      h.handle().promise().read_handle_->retry_read();
   }
   h.resume();
}
