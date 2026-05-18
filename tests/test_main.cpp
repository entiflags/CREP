#include "mem_buffer.h"
#include "pe_parser.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

static int g_passed = 0;
static int g_failed = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            printf("  FAIL: %s\n", msg); \
            ++g_failed; \
        } else { \
            ++g_passed; \
        } \
    } while(0)

void test_membuffer_fromData()
{
    printf("[TEST] MemBuffer::fromData\n");

    uint8_t data[] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 };
    auto buf = MemBuffer::fromData(data, sizeof(data));

    TEST_ASSERT(buf != nullptr, "fromData should succeed");
    TEST_ASSERT(buf->size() == 8, "size should be 8");
    TEST_ASSERT(buf->readByte(0) == 0x11, "byte[0] == 0x11");
    TEST_ASSERT(buf->readByte(7) == 0x88, "byte[7] == 0x88");
}

void test_membuffer_bounds()
{
    printf("[TEST] MemBuffer bounds checking\n");

    uint8_t data[] = { 0xAA, 0xBB, 0xCC, 0xDD };
    auto buf = MemBuffer::fromData(data, 4);

    TEST_ASSERT(buf->readByte(4) == 0, "readByte out of bounds returns 0");
    TEST_ASSERT(buf->readWord(3) == 0, "readWord at offset 3 (needs 2 bytes, only 1 left) returns 0");
    TEST_ASSERT(buf->readDword(1) == 0, "readDword at offset 1 (needs 4 bytes, only 3 left) returns 0");
    TEST_ASSERT(buf->writeByte(4, 0xFF) == false, "writeByte out of bounds returns false");
    TEST_ASSERT(buf->readByte(3) == 0xDD, "buffer unchanged after failed write");
}

