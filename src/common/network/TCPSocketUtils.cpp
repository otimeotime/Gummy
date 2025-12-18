#include "PacketUtils.hpp"
#include <cstring>
#include <iostream>

namespace PacketUtils {
    // Main Serialization/Deserialization Functions------------------------------------------------------------------
    void SerializePacket(const Packet& packet, std::vector<char>& outBuffer) {
        size_t totalSize = sizeof(Header) + packet.payload.size();
        outBuffer.resize(totalSize);
        std::memcpy(outBuffer.data(), &packet.header, sizeof(Header));
        if (!packet.payload.empty()) {
            std::memcpy(outBuffer.data() + sizeof(Header), packet.payload.data(), packet.payload.size());
        }
    }

    bool DeserializePacket(const std::vector<char>& inBuffer, Packet& outPacket) {
        if (inBuffer.size() < sizeof(Header)) {
            return false;
        }

        Header header;
        std::memcpy(&header, inBuffer.data(), sizeof(Header));
        if(inBuffer.size() < sizeof(Header) + header.length) {
            return false;
        }
        outPacket.header = header;
        outPacket.payload.resize(header.length);
        if (header.length > 0) {
            std::memcpy(outPacket.payload.data(), inBuffer.data() + sizeof(Header), header.length);
        }
        return true;
    }
    
    bool ReadHeader(const char* buffer, size_t size, Header& outHeader) {
        if (size < sizeof(Header)) {
            return false;
        }
        std::memcpy(&outHeader, buffer, sizeof(Header));
        return true;
    }
    //--------------------------------------------------------------------------------------------------------------

    // Utility Write Functions, turn data into byte buffers---------------------------------------------------------
    void WriteInt(std::vector<char>& buffer, int32_t value) {
        size_t oldSize = buffer.size();
        buffer.resize(oldSize + sizeof(int32_t));
        std::memcpy(buffer.data() + oldSize, &value, sizeof(int32_t));
    }

    void WriteFloat(std::vector<char>& buffer, float value) {
        size_t oldSize = buffer.size();
        buffer.resize(oldSize + sizeof(float));
        std::memcpy(buffer.data() + oldSize, &value, sizeof(float));
    }

    void WriteBool(std::vector<char>& buffer, bool value) {
        size_t oldSize = buffer.size();
        buffer.resize(oldSize + sizeof(bool));
        std::memcpy(buffer.data() + oldSize, &value, sizeof(bool));
    }

    void WriteString(std::vector<char>& buffer, const std::string& str) {
        int32_t len = static_cast<int32_t>(str.size());
        WriteInt(buffer, len);

        size_t oldSize = buffer.size();
        buffer.resize(oldSize + len);
        std::memcpy(buffer.data() + oldSize, str.data(), len);
    }
    //--------------------------------------------------------------------------------------------------------------


    // Utility Read Functions, turn byte buffers into data----------------------------------------------------------
    int32_t ReadInt(const std::vector<char>& buffer, size_t& offset) {
        if (offset + sizeof(int32_t) > buffer.size()) throw std::runtime_error("Buffer underflow (Int)");
        
        int32_t value;
        std::memcpy(&value, buffer.data() + offset, sizeof(int32_t));
        offset += sizeof(int32_t);
        return value;
    }

    float ReadFloat(const std::vector<char>& buffer, size_t& offset) {
        if (offset + sizeof(float) > buffer.size()) throw std::runtime_error("Buffer underflow (Float)");
        
        float value;
        std::memcpy(&value, buffer.data() + offset, sizeof(float));
        offset += sizeof(float);
        return value;
    }

    bool ReadBool(const std::vector<char>& buffer, size_t& offset) {
        if (offset + sizeof(bool) > buffer.size()) throw std::runtime_error("Buffer underflow (Bool)");
        
        bool value;
        std::memcpy(&value, buffer.data() + offset, sizeof(bool));
        offset += sizeof(bool);
        return value;
    }

    std::string ReadString(const std::vector<char>& buffer, size_t& offset) {
        int32_t len = ReadInt(buffer, offset);
        if (len < 0 || offset + len > buffer.size()) throw std::runtime_error("Buffer underflow (String)");
        std::string str(buffer.data() + offset, len);
        offset += len;
        return str;
    }
    //--------------------------------------------------------------------------------------------------------------

}
