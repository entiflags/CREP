#include "file_manager.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

FileContext* FileManager::openFile(const std::wstring& path)
{
    auto buffer = MemBuffer::fromFile(path);
    if (!buffer)
        return nullptr;

    auto ctx = std::make_unique<FileContext>();
    ctx->filePath = path;
    ctx->isModified = false;

    // Detect PE by MZ signature
    if (buffer->size() >= 2 && buffer->readWord(0) == 0x5A4D) {
        ctx->isPE = true;
        ctx->peInfo = m_parser.parse(*buffer);
    }
    else {
        ctx->isPE = false;
    }

    ctx->buffer = std::move(buffer);
    m_activeFile = ctx.get();
    m_files.push_back(std::move(ctx));

    return m_activeFile;
}

bool FileManager::saveFile(FileContext* ctx)
{
    if (!ctx || !ctx->buffer)
        return false;
    return saveFileAs(ctx, ctx->filePath);
}

bool FileManager::saveFileAs(FileContext* ctx, const std::wstring& newPath)
{
    if (!ctx || !ctx->buffer || newPath.empty())
        return false;

    HANDLE hFile = CreateFileW(
        newPath.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (hFile == INVALID_HANDLE_VALUE)
        return false;

    DWORD written = 0;
    BOOL ok = WriteFile(
        hFile,
        ctx->buffer->data(),
        static_cast<DWORD>(ctx->buffer->size()),
        &written,
        nullptr);

    CloseHandle(hFile);

    if (!ok || written != static_cast<DWORD>(ctx->buffer->size()))
        return false;

    ctx->filePath = newPath;
    ctx->isModified = false;
    return true;
}

void FileManager::closeFile(FileContext* ctx)
{
    if (!ctx)
        return;

    for (auto it = m_files.begin(); it != m_files.end(); ++it) {
        if (it->get() == ctx) {
            m_files.erase(it);
            break;
        }
    }

    if (m_activeFile == ctx)
        m_activeFile = m_files.empty() ? nullptr : m_files.back().get();
}
