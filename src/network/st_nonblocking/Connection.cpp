#include "Connection.h"
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

#include <iostream>

namespace Afina {
namespace Network {
namespace STnonblock {

// See Connection.h
void Connection::Start() {
    std::cout << "Start" << std::endl;
    alive = true;
    parser.Reset();
    command_to_execute.reset();
    readed_bytes = -1;
    _event.events = EPOLLIN;
}

// See Connection.h
void Connection::OnError() {
    std::cout << "OnError" << std::endl;
    alive = false;
    _event.events = EPOLLERR;
}

// See Connection.h
void Connection::OnClose() {
    std::cout << "OnClose" << std::endl;
    alive = false;
    _event.events = EPOLLRDHUP;
}

// See Connection.h
void Connection::DoRead() {
    std::cout << "DoRead" << std::endl;

    readed_bytes = read(_socket, client_buffer, sizeof(client_buffer));
    try {
        while (readed_bytes > 0) {
            // There is no command yet
            if (!command_to_execute) {
                std::size_t parsed = 0;
                if (parser.Parse(client_buffer, readed_bytes, parsed)) {
                    // There is no command to be launched, continue to parse input stream
                    // Here we are, current chunk finished some command, process it
                    command_to_execute = parser.Build(arg_remains);
                    if (arg_remains > 0) {
                        arg_remains += 2;
                    }
                }

                // Parsed might fails to consume any bytes from input stream. In real life that could happens,
                // for example, because we are working with UTF-16 chars and only 1 byte left in stream
                if (parsed == 0) {
                    break;
                } else {
                    std::memmove(client_buffer, client_buffer + parsed, readed_bytes - parsed);
                    readed_bytes -= parsed;
                }
            }

            // There is command, but we still wait for argument to arrive...
            if (command_to_execute && arg_remains > 0) {
                // There is some parsed command, and now we are reading argument
                std::size_t to_read = std::min(arg_remains, std::size_t(readed_bytes));
                argument_for_command.append(client_buffer, to_read);

                std::memmove(client_buffer, client_buffer + to_read, readed_bytes - to_read);
                arg_remains -= to_read;
                readed_bytes -= to_read;
            }

            // Thre is command & argument - RUN!
            if (command_to_execute && arg_remains == 0) {
                std::string result;
                if (argument_for_command.size()) {
                    argument_for_command.resize(argument_for_command.size() - 2);
                }
                command_to_execute->Execute(*ps, argument_for_command, result);

                // We need to send response
                result += "\r\n";
                results.push_back(result);
                // if (send(client_socket, result.data(), result.size(), 0) <= 0) {
                //    throw std::runtime_error("Failed to send response");
                //}
                if (!readed_bytes) {
                    _event.events &= !EPOLLIN;
                }
                _event.events |= EPOLLOUT;

                // Prepare for the next command
                command_to_execute.reset();
                argument_for_command.resize(0);
                parser.Reset();
            }
        } // while (readed_bytes)
        if (readed_bytes < 0) {
            OnError();
            return;
        }
    } catch (std::runtime_error &ex) {
        results.push_back("ERROR\r\n");
        _event.events |= EPOLLOUT;
    }
    if (!(_event.events & EPOLLOUT) && !(_event.events & EPOLLIN)) {
        OnClose();
    }
}

// See Connection.h
void Connection::DoWrite() {
    std::cout << "DoWrite" << std::endl;

    auto output = std::unique_ptr<struct iovec[]>(new struct iovec[results.size()]);
    for (size_t i = 0; i != results.size(); ++i) {
        output[i].iov_base = &results[i][0];
        output[i].iov_len = results[i].size();
    }
    int written = writev(_socket, output.get(), results.size());
    if (written <= 0) {
        OnError();
        return;
    }
    auto last = results.begin();
    while (written > 0) {
        if (written >= last->size()) {
            written -= last->size();
            ++last;
        } else {
            last->erase(last->begin(), last->begin() + written);
            break;
        }
    }
    results.erase(results.begin(), last);
    if (!results.size()) {
        _event.events &= !EPOLLOUT;
    }
    if (!(_event.events & EPOLLOUT) && !(_event.events & EPOLLIN)) {
        OnClose();
    }
}

} // namespace STnonblock
} // namespace Network
} // namespace Afina
