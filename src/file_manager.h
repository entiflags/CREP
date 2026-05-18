#pragma once

#include "mem_buffer.h"
#include "pe_parser.h"
#include <string>
#include <vector>
#include <memory>

struct FileContext {
    std::wstring               filePath;
    std::unique_ptr<MemBuffer> buffer;
    PEInfo                     peInfo;
    bool                       isPE = false;
    bool                       isModified = false;
};

class FileManager {
public:
    FileContext* openFile(const std::wstring& path);
    bool saveFile(FileContext* ctx);
    bool saveFileAs(FileContext* ctx, const std::wstring& newPath);
    void closeFile(FileContext* ctx);
    FileContext* getActiveFile() const { return m_activeFile; }

private:
    std::vector<std::unique_ptr<FileContext>> m_files;
    FileContext* m_activeFile = nullptr;
    PEParser m_parser;
};
