#include <afina/Storage.h>
#include <afina/execute/Set.h>
#include <thread>

#include <iostream>

namespace Afina {
namespace Execute {

// memcached protocol: "set" means "store this data".
void Set::Execute(Storage &storage, const std::string &args, std::string &out) {
    std::cout << "Set(" << _key << "): " << args << std::endl;
    // std::cout << "Lol1" << std::endl;
    storage.Put(_key, args);
    // std::this_thread::sleep_for(std::chrono::milliseconds(10000));

    std::cout << "STORED" << std::endl;
    out = "STORED";
}

} // namespace Execute
} // namespace Afina
