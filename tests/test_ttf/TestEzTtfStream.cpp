#include <TestEzTtfStream.h>
#include <ezlibs/ezCTest.hpp>
#include <ezlibs/ezTTF/ezTTF.hpp>

#include <cstring>

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

// the format is big-endian : checked BYTE BY BYTE, not through a
// read-back (a symmetric endianness bug would round-trip fine)
bool TestEzTtfStream_WritesBigEndianBytes() {
    ez::ttf::Stream stream;
    stream.writeU16(0x1234u);
    stream.writeU24(0x0A0B0Cu);
    stream.writeU32(0xDEADBEEFu);
    CTEST_ASSERT(stream.getSize() == 9u);
    const uint8_t* pBytes = stream.getBytes();
    CTEST_ASSERT(pBytes[0] == 0x12u && pBytes[1] == 0x34u);
    CTEST_ASSERT(pBytes[2] == 0x0Au && pBytes[3] == 0x0Bu && pBytes[4] == 0x0Cu);
    CTEST_ASSERT(pBytes[5] == 0xDEu && pBytes[6] == 0xADu && pBytes[7] == 0xBEu && pBytes[8] == 0xEFu);
    return true;
}

// every scalar the tables speak, written then read back — negatives
// included (the signed reads go through an unsigned cast)
bool TestEzTtfStream_RoundTripsEveryType() {
    ez::ttf::Stream stream;
    stream.writeU8(0xABu);
    stream.writeU16(0xFFFEu);
    stream.writeI16(-12345);
    stream.writeU24(0xFFFFFEu);
    stream.writeU32(0xFFFFFFFEu);
    stream.writeI32(-123456789);
    stream.writeI64(-1234567890123456789LL);
    ez::ttf::Fixed fixed;
    fixed.setFloat(-1.5f);
    stream.writeFixed(fixed);
    ez::ttf::F2DOT14 scale;
    scale.setFloat(-2.0f);
    stream.writeF2DOT14(scale);
    stream.writeTag(ez::ttf::kTagGlyf);
    const uint8_t rawBytes[3] = {1u, 2u, 3u};
    stream.writeBytes(rawBytes, 3u);

    CTEST_ASSERT(stream.readU8() == 0xABu);
    CTEST_ASSERT(stream.readU16() == 0xFFFEu);
    CTEST_ASSERT(stream.readI16() == -12345);
    CTEST_ASSERT(stream.readU24() == 0xFFFFFEu);
    CTEST_ASSERT(stream.readU32() == 0xFFFFFFFEu);
    CTEST_ASSERT(stream.readI32() == -123456789);
    CTEST_ASSERT(stream.readI64() == -1234567890123456789LL);
    CTEST_ASSERT(stream.readFixed().raw == fixed.raw);
    CTEST_ASSERT(stream.readF2DOT14().raw == scale.raw);
    CTEST_ASSERT(stream.readTag() == ez::ttf::kTagGlyf);
    uint8_t readBack[3] = {0u, 0u, 0u};
    CTEST_ASSERT(stream.readBytes(readBack, 3u));
    CTEST_ASSERT(std::memcmp(rawBytes, readBack, 3u) == 0);
    CTEST_ASSERT(stream.ok());
    CTEST_ASSERT(stream.getReadPos() == stream.getSize());
    return true;
}

// a read past the end answers 0 and LATCHES the error — sticky : a later
// in-bounds read still succeeds as a read, but ok() stays false. this is
// the whole corrupt-font defense
bool TestEzTtfStream_OutOfBoundsReadLatchesTheError() {
    const uint8_t twoBytes[2] = {0x11u, 0x22u};
    ez::ttf::Stream stream(twoBytes, 2u);
    CTEST_ASSERT(stream.ok());
    CTEST_ASSERT(stream.readU32() == 0u);  // 4 asked, 2 present
    CTEST_ASSERT(!stream.ok());
    CTEST_ASSERT(stream.getReadPos() == 0u);  // a refused read moves nothing
    CTEST_ASSERT(stream.readU16() == 0x1122u);  // still readable...
    CTEST_ASSERT(!stream.ok());                 // ...but the latch holds
    return true;
}

bool TestEzTtfStream_EmptyStreamRefusesEveryRead() {
    ez::ttf::Stream stream;
    CTEST_ASSERT(stream.readU8() == 0u);
    CTEST_ASSERT(!stream.ok());
    return true;
}

