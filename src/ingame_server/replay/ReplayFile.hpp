#pragma once

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>

#include "../../common/network/PacketStructs.hpp"

#pragma pack(push, 1)
struct ReplayHeaderV1 {
    char magic[4];         // "GRPL"
    uint16_t version;      // 1
    uint16_t tickRate;     // e.g. 60
    uint32_t matchId;
    uint32_t firstTick;
    uint32_t lastTick;     // 0 if unknown at write time
    uint16_t frameSize;    // sizeof(ResIngameState)
    uint16_t reserved;
};

struct ReplayHeader {
    char magic[4];         // "GRPL"
    uint16_t version;      // 2
    uint16_t headerSize;   // sizeof(ReplayHeader)
    uint16_t tickRate;     // e.g. 60
    uint16_t reserved0;
    uint32_t matchId;
    uint32_t firstTick;
    uint32_t lastTick;     // 0 if unknown at write time
    uint16_t frameSize;    // sizeof(ResIngameState)
    uint16_t reserved1;
    char mapPath[256];     // null-terminated if set
};
#pragma pack(pop)

class ReplayWriter {
public:
    ReplayWriter() = default;

    void Open(const std::string& path, uint16_t tickRate, uint32_t matchId) {
        Close();
        m_path = path;
        m_stream.open(path, std::ios::binary | std::ios::trunc | std::ios::out);
        if (!m_stream.is_open()) {
            throw std::runtime_error("ReplayWriter: failed to open file: " + path);
        }

        ReplayHeader hdr{};
        std::memcpy(hdr.magic, "GRPL", 4);
        hdr.version = 2;
        hdr.headerSize = (uint16_t)sizeof(ReplayHeader);
        hdr.tickRate = tickRate;
        hdr.reserved0 = 0;
        hdr.matchId = matchId;
        hdr.firstTick = 0;
        hdr.lastTick = 0;
        hdr.frameSize = (uint16_t)sizeof(ResIngameState);
        hdr.reserved1 = 0;
        std::memset(hdr.mapPath, 0, sizeof(hdr.mapPath));
        m_stream.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
        if (!m_stream.good()) {
            throw std::runtime_error("ReplayWriter: failed to write header");
        }

        m_header = hdr;
        m_hasHeader = true;
    }

    bool IsOpen() const { return m_stream.is_open(); }

    void SetMapPath(const std::string& mapPath) {
        if (!m_stream.is_open() || !m_hasHeader) return;
        std::memset(m_header.mapPath, 0, sizeof(m_header.mapPath));
        if (!mapPath.empty()) {
            std::snprintf(m_header.mapPath, sizeof(m_header.mapPath), "%s", mapPath.c_str());
        }
        PatchHeader();
    }

    void Append(const ResIngameState& frame) {
        if (!m_stream.is_open() || !m_hasHeader) return;

        if (m_header.firstTick == 0) {
            m_header.firstTick = frame.tick;
            // patch header firstTick
            PatchHeader();
        }
        m_header.lastTick = frame.tick;

        m_stream.write(reinterpret_cast<const char*>(&frame), sizeof(frame));
        if (!m_stream.good()) {
            throw std::runtime_error("ReplayWriter: write failed");
        }
    }

    void Close() {
        if (m_stream.is_open()) {
            // final patch of header (lastTick)
            if (m_hasHeader) {
                try {
                    PatchHeader();
                } catch (...) {
                    // best-effort
                }
            }
            m_stream.close();
        }
        m_hasHeader = false;
        m_path.clear();
        std::memset(&m_header, 0, sizeof(m_header));
    }

    ~ReplayWriter() { Close(); }

private:
    void PatchHeader() {
        if (!m_stream.is_open()) return;
        std::streampos cur = m_stream.tellp();
        m_stream.seekp(0, std::ios::beg);
        m_stream.write(reinterpret_cast<const char*>(&m_header), (std::streamsize)m_header.headerSize);
        m_stream.seekp(cur);
    }

    std::string m_path;
    std::ofstream m_stream;
    ReplayHeader m_header{};
    bool m_hasHeader{false};
};

class ReplayReader {
public:
    ReplayReader() = default;

