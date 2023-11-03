#ifndef COROUTINE_LIB_HPP
#define COROUTINE_LIB_HPP

#include <fcntl.h>
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
#include <vector>

struct socket_task {
   struct promise_type;

   using handle_type = std::coroutine_handle<promise_type>;

   struct socket_info {
      short events_to_test;
      int handle = -1;
   };

   struct promise_type {
      // std::exception_ptr exception_;
      socket_info sock_info_;

      void return_void() noexcept {}

      socket_task get_return_object() noexcept
      {
         return socket_task{std::coroutine_handle<promise_type>::from_promise(*this)};
      }

      std::suspend_never initial_suspend() noexcept { return {}; }
      std::suspend_always final_suspend() noexcept { return {}; }

      void unhandled_exception() noexcept {}
   };

   socket_task(socket_task&) = delete;
   socket_task(socket_task&& other) : handle_{other.handle_} { other.handle_ = nullptr; }

   socket_task& operator=(socket_task&) = delete;
   socket_task& operator=(socket_task&& other) noexcept
   {
      if (this != &other) {
         handle_ = other.handle_;
         other.handle_ = nullptr;
      }
      return *this;
   }

   ~socket_task()
   {
      if (handle_) {
         if (handle_.promise().sock_info_.handle != -1) {
            close(handle_.promise().sock_info_.handle);
         }
         handle_.destroy();
      }
   }

   socket_info get_sock_info() const noexcept
   {
      assert(handle_ && !handle_.done());
      return handle_.promise().sock_info_;
   }

   void resume() noexcept
   {
      assert(handle_ && !handle_.done());
      handle_.resume();
   }

   bool done() const noexcept
   {
      if (handle_) {
         return handle_.done();
      }
      return true;
   }

private:
   explicit socket_task(handle_type h) noexcept : handle_{h} {}

   handle_type handle_;
};

auto async_connect(const char* addr, const char* port) noexcept
{
   struct connect_awaiter {
      bool await_ready() noexcept
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
            return true;
         }
         else {
            // Only try connecting with the first address, doing multiple would require
            // much more complicated code and probably another layer of coroutines
            for (auto ptr = result; ptr; ptr = ptr->ai_next) {
               sock_handle = socket(ptr->ai_family, ptr->ai_socktype | SOCK_NONBLOCK, ptr->ai_protocol);
               if (sock_handle == -1) {
                  err = errno;
                  continue;
               }
               const auto res = connect(sock_handle, ptr->ai_addr, ptr->ai_addrlen);
               if (res == -1) {
                  if (errno != EINPROGRESS) {
                     // Some unexpected error occurred, try the next address
                     close(sock_handle);
                     err = errno;
                     continue;
                  }
                  // We're waiting on the connection, can't resume yet
                  freeaddrinfo(result);
                  return false;
               }
               // We connected without error, so we can resume right away
               freeaddrinfo(result);
               return true;
            }
            // if we reach here it means it errored so we can resume right away
            freeaddrinfo(result);
            return true;
         }
      }

      void await_suspend(std::coroutine_handle<socket_task::promise_type> h) noexcept
      {
         h.promise().sock_info_ = {POLLOUT, sock_handle};
      }

      std::expected<int, int> await_resume() noexcept
      {
         if (err == 0) {
            return sock_handle;
         }
         return std::unexpected(err);
      }

      int sock_handle;
      int err;
      const char* addr;
      const char* port;
   };
   return connect_awaiter{-1, 0, addr, port};
}

