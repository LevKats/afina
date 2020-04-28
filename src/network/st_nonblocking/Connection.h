#ifndef AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H
#define AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H

#include <afina/execute/Command.h>
#include <cstring>
#include <protocol/Parser.h>
#include <sys/epoll.h>
#include <vector>

namespace Afina {
namespace Network {
namespace STnonblock {

class Connection {
public:
    Connection(int s, std::shared_ptr<Afina::Storage> ps) : _socket(s), ps(ps) {
        std::memset(&_event, 0, sizeof(struct epoll_event));
        _event.data.ptr = this;
    }

    inline bool isAlive() const { return alive; }

    void Start();

protected:
    void OnError();
    void OnClose();
    void DoRead();
    void DoWrite();

private:
    friend class ServerImpl;

    int _socket;
    bool alive;
    struct epoll_event _event;

    char client_buffer[4096];
    Protocol::Parser parser;
    std::unique_ptr<Execute::Command> command_to_execute;
    int readed_bytes;
    std::size_t arg_remains;
    std::string argument_for_command;
    std::shared_ptr<Afina::Storage> ps;

    std::vector<std::string> results;
};

} // namespace STnonblock
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H
