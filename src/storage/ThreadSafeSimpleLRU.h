#ifndef AFINA_STORAGE_THREAD_SAFE_SIMPLE_LRU_H
#define AFINA_STORAGE_THREAD_SAFE_SIMPLE_LRU_H

#include <condition_variable>
#include <map>
#include <mutex>
#include <string>

#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

/**
 * # SimpleLRU thread safe version
 *
 *
 */
class ThreadSafeSimplLRU : public Storage {
public:
    ThreadSafeSimplLRU(size_t max_size = 1024) : _st_storage(max_size) {}
    ~ThreadSafeSimplLRU() {}

    // see SimpleLRU.h
    bool Put(const std::string &key, const std::string &value) override {
        // TODO: sinchronization
        std::unique_lock<std::mutex> lock(in_use);
        return _st_storage.Put(key, value);
    }

    // see SimpleLRU.h
    bool PutIfAbsent(const std::string &key, const std::string &value) override {
        // TODO: sinchronization
        std::unique_lock<std::mutex> lock(in_use);
        return _st_storage.PutIfAbsent(key, value);
    }

    // see SimpleLRU.h
    bool Set(const std::string &key, const std::string &value) override {
        // TODO: sinchronization
        std::unique_lock<std::mutex> lock(in_use);
        return _st_storage.Set(key, value);
    }

    // see SimpleLRU.h
    bool Delete(const std::string &key) override {
        // TODO: sinchronization
        std::unique_lock<std::mutex> lock(in_use);
        return _st_storage.Delete(key);
    }

    // see SimpleLRU.h
    bool Get(const std::string &key, std::string &value) override {
        // TODO: sinchronization
        std::unique_lock<std::mutex> lock(in_use);
        return _st_storage.Get(key, value);
    }

private:
    // TODO: sinchronization primitives
    std::mutex in_use;
    SimpleLRU _st_storage;
};

} // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_THREAD_SAFE_SIMPLE_LRU_H