// a seek past the end is an error (a corrupt table directory offset must
// not fake a valid position) ; a seek AT the end is legal, reads there fail
bool TestEzTtfStream_SeekPastEndLatchesTheError() {
    const uint8_t fourBytes[4] = {1u, 2u, 3u, 4u};
    ez::ttf::Stream stream(fourBytes, 4u);
    stream.setReadPos(4u);  // the end : a legal position
    CTEST_ASSERT(stream.ok());
    stream.setReadPos(2u);
    CTEST_ASSERT(stream.readU16() == 0x0304u);
    stream.setReadPos(5u);  // past the end
    CTEST_ASSERT(!stream.ok());
    return true;
}

// all-or-nothing : a partial trailing read never fills half a buffer
bool TestEzTtfStream_ReadBytesIsAllOrNothing() {
    const uint8_t threeBytes[3] = {7u, 8u, 9u};
    ez::ttf::Stream stream(threeBytes, 3u);
    uint8_t sink[5] = {0u, 0u, 0u, 0u, 0u};
    CTEST_ASSERT(!stream.readBytes(sink, 5u));
    CTEST_ASSERT(!stream.ok());
    CTEST_ASSERT(sink[0] == 0u);                // untouched
    CTEST_ASSERT(stream.getReadPos() == 0u);    // unmoved
    CTEST_ASSERT(stream.readBytes(sink, 3u));   // the honest size passes
    CTEST_ASSERT(sink[0] == 7u && sink[2] == 9u);
    return true;
}

// checksums are computed on 4-byte units : the writer pads each table
bool TestEzTtfStream_PadToLongAligns() {
    ez::ttf::Stream stream;
    stream.writeU8(1u);
    stream.writeU8(2u);
    stream.writeU8(3u);
    stream.padToLong();
    CTEST_ASSERT(stream.getSize() == 4u);
    CTEST_ASSERT(stream.getBytes()[3] == 0u);
    stream.padToLong();  // already aligned : untouched
    CTEST_ASSERT(stream.getSize() == 4u);
    return true;
}

// an EMPTY read at the very end of the stream is legal : a post table
// whose last custom name is empty ends exactly there (the read used to
// index one past the end)
bool TestEzTtfStream_AnEmptyReadAtTheEndIsLegal() {
    const uint8_t bytes[] = {0x03u, 'a', 'b', 'c', 0x00u};
    ez::ttf::Stream stream(bytes, sizeof(bytes));
    CTEST_ASSERT(stream.readU8() == 3u);
    CTEST_ASSERT(stream.readString(3u) == "abc");
    CTEST_ASSERT(stream.readU8() == 0u);
    CTEST_ASSERT(stream.getReadPos() == sizeof(bytes));
    CTEST_ASSERT(stream.readString(0u).empty());
    CTEST_ASSERT(stream.ok());
    uint8_t sink[1] = {0xFFu};
    CTEST_ASSERT(stream.readBytes(sink, 0u));
    CTEST_ASSERT(stream.ok());
    CTEST_ASSERT(stream.readString(1u).empty());
    CTEST_ASSERT(!stream.ok());  // one byte past the end : the error latches
    return true;
}

bool TestEzTtfStream_AppendStreamAndString() {
    ez::ttf::Stream tableStream;
    tableStream.writeTag(ez::ttf::kTagName);
    ez::ttf::Stream fontStream;
    fontStream.writeU16(1u);
    fontStream.appendStream(tableStream);
    CTEST_ASSERT(fontStream.getSize() == 6u);
    fontStream.setReadPos(2u);
    CTEST_ASSERT(fontStream.readString(4u) == "name");
    CTEST_ASSERT(fontStream.ok());
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzTtfStream(const std::string& vTest) {
    IfTestExist(TestEzTtfStream_WritesBigEndianBytes);
    else IfTestExist(TestEzTtfStream_RoundTripsEveryType);
    else IfTestExist(TestEzTtfStream_OutOfBoundsReadLatchesTheError);
    else IfTestExist(TestEzTtfStream_EmptyStreamRefusesEveryRead);
    else IfTestExist(TestEzTtfStream_SeekPastEndLatchesTheError);
    else IfTestExist(TestEzTtfStream_ReadBytesIsAllOrNothing);
    else IfTestExist(TestEzTtfStream_PadToLongAligns);
    else IfTestExist(TestEzTtfStream_AppendStreamAndString);
    else IfTestExist(TestEzTtfStream_AnEmptyReadAtTheEndIsLegal);
    return false;
}
