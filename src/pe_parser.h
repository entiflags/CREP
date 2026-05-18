#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

class MemBuffer;

struct PESectionInfo {
    char name[9];
    uint32_t virtualAddress;
    uint32_t virtualSize;
    uint32_t rawDataOffset;
    uint32_t rawDataSize;
    uint32_t characteristics;
};

struct PEImportEntry {
    std::string dllName;
    std::vector<std::string> functions;
};

struct PEExportEntry {
    std::string name;
    uint32_t    ordinal;
    uint32_t    rva;
};

struct PEInfo {
    // DOS header
    uint32_t peSignatureOffset = 0;

    // File header
    uint16_t machine = 0;
    uint16_t numberOfSections = 0;
    uint32_t timestamp = 0;

    // Optional header
    bool is64Bit = false;
    uint64_t imageBase = 0;
    uint32_t entryPointRVA = 0;
    uint32_t sectionAlignment = 0;
    uint32_t fileAlignment = 0;

    // Data directories (RVA + size)
    uint32_t importDirRVA = 0;
    uint32_t importDirSize = 0;
    uint32_t exportDirRVA = 0;
    uint32_t exportDirSize = 0;

    // Parsed data
    std::vector<PESectionInfo> sections;
    std::vector<PEImportEntry> imports;
    std::vector<PEExportEntry> exports;

    // Validity
    bool isValid = false;
    std::string errorMessage;
};

class PEParser {
public:
    PEInfo parse(const MemBuffer& buffer);

    // Address conversion utilities
    static uint32_t rvaToFileOffset(const PEInfo& pe, uint32_t rva);
    static uint32_t fileOffsetToRva(const PEInfo& pe, uint32_t offset);

private:
    bool parseDosHeader(const MemBuffer& buf, PEInfo& info);
    bool parseNtHeaders(const MemBuffer& buf, PEInfo& info);
    bool parseSectionHeaders(const MemBuffer& buf, PEInfo& info);
    bool parseImportDirectory(const MemBuffer& buf, PEInfo& info);
    bool parseExportDirectory(const MemBuffer& buf, PEInfo& info);

    // Read a null-terminated ASCII string from buffer
    static std::string readAsciiString(const MemBuffer& buf, size_t offset, size_t maxLen = 256);
};
