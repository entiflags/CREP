#include "mem_buffer.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <new>
#include <algorithm>

std::unique_ptr<MemBuffer> MemBuffer::fromFile(const std::wstring& path)
{
    HANDLE hFile = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (hFile == INVALID_HANDLE_VALUE)
        return nullptr;

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize) || fileSize.QuadPart == 0) {
        CloseHandle(hFile);
        return nullptr;
    }

    auto buffer = std::unique_ptr<MemBuffer>(new MemBuffer());

    try {
        buffer->m_data.resize(static_cast<size_t>(fileSize.QuadPart));
    }
    catch (const std::bad_alloc&) {
        CloseHandle(hFile);
        return nullptr;
    }

    size_t totalToRead = buffer->m_data.size();
    size_t totalRead = 0;
    uint8_t* dest = buffer->m_data.data();

    while (totalRead < totalToRead) {
        DWORD chunkSize = static_cast<DWORD>(
            (std::min)(totalToRead - totalRead, static_cast<size_t>(0xFFFFFFFF)));
        DWORD bytesRead = 0;

        if (!ReadFile(hFile, dest + totalRead, chunkSize, &bytesRead, nullptr)
            || bytesRead == 0) {
            CloseHandle(hFile);
            return nullptr;
        }
        totalRead += bytesRead;
    }

    CloseHandle(hFile);
    return buffer;
}

std::unique_ptr<MemBuffer> MemBuffer::fromData(const uint8_t* data, size_t size)
{
    if (!data || size == 0)
        return nullptr;

    auto buffer = std::unique_ptr<MemBuffer>(new MemBuffer());

    try {
        buffer->m_data.assign(data, data + size);
    }
    catch (const std::bad_alloc&) {
        return nullptr;
    }

    return buffer;
}

uint8_t MemBuffer::readByte(size_t offset) const
{
    if (offset >= m_data.size())
        return 0;
    return m_data[offset];
}

uint16_t MemBuffer::readWord(size_t offset) const
{
    if (offset >= m_data.size() || (m_data.size() - offset) < 2)
        return 0;

    return static_cast<uint16_t>(m_data[offset])
         | (static_cast<uint16_t>(m_data[offset + 1]) << 8);
}

uint32_t MemBuffer::readDword(size_t offset) const
{
    if (offset >= m_data.size() || (m_data.size() - offset) < 4)
        return 0;

    return static_cast<uint32_t>(m_data[offset])
         | (static_cast<uint32_t>(m_data[offset + 1]) << 8)
         | (static_cast<uint32_t>(m_data[offset + 2]) << 16)
         | (static_cast<uint32_t>(m_data[offset + 3]) << 24);
}

uint64_t MemBuffer::readQword(size_t offset) const
{
    if (offset >= m_data.size() || (m_data.size() - offset) < 8)
        return 0;

    return static_cast<uint64_t>(m_data[offset])
         | (static_cast<uint64_t>(m_data[offset + 1]) << 8)
         | (static_cast<uint64_t>(m_data[offset + 2]) << 16)
         | (static_cast<uint64_t>(m_data[offset + 3]) << 24)
         | (static_cast<uint64_t>(m_data[offset + 4]) << 32)
         | (static_cast<uint64_t>(m_data[offset + 5]) << 40)
         | (static_cast<uint64_t>(m_data[offset + 6]) << 48)
         | (static_cast<uint64_t>(m_data[offset + 7]) << 56);
}

bool MemBuffer::readBytes(size_t offset, uint8_t* out, size_t count) const
{
    if (!out || count == 0)
        return false;
    if (offset >= m_data.size())
        return false;
    if (count > m_data.size() - offset)
        return false;

    memcpy(out, m_data.data() + offset, count);
    return true;
}

bool MemBuffer::writeByte(size_t offset, uint8_t value)
{
    if (offset >= m_data.size())
        return false;
    m_data[offset] = value;
    return true;
}

bool MemBuffer::writeBytes(size_t offset, const uint8_t* data, size_t count)
{
    if (!data || count == 0)
        return false;
    if (offset >= m_data.size())
        return false;
    if (count > m_data.size() - offset)
        return false;

    memcpy(m_data.data() + offset, data, count);
    return true;
}

size_t MemBuffer::size() const
{
    return m_data.size();
}

const uint8_t* MemBuffer::data() const
{
    return m_data.data();
}

bool MemBuffer::isValidOffset(size_t offset) const
{
    return offset < m_data.size();
}
