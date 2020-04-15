#ifndef AFINA_STORAGE_SIMPLE_LRU_H
#define AFINA_STORAGE_SIMPLE_LRU_H

#include <map>
#include <memory>
#include <mutex>
#include <string>

#include <afina/Storage.h>

namespace Afina {
namespace Backend {

/**
 * # Map based implementation
 * That is NOT thread safe implementaiton!!
 */
class SimpleLRU : public Afina::Storage {
public:
    SimpleLRU(size_t max_size = 1024) : _max_size(max_size) {
        _current_size = 0;
        // _lru_head->prev = _lru_head.get();
    }

    ~SimpleLRU() {
        _lru_index.clear();
        if (_lru_head) {
            auto ptr = _lru_head->prev;
            while (ptr != _lru_head.get()) {
                ptr->next.reset();
                ptr = ptr->prev;
            }
        }
        _lru_head.reset();
    }

    // Implements Afina::Storage interface
    bool Put(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool PutIfAbsent(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool Set(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool Delete(const std::string &key) override;

    // Implements Afina::Storage interface
    bool Get(const std::string &key, std::string &value) override;

private:
    // LRU cache node
    using lru_node = struct lru_node {
        const std::string key;
        std::string value;
        lru_node *prev;
        std::unique_ptr<lru_node> next;

        lru_node(const std::string &key, const std::string &value) : key(key), value(value) {}
    };

    /**
     * Just stores association between given key/value pair.
     * Adds element to tail.
     *
     * @param key to be associated with value
     * @param value to be assigned for the key
     */
    void _PutWithoutCheck(const std::string &key, const std::string &value);

    /**
     * Free some space. Returns true if operation is successful.
     *
     * @param size to free this size
     */
    bool _FreeSpace(std::size_t size);

    /**
     * Cut node and put to the tail.
     *
     * @param node to node
     */
    void _PutToTail(std::unique_ptr<lru_node> &&node);

    // Maximum number of bytes could be stored in this cache.
    // i.e all (keys+values) must be less the _max_size
    std::size_t _max_size;
    std::size_t _current_size; //

    // Main storage of lru_nodes, elements in this list ordered descending by "freshness": in the head
    // element that wasn't used for longest time.
    //
    // List owns all nodes
    std::unique_ptr<lru_node> _lru_head;
    // _lru_head->prev is last element

    // Index of nodes from list above, allows fast random access to elements by lru_node#key
    std::map<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>, std::less<std::string>>
        _lru_index;
};

} // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_SIMPLE_LRU_H
