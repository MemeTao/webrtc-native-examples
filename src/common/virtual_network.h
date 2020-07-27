#ifndef SRC_COMMON_VIRTUAL_NETWORK_H_
#define SRC_COMMON_VIRTUAL_NETWORK_H_

#include <cstdint>
#include <cstddef>

class VirtrualSocket {
public:
    void send(const uint8_t* data, size_t size);
};

class VirtualNetRouter {
public:
    void add_router();
};
#endif