auto async_read(int sock_handle, char* buffer, std::size_t buf_size) noexcept
{
   struct read_awaiter {
      bool await_ready() noexcept { return try_read(); }

      void await_suspend(std::coroutine_handle<socket_task::promise_type> h) noexcept
      {
         h.promise().sock_info_ = {POLLIN, sock_handle};
      }

      std::expected<int, int> await_resume() noexcept
      {
         if (!read_done) {
            const bool result = try_read();
            assert(result);
            (void)result;
         }
         if (err == 0) {
            return num_bytes_read;
         }
         return std::unexpected(err);
      }

      bool try_read() noexcept
      {
         num_bytes_read = read(sock_handle, buffer, buf_size);
         if (num_bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            err = errno;
            read_done = true;
            return true;
         }
         read_done = num_bytes_read >= 0;
         return read_done;
      }

      bool read_done;
      int sock_handle;
      int err;
      int num_bytes_read;
      std::size_t buf_size;
      char* buffer;
   };

   return read_awaiter{false, sock_handle, 0, 0, buf_size, buffer};
}

auto async_write(int sock_handle, const char* buffer, std::size_t buf_size) noexcept
{
   struct write_awaiter {
      bool await_ready() noexcept { return try_write(); }

      void await_suspend(std::coroutine_handle<socket_task::promise_type> h) noexcept
      {
         h.promise().sock_info_ = {POLLOUT, sock_handle};
      }

      std::expected<int, int> await_resume() noexcept
      {
         if (!write_done) {
            const auto result = try_write();
            assert(result);
            (void)result;
         }
         if (err == 0) {
            return num_bytes_written;
         }
         return std::unexpected(err);
      }

      bool try_write() noexcept
      {
         num_bytes_written = write(sock_handle, buffer, buf_size);
         if (num_bytes_written < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            err = errno;
            write_done = true;
            return true;
         }
         write_done = num_bytes_written >= 0;
         return write_done;
      }

      bool write_done;
      int sock_handle;
      int err;
      int num_bytes_written;
      std::size_t buf_size;
      const char* buffer;
   };

   return write_awaiter{false, sock_handle, 0, 0, buf_size, buffer};
}

auto async_accept(int sock_handle) noexcept
{
   struct accept_awaiter {
      bool await_ready() noexcept { return try_accept(); }

      void await_suspend(std::coroutine_handle<socket_task::promise_type> h) noexcept
      {
         h.promise().sock_info_ = {POLLIN, sock_handle};
      }

      std::expected<int, int> await_resume() noexcept
      {
         if (!accept_done) {
            const auto result = try_accept();
            assert(result);
            (void)result;
         }
         if (err == 0) {
            return new_socket_handle;
         }
         return std::unexpected(err);
      }

      bool try_accept() noexcept
      {
         new_socket_handle = accept(sock_handle, nullptr, nullptr);
         if (new_socket_handle < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            err = errno;
            accept_done = true;
            return true;
         }
         accept_done = new_socket_handle >= 0;
         if (accept_done) {
            // set non-blocking
            const int flags = fcntl(new_socket_handle, F_GETFL, 0);
            fcntl(new_socket_handle, F_SETFL, flags | O_NONBLOCK);
         }
         return accept_done;
      }

      bool accept_done;
      int sock_handle;
      int new_socket_handle;
      int err;
   };

   return accept_awaiter{false, sock_handle, -1, 0};
}

inline void socket_scheduler(std::vector<socket_task>& tasks) noexcept
{
   while (!tasks.empty()) {
      std::vector<pollfd> poll_infos;
      for (const auto& task : tasks) {
         const auto info = task.get_sock_info();
         pollfd poll_info;
         poll_info.fd = info.handle;
         poll_info.events = info.events_to_test;
         poll_infos.push_back(poll_info);
      }
      poll(poll_infos.data(), poll_infos.size(), -1);
      for (std::size_t i = 0; i < poll_infos.size(); ++i) {
         const auto& poll_info = poll_infos[i];
         const auto info = tasks[i].get_sock_info();
         if (
            (poll_info.revents & info.events_to_test) != 0 || (poll_info.revents & POLLERR) != 0
            || (poll_info.revents & POLLHUP) != 0 || (poll_info.revents & POLLNVAL) != 0) {
            tasks[i].resume();
         }
      }
      std::erase_if(tasks, [](const auto& task) { return task.done(); });
   }
}

#endif // COROUTINE_LIB_HPP
