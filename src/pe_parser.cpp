#include "pe_parser.h"
#include "mem_buffer.h"

std::string PEParser::readAsciiString(const MemBuffer& buf, size_t offset, size_t maxLen)
{
    std::string result;
    result.reserve(64);

    for (size_t i = 0; i < maxLen; ++i) {
        if (offset + i >= buf.size())
            break;
        uint8_t ch = buf.readByte(offset + i);
        if (ch == 0)
            break;
        result.push_back(static_cast<char>(ch));
    }
    return result;
}

uint32_t PEParser::rvaToFileOffset(const PEInfo& pe, uint32_t rva)
{
    for (const auto& sec : pe.sections) {
        uint32_t secStart = sec.virtualAddress;
        uint32_t secEnd = secStart + sec.virtualSize;

        if (secEnd < secStart)
            secEnd = 0xFFFFFFFF;

        if (rva >= secStart && rva < secEnd) {
            uint32_t delta = rva - secStart;
            uint64_t result = static_cast<uint64_t>(sec.rawDataOffset) + delta;
            if (result > 0xFFFFFFFF)
                return 0;
            return static_cast<uint32_t>(result);
        }
    }
    return 0;
}

uint32_t PEParser::fileOffsetToRva(const PEInfo& pe, uint32_t offset)
{
    for (const auto& sec : pe.sections) {
        uint32_t secStart = sec.rawDataOffset;
        uint32_t secEnd = secStart + sec.rawDataSize;

        if (secEnd < secStart)
            secEnd = 0xFFFFFFFF;

        if (offset >= secStart && offset < secEnd) {
            uint32_t delta = offset - secStart;
            uint64_t result = static_cast<uint64_t>(sec.virtualAddress) + delta;
            if (result > 0xFFFFFFFF)
                return 0;
            return static_cast<uint32_t>(result);
        }
    }
    return 0;
}

PEInfo PEParser::parse(const MemBuffer& buffer)
{
    PEInfo info = {};
    info.isValid = false;

    if (!parseDosHeader(buffer, info))
        return info;

    if (!parseNtHeaders(buffer, info))
        return info;

    if (!parseSectionHeaders(buffer, info))
        return info;

    parseImportDirectory(buffer, info);
    parseExportDirectory(buffer, info);

    info.isValid = true;
    return info;
}

bool PEParser::parseDosHeader(const MemBuffer& buf, PEInfo& info)
{
    if (buf.size() < 64) {
        info.errorMessage = "File too small for DOS header";
        return false;
    }

    uint16_t mzSig = buf.readWord(0);
    if (mzSig != 0x5A4D) {
        info.errorMessage = "Invalid MZ signature";
        return false;
    }

    info.peSignatureOffset = buf.readDword(0x3C);
    return true;
}