void test_membuffer_littleendian()
{
    printf("[TEST] MemBuffer little-endian reads\n");

    uint8_t data[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
    auto buf = MemBuffer::fromData(data, 8);

    TEST_ASSERT(buf->readWord(0) == 0x0201, "readWord LE");
    TEST_ASSERT(buf->readDword(0) == 0x04030201, "readDword LE");
    TEST_ASSERT(buf->readQword(0) == 0x0807060504030201ULL, "readQword LE");
}

void test_membuffer_write()
{
    printf("[TEST] MemBuffer write operations\n");

    uint8_t data[] = { 0x00, 0x00, 0x00, 0x00 };
    auto buf = MemBuffer::fromData(data, 4);

    TEST_ASSERT(buf->writeByte(0, 0xAB) == true, "writeByte succeeds");
    TEST_ASSERT(buf->readByte(0) == 0xAB, "read after write returns written value");
    TEST_ASSERT(buf->size() == 4, "size unchanged after write");
}

void test_membuffer_null_and_empty()
{
    printf("[TEST] MemBuffer null/empty cases\n");

    auto buf1 = MemBuffer::fromData(nullptr, 0);
    TEST_ASSERT(buf1 == nullptr, "fromData(nullptr, 0) returns nullptr");

    auto buf2 = MemBuffer::fromData(nullptr, 10);
    TEST_ASSERT(buf2 == nullptr, "fromData(nullptr, 10) returns nullptr");

    uint8_t data[] = { 0x01 };
    auto buf3 = MemBuffer::fromData(data, 0);
    TEST_ASSERT(buf3 == nullptr, "fromData(data, 0) returns nullptr");
}

void test_membuffer_readBytes()
{
    printf("[TEST] MemBuffer::readBytes\n");

    uint8_t data[] = { 0x10, 0x20, 0x30, 0x40, 0x50 };
    auto buf = MemBuffer::fromData(data, 5);

    uint8_t out[3] = {};
    TEST_ASSERT(buf->readBytes(1, out, 3) == true, "readBytes within bounds succeeds");
    TEST_ASSERT(out[0] == 0x20 && out[1] == 0x30 && out[2] == 0x40, "readBytes correct values");

    TEST_ASSERT(buf->readBytes(3, out, 3) == false, "readBytes crossing boundary fails");
    TEST_ASSERT(buf->readBytes(0, nullptr, 3) == false, "readBytes with null out fails");
}

void test_membuffer_writeBytes()
{
    printf("[TEST] MemBuffer::writeBytes\n");

    uint8_t data[] = { 0x00, 0x00, 0x00, 0x00, 0x00 };
    auto buf = MemBuffer::fromData(data, 5);

    uint8_t src[] = { 0xAA, 0xBB, 0xCC };
    TEST_ASSERT(buf->writeBytes(1, src, 3) == true, "writeBytes within bounds succeeds");
    TEST_ASSERT(buf->readByte(1) == 0xAA, "writeBytes[0] correct");
    TEST_ASSERT(buf->readByte(2) == 0xBB, "writeBytes[1] correct");
    TEST_ASSERT(buf->readByte(3) == 0xCC, "writeBytes[2] correct");

    TEST_ASSERT(buf->writeBytes(4, src, 3) == false, "writeBytes crossing boundary fails");
    TEST_ASSERT(buf->writeBytes(0, nullptr, 3) == false, "writeBytes with null data fails");
}

void test_peparser_too_small()
{
    printf("[TEST] PEParser: file too small\n");

    uint8_t data[32] = {};
    auto buf = MemBuffer::fromData(data, 32);

    PEParser parser;
    PEInfo info = parser.parse(*buf);

    TEST_ASSERT(info.isValid == false, "isValid should be false");
    TEST_ASSERT(!info.errorMessage.empty(), "errorMessage should not be empty");
    printf("  Error: %s\n", info.errorMessage.c_str());
}

void test_peparser_bad_mz()
{
    printf("[TEST] PEParser: invalid MZ signature\n");

    uint8_t data[128] = {};
    data[0] = 'X';
    data[1] = 'X';
    auto buf = MemBuffer::fromData(data, 128);

    PEParser parser;
    PEInfo info = parser.parse(*buf);

    TEST_ASSERT(info.isValid == false, "isValid should be false");
    TEST_ASSERT(info.errorMessage.find("MZ") != std::string::npos, "error mentions MZ");
}

void test_peparser_bad_pe_sig()
{
    printf("[TEST] PEParser: invalid PE signature\n");

    uint8_t data[256] = {};
    data[0] = 'M';
    data[1] = 'Z';
    data[0x3C] = 0x80;
    data[0x80] = 0xFF;
    data[0x81] = 0xFF;
    data[0x82] = 0xFF;
    data[0x83] = 0xFF;

    auto buf = MemBuffer::fromData(data, 256);

    PEParser parser;
    PEInfo info = parser.parse(*buf);

    TEST_ASSERT(info.isValid == false, "isValid should be false");
    TEST_ASSERT(info.errorMessage.find("PE") != std::string::npos, "error mentions PE");
}

void test_peparser_real_file()
{
    printf("[TEST] PEParser: parse real PE (notepad.exe)\n");

    auto buf = MemBuffer::fromFile(L"C:\\Windows\\System32\\notepad.exe");
    if (!buf) {
        printf("  SKIP: Could not open notepad.exe\n");
        return;
    }

    PEParser parser;
    PEInfo info = parser.parse(*buf);

    TEST_ASSERT(info.isValid == true, "notepad.exe should be valid PE");
    TEST_ASSERT(info.numberOfSections > 0, "should have sections");
    TEST_ASSERT(info.sections.size() == info.numberOfSections, "sections count matches");
    TEST_ASSERT(info.entryPointRVA != 0, "entry point should be non-zero");
    TEST_ASSERT(info.imageBase != 0, "image base should be non-zero");
    TEST_ASSERT(!info.imports.empty(), "should have imports");

    printf("  Type: %s\n", info.is64Bit ? "PE32+" : "PE32");
    printf("  Sections: %u\n", info.numberOfSections);
    printf("  Imports: %zu DLLs\n", info.imports.size());
    printf("  Entry Point RVA: 0x%08X\n", info.entryPointRVA);

    // RVA conversion
    uint32_t epOffset = PEParser::rvaToFileOffset(info, info.entryPointRVA);
    TEST_ASSERT(epOffset != 0, "entry point RVA converts to valid offset");
    TEST_ASSERT(epOffset < buf->size(), "converted offset within file");

    // Round-trip
    uint32_t backToRva = PEParser::fileOffsetToRva(info, epOffset);
    TEST_ASSERT(backToRva == info.entryPointRVA, "RVA round-trip matches");

    printf("  EP file offset: 0x%08X (round-trip OK)\n", epOffset);
}

void test_peparser_kernel32()
{
    printf("[TEST] PEParser: parse kernel32.dll (exports)\n");

    auto buf = MemBuffer::fromFile(L"C:\\Windows\\System32\\kernel32.dll");
    if (!buf) {
        printf("  SKIP: Could not open kernel32.dll\n");
        return;
    }

    PEParser parser;
    PEInfo info = parser.parse(*buf);

    TEST_ASSERT(info.isValid == true, "kernel32.dll should be valid PE");
    TEST_ASSERT(!info.exports.empty(), "kernel32 should have exports");

    printf("  Exports: %zu functions\n", info.exports.size());
    if (!info.exports.empty()) {
        printf("  First export: [%u] %s\n",
               info.exports[0].ordinal, info.exports[0].name.c_str());
    }
}

void test_peparser_rva_out_of_range()
{
    printf("[TEST] PEParser: RVA out of range returns 0\n");

    auto buf = MemBuffer::fromFile(L"C:\\Windows\\System32\\notepad.exe");
    if (!buf) {
        printf("  SKIP: Could not open notepad.exe\n");
        return;
    }

    PEParser parser;
    PEInfo info = parser.parse(*buf);
    if (!info.isValid) {
        printf("  SKIP: parse failed\n");
        return;
    }

    // RVA that's way beyond any section
    uint32_t result = PEParser::rvaToFileOffset(info, 0xFFFF0000);
    TEST_ASSERT(result == 0, "out-of-range RVA returns 0");

    // File offset beyond any section
    uint32_t result2 = PEParser::fileOffsetToRva(info, 0xFFFF0000);
    TEST_ASSERT(result2 == 0, "out-of-range file offset returns 0");
}

int main()
{
    printf("========================================\n");
    printf("  C.R.E.P Tests\n");
    printf("========================================\n\n");

    // MemBuffer tests
    test_membuffer_fromData();
    test_membuffer_bounds();
    test_membuffer_littleendian();
    test_membuffer_write();
    test_membuffer_null_and_empty();
    test_membuffer_readBytes();
    test_membuffer_writeBytes();

    printf("\n");

    // PEParser tests
    test_peparser_too_small();
    test_peparser_bad_mz();
    test_peparser_bad_pe_sig();
    test_peparser_real_file();
    test_peparser_kernel32();
    test_peparser_rva_out_of_range();

    printf("\n========================================\n");
    printf("  Results: %d passed, %d failed\n", g_passed, g_failed);
    printf("========================================\n");

    printf("\nPress Enter to exit...");
    getchar();

    return g_failed > 0 ? 1 : 0;
}
