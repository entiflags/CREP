#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <memory>

class MemBuffer {
public:
    static std::unique_ptr<MemBuffer> fromFile(const std::wstring& path);
    static std::unique_ptr<MemBuffer> fromData(const uint8_t* data, size_t size);

    uint8_t  readByte(size_t offset) const;
    uint16_t readWord(size_t offset) const;
    uint32_t readDword(size_t offset) const;
    uint64_t readQword(size_t offset) const;
    bool     readBytes(size_t offset, uint8_t* out, size_t count) const;

    bool writeByte(size_t offset, uint8_t value);
    bool writeBytes(size_t offset, const uint8_t* data, size_t count);

    size_t         size() const;
    const uint8_t* data() const;
    bool           isValidOffset(size_t offset) const;

private:
    MemBuffer() = default;
    std::vector<uint8_t> m_data;
};