    void Open(const std::string& path) {
        Close();
        m_path = path;
        m_stream.open(path, std::ios::binary | std::ios::in);
        if (!m_stream.is_open()) {
            throw std::runtime_error("ReplayReader: failed to open file: " + path);
        }

        // Peek magic+version to select header format.
        char magic[4];
        uint16_t version = 0;
        m_stream.read(reinterpret_cast<char*>(&magic[0]), 4);
        m_stream.read(reinterpret_cast<char*>(&version), sizeof(version));
        if (!m_stream.good()) {
            throw std::runtime_error("ReplayReader: failed to read header prefix");
        }
        if (std::memcmp(magic, "GRPL", 4) != 0) {
            throw std::runtime_error("ReplayReader: bad magic (not a GRPL replay)");
        }

        m_stream.seekg(0, std::ios::beg);
        if (version == 1) {
            ReplayHeaderV1 v1{};
            m_stream.read(reinterpret_cast<char*>(&v1), sizeof(v1));
            if (!m_stream.good()) {
                throw std::runtime_error("ReplayReader: failed to read v1 header");
            }
            if (v1.frameSize != sizeof(ResIngameState)) {
                throw std::runtime_error("ReplayReader: frameSize mismatch (protocol/struct changed)");
            }
            ReplayHeader v2{};
            std::memcpy(v2.magic, v1.magic, 4);
            v2.version = 1;
            v2.headerSize = (uint16_t)sizeof(ReplayHeaderV1);
            v2.tickRate = v1.tickRate;
            v2.reserved0 = 0;
            v2.matchId = v1.matchId;
            v2.firstTick = v1.firstTick;
            v2.lastTick = v1.lastTick;
            v2.frameSize = v1.frameSize;
            v2.reserved1 = 0;
            std::memset(v2.mapPath, 0, sizeof(v2.mapPath));
            m_header = v2;
        } else if (version == 2) {
            ReplayHeader hdr{};
            m_stream.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
            if (!m_stream.good()) {
                throw std::runtime_error("ReplayReader: failed to read v2 header");
            }
            if (hdr.headerSize < sizeof(ReplayHeaderV1) || hdr.headerSize > sizeof(ReplayHeader)) {
                throw std::runtime_error("ReplayReader: invalid headerSize");
            }
            if (hdr.frameSize != sizeof(ResIngameState)) {
                throw std::runtime_error("ReplayReader: frameSize mismatch (protocol/struct changed)");
            }
            m_header = hdr;
        } else {
            throw std::runtime_error("ReplayReader: unsupported version");
        }

        // compute frame count
        m_stream.seekg(0, std::ios::end);
        const std::streamoff fileSize = m_stream.tellg();
        const std::streamoff dataSize = fileSize - (std::streamoff)m_header.headerSize;
        if (dataSize < 0 || (dataSize % (std::streamoff)m_header.frameSize) != 0) {
            throw std::runtime_error("ReplayReader: corrupt/truncated replay file");
        }
        m_frameCount = (uint32_t)(dataSize / (std::streamoff)m_header.frameSize);

        m_stream.seekg((std::streamoff)m_header.headerSize, std::ios::beg);
    }

    bool IsOpen() const { return m_stream.is_open(); }
    const ReplayHeader& Header() const { return m_header; }
    uint32_t FrameCount() const { return m_frameCount; }

    bool ReadFrameAtIndex(uint32_t index, ResIngameState& out) {
        if (!m_stream.is_open()) return false;
        if (index >= m_frameCount) return false;
        const std::streamoff offset = (std::streamoff)m_header.headerSize + (std::streamoff)index * (std::streamoff)m_header.frameSize;
        m_stream.seekg(offset, std::ios::beg);
        m_stream.read(reinterpret_cast<char*>(&out), sizeof(out));
        return m_stream.good();
    }

    void Close() {
        if (m_stream.is_open()) m_stream.close();
        m_path.clear();
        std::memset(&m_header, 0, sizeof(m_header));
        m_frameCount = 0;
    }

    ~ReplayReader() { Close(); }

private:
    std::string m_path;
    std::ifstream m_stream;
    ReplayHeader m_header{};
    uint32_t m_frameCount{0};
};