bool PEParser::parseNtHeaders(const MemBuffer& buf, PEInfo& info)
{
    uint32_t peOffset = info.peSignatureOffset;

    if (peOffset + 4 > buf.size()) {
        info.errorMessage = "PE signature offset out of bounds";
        return false;
    }

    uint32_t peSig = buf.readDword(peOffset);
    if (peSig != 0x00004550) {
        info.errorMessage = "Invalid PE signature";
        return false;
    }

    // File header
    size_t fhOffset = static_cast<size_t>(peOffset) + 4;
    if (fhOffset + 20 > buf.size()) {
        info.errorMessage = "File header truncated";
        return false;
    }

    info.machine = buf.readWord(fhOffset);
    info.numberOfSections = buf.readWord(fhOffset + 2);
    info.timestamp = buf.readDword(fhOffset + 4);

    // Optional header
    size_t ohOffset = fhOffset + 20;
    if (ohOffset + 2 > buf.size()) {
        info.errorMessage = "Optional header truncated";
        return false;
    }

    uint16_t magic = buf.readWord(ohOffset);

    if (magic == 0x020B) {
        // PE32+ (64-bit)
        info.is64Bit = true;

        if (ohOffset + 112 > buf.size()) {
            info.errorMessage = "Optional header (PE32+) truncated";
            return false;
        }

        info.entryPointRVA = buf.readDword(ohOffset + 16);
        info.imageBase = buf.readQword(ohOffset + 24);
        info.sectionAlignment = buf.readDword(ohOffset + 32);
        info.fileAlignment = buf.readDword(ohOffset + 36);

        uint32_t numDataDirs = buf.readDword(ohOffset + 108);
        size_t ddOffset = ohOffset + 112;

        if (numDataDirs > 0 && ddOffset + 8 <= buf.size()) {
            info.exportDirRVA = buf.readDword(ddOffset);
            info.exportDirSize = buf.readDword(ddOffset + 4);
        }
        if (numDataDirs > 1 && ddOffset + 16 <= buf.size()) {
            info.importDirRVA = buf.readDword(ddOffset + 8);
            info.importDirSize = buf.readDword(ddOffset + 12);
        }
    }
    else if (magic == 0x010B) {
        // PE32 (32-bit)
        info.is64Bit = false;

        if (ohOffset + 96 > buf.size()) {
            info.errorMessage = "Optional header (PE32) truncated";
            return false;
        }

        info.entryPointRVA = buf.readDword(ohOffset + 16);
        info.imageBase = buf.readDword(ohOffset + 28);
        info.sectionAlignment = buf.readDword(ohOffset + 32);
        info.fileAlignment = buf.readDword(ohOffset + 36);

        uint32_t numDataDirs = buf.readDword(ohOffset + 92);
        size_t ddOffset = ohOffset + 96;

        if (numDataDirs > 0 && ddOffset + 8 <= buf.size()) {
            info.exportDirRVA = buf.readDword(ddOffset);
            info.exportDirSize = buf.readDword(ddOffset + 4);
        }
        if (numDataDirs > 1 && ddOffset + 16 <= buf.size()) {
            info.importDirRVA = buf.readDword(ddOffset + 8);
            info.importDirSize = buf.readDword(ddOffset + 12);
        }
    }
    else {
        info.errorMessage = "Unrecognized Optional Header magic value";
        return false;
    }

    return true;
}

bool PEParser::parseSectionHeaders(const MemBuffer& buf, PEInfo& info)
{
    uint32_t peOffset = info.peSignatureOffset;
    size_t fhOffset = static_cast<size_t>(peOffset) + 4;
    uint16_t sizeOfOptionalHeader = buf.readWord(fhOffset + 16);

    size_t sectionsOffset = fhOffset + 20 + sizeOfOptionalHeader;
    size_t totalSectionBytes = static_cast<size_t>(info.numberOfSections) * 40;

    if (sectionsOffset + totalSectionBytes > buf.size()) {
        info.errorMessage = "Section headers truncated";
        return false;
    }

    info.sections.reserve(info.numberOfSections);

    for (uint16_t i = 0; i < info.numberOfSections; ++i) {
        size_t off = sectionsOffset + static_cast<size_t>(i) * 40;
        PESectionInfo sec = {};

        for (int j = 0; j < 8; ++j)
            sec.name[j] = static_cast<char>(buf.readByte(off + j));
        sec.name[8] = '\0';

        sec.virtualSize = buf.readDword(off + 8);
        sec.virtualAddress = buf.readDword(off + 12);
        sec.rawDataSize = buf.readDword(off + 16);
        sec.rawDataOffset = buf.readDword(off + 20);
        sec.characteristics = buf.readDword(off + 36);

        info.sections.push_back(sec);
    }

    return true;
}

