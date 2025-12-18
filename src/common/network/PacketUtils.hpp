#ifndef PACKET_UTILS_HPP
#define PACKET_UTILS_HPP

#include "Packet.hpp"
#include "TCPSocket.hpp"
#include <vector>
#include <string>
#include <cstdint>

namespace PacketUtils {
    // Serialization
    void SerializePacket(const Packet&, std::vector<char>& outBuffer);
    void WriteInt(std::vector<char>& buffer, int32_t value);
    void WriteString(std::vector<char>& buffer, const std::string& value);
    void WriteFloat(std::vector<char>& buffer, float value);
    void WriteBool(std::vector<char>& buffer, bool value);
    // Deserialization
    bool DeserializePacket(const std::vector<char>& inBuffer, Packet& outPacket);
    bool ReadHeader(const char* buffer, size_t size, Header& outHeader);
    int32_t ReadInt(const std::vector<char>& buffer, size_t& offset);
    std::string ReadString(const std::vector<char>& buffer, size_t& offset);
    float ReadFloat(const std::vector<char>& buffer, size_t& offset);
    bool ReadBool(const std::vector<char>& buffer, size_t& offset);
};

template <typename T>
bool SendPacket(TCPSocket* socket, PacketType type, const T& payloadStruct) {
    Packet packet(type);
    packet.SetPayload(payloadStruct);
    std::vector<char> buffer;
    PacketUtils::SerializePacket(packet, buffer);
    return socket->Send(buffer.data(), buffer.size());
}

bool SendPacket(TCPSocket* socket, PacketType type) {
    Packet packet(type);
    std::vector<char> buffer;
    PacketUtils::SerializePacket(packet, buffer);
    return socket->Send(buffer.data(), buffer.size());
}

#endif // PACKET_UTILS_HPP