#ifndef AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H
#define AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H

#include <afina/execute/Command.h>
#include <atomic>
#include <cstring>
#include <protocol/Parser.h>
#include <sys/epoll.h>
#include <vector>

#include <afina/logging/Service.h>
#include <spdlog/logger.h>

namespace Afina {
namespace Network {
namespace MTnonblock {

class Connection {
public:
    Connection(int s, std::shared_ptr<Afina::Storage> ps, std::shared_ptr<Afina::Logging::Service> pl)
        : _socket(s), ps(ps), is_alive(false), is_ready(false) {
        std::memset(&_event, 0, sizeof(struct epoll_event));
        _event.data.ptr = this;
        logger = pl->select("network.connection");
    }

    inline bool isAlive() const { return is_alive.load(std::memory_order_acquire); }

    void Start();

protected:
    void OnError();
    void OnClose();
    void DoRead();
    void DoWrite();

private:
    friend class Worker;
    friend class ServerImpl;

    int _socket;
    struct epoll_event _event;

    // synchronization
    std::atomic<bool> is_alive;
    std::atomic<bool> is_ready;

    // state storage
    char client_buffer[4096];
    Protocol::Parser parser;
    std::unique_ptr<Execute::Command> command_to_execute;
    int readed_bytes;
    std::size_t arg_remains;
    std::string argument_for_command;
    std::shared_ptr<Afina::Storage> ps;

    // logger
    std::shared_ptr<spdlog::logger> logger;

    std::vector<std::string> results;
};

} // namespace MTnonblock
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H