bool PEParser::parseImportDirectory(const MemBuffer& buf, PEInfo& info)
{
    if (info.importDirRVA == 0 || info.importDirSize == 0)
        return true;

    uint32_t importOffset = rvaToFileOffset(info, info.importDirRVA);
    if (importOffset == 0)
        return true;

    const size_t descSize = 20;

    for (size_t i = 0; i < 1024; ++i) {
        size_t descOffset = static_cast<size_t>(importOffset) + i * descSize;

        if (descOffset + descSize > buf.size())
            break;

        uint32_t originalFirstThunk = buf.readDword(descOffset);
        uint32_t nameRVA = buf.readDword(descOffset + 12);
        uint32_t firstThunk = buf.readDword(descOffset + 16);

        if (nameRVA == 0 && originalFirstThunk == 0 && firstThunk == 0)
            break;

        uint32_t nameOffset = rvaToFileOffset(info, nameRVA);
        if (nameOffset == 0)
            continue;

        PEImportEntry entry;
        entry.dllName = readAsciiString(buf, nameOffset);
        if (entry.dllName.empty())
            continue;

        uint32_t thunkRVA = (originalFirstThunk != 0) ? originalFirstThunk : firstThunk;
        uint32_t thunkOffset = rvaToFileOffset(info, thunkRVA);
        if (thunkOffset == 0) {
            info.imports.push_back(std::move(entry));
            continue;
        }

        size_t thunkEntrySize = info.is64Bit ? 8 : 4;
        uint64_t ordinalFlag = info.is64Bit ? 0x8000000000000000ULL : 0x80000000ULL;

        for (size_t j = 0; j < 4096; ++j) {
            size_t tOff = static_cast<size_t>(thunkOffset) + j * thunkEntrySize;

            if (tOff + thunkEntrySize > buf.size())
                break;

            uint64_t thunkValue = info.is64Bit
                ? buf.readQword(tOff)
                : buf.readDword(tOff);

            if (thunkValue == 0)
                break;

            if (thunkValue & ordinalFlag) {
                uint16_t ord = static_cast<uint16_t>(thunkValue & 0xFFFF);
                entry.functions.push_back("Ordinal#" + std::to_string(ord));
            }
            else {
                uint32_t hintNameRVA = static_cast<uint32_t>(thunkValue);
                uint32_t hintNameOffset = rvaToFileOffset(info, hintNameRVA);
                if (hintNameOffset == 0 || hintNameOffset + 2 >= buf.size())
                    continue;

                std::string funcName = readAsciiString(buf, hintNameOffset + 2);
                if (!funcName.empty())
                    entry.functions.push_back(std::move(funcName));
            }
        }

        info.imports.push_back(std::move(entry));
    }

    return true;
}

bool PEParser::parseExportDirectory(const MemBuffer& buf, PEInfo& info)
{
    if (info.exportDirRVA == 0 || info.exportDirSize == 0)
        return true;

    uint32_t exportOffset = rvaToFileOffset(info, info.exportDirRVA);
    if (exportOffset == 0)
        return true;

    if (exportOffset + 40 > buf.size())
        return true;

    uint32_t numberOfFunctions = buf.readDword(exportOffset + 20);
    uint32_t numberOfNames = buf.readDword(exportOffset + 24);
    uint32_t addressOfFunctions = buf.readDword(exportOffset + 28);
    uint32_t addressOfNames = buf.readDword(exportOffset + 32);
    uint32_t addressOfOrdinals = buf.readDword(exportOffset + 36);
    uint32_t ordinalBase = buf.readDword(exportOffset + 16);

    uint32_t functionsOffset = rvaToFileOffset(info, addressOfFunctions);
    uint32_t namesOffset = rvaToFileOffset(info, addressOfNames);
    uint32_t ordinalsOffset = rvaToFileOffset(info, addressOfOrdinals);

    if (functionsOffset == 0)
        return true;

    if (numberOfNames > 65536)
        numberOfNames = 65536;

    for (uint32_t i = 0; i < numberOfNames; ++i) {
        PEExportEntry entry;

        if (namesOffset != 0) {
            size_t nameRVAOff = static_cast<size_t>(namesOffset) + i * 4;
            if (nameRVAOff + 4 > buf.size())
                break;

            uint32_t nameRVA = buf.readDword(nameRVAOff);
            uint32_t nameFileOff = rvaToFileOffset(info, nameRVA);
            if (nameFileOff != 0)
                entry.name = readAsciiString(buf, nameFileOff);
        }

        uint16_t ordinalIndex = 0;
        if (ordinalsOffset != 0) {
            size_t ordOff = static_cast<size_t>(ordinalsOffset) + i * 2;
            if (ordOff + 2 <= buf.size())
                ordinalIndex = buf.readWord(ordOff);
        }

        entry.ordinal = ordinalBase + ordinalIndex;

        if (ordinalIndex < numberOfFunctions) {
            size_t funcOff = static_cast<size_t>(functionsOffset) + ordinalIndex * 4;
            if (funcOff + 4 <= buf.size())
                entry.rva = buf.readDword(funcOff);
        }

        info.exports.push_back(std::move(entry));
    }

    return true;
}
