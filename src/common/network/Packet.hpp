#ifndef PACKET_HPP
#define PACKET_HPP

#include <PacketType.hpp>
#include <cstdint>
#include <vector>

#pragma pack(push, 1) // Ensure no padding is added by the compiler
typedef struct {
    PacketType type;
    uint32_t length;
} Header;
#pragma pack(pop)

class Packet {
public:
    Header header;
    std::vector<char> payload;

    Packet() {
        header.type = static_cast<PacketType>(0);
        header.length = 0;
    }

    Packet(PacketType type) {
        header.type = type;
        header.length = 0;
    }

    // Convert struct into payload bytes
    template<typename T>
    void SetPayload(const T& data) {
        size_t size = sizeof(T);
        payload.resize(size);
        std::memcpy(payload.data(), &data, size);
        header.length = static_cast<uint32_t>(size);
    }

    // Convert payload bytes into struct
    template<typename T>
    T GetPayload() const {
        if (payload.size() < sizeof(T)) {
            throw std::runtime_error("Payload size is smaller than requested type size");
        }
        T data;
        std::memcpy(&data, payload.data(), sizeof(T));
        return data;
    }
};

#endif // PACKET_HPP