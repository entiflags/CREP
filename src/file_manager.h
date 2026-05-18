#pragma once

#include "mem_buffer.h"
#include "pe_parser.h"
#include <string>
#include <vector>
#include <memory>

struct FileContext {
    std::wstring             filePath;
    std::unique_ptr<MemBuffer> buffer;
    PEInfo                   peInfo;
    bool                     isPE = false;
    bool                     isModified = false;
};

class FileManager {
public:
    // Open a file, parse PE if applicable. Returns pointer to context or nullptr on failure.
    FileContext* openFile(const std::wstring& path);

    // Save current buffer to original path
    bool saveFile(FileContext* ctx);

    // Save current buffer to a new path
    bool saveFileAs(FileContext* ctx, const std::wstring& newPath);

    // Close and remove a file context
    void closeFile(FileContext* ctx);

    // Get the currently active file
    FileContext* getActiveFile() const { return m_activeFile; }

private:
    std::vector<std::unique_ptr<FileContext>> m_files;
    FileContext* m_activeFile = nullptr;
    PEParser m_parser;
};
