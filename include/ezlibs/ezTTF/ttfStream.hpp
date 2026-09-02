#pragma once

/*
MIT License

Copyright (c) 2014-2026 Stephane Cuillerdier (aka aiekick)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

// ezTTF is part of the ezLibs project : https://github.com/aiekick/ezLibs.git
// the big-endian byte stream every table reader/writer speaks (the
// ttfrrw MemoryStream role). the ONE design rule : every read is
// BOUNDED — a corrupt font may lie about any offset or length, and the
// answer is a latched error, never undefined behavior. ttfrrw read
// unguarded ; this one refuses.
//
// error contract : a read past the end returns 0 (of the asked type)
// and latches ok() to false — STICKY, later valid reads do not clear
// it. the caller checks ok() once per table, not once per field

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "ttfTypes.hpp"

namespace ez {
namespace ttf {

class Stream {
private:
    std::vector<uint8_t> m_bytes;
    std::size_t m_readPos;
    bool m_ok;

public:
    Stream() : m_readPos(0), m_ok(true) {}
    Stream(const uint8_t* apBytes, std::size_t aByteCount) : m_readPos(0), m_ok(true) { setBytes(apBytes, aByteCount); }

    // --- storage ---
    void setBytes(const uint8_t* apBytes, std::size_t aByteCount) {
        m_bytes.assign(apBytes, apBytes + aByteCount);
        m_readPos = 0;
        m_ok = (apBytes != nullptr || aByteCount == 0);
    }
    const uint8_t* getBytes() const { return m_bytes.empty() ? nullptr : &m_bytes[0]; }
    std::size_t getSize() const { return m_bytes.size(); }
    bool ok() const { return m_ok; }

    // --- read cursor ---
    std::size_t getReadPos() const { return m_readPos; }
    // a seek past the end latches the error (an offset read from a
    // corrupt directory must not fake a valid position)
    void setReadPos(std::size_t aPos) {
        if (aPos > m_bytes.size()) {
            m_ok = false;
            return;
        }
        m_readPos = aPos;
    }

    // --- writes : append at the end, big-endian ---
    void writeU8(uint8_t aValue) { m_bytes.push_back(aValue); }
    void writeU16(uint16_t aValue) {
        m_bytes.push_back(static_cast<uint8_t>(aValue >> 8));
        m_bytes.push_back(static_cast<uint8_t>(aValue & 0xFFu));
    }
    void writeI16(int16_t aValue) { writeU16(static_cast<uint16_t>(aValue)); }
    void writeU24(uint32_t aValue) {
        m_bytes.push_back(static_cast<uint8_t>((aValue >> 16) & 0xFFu));
        m_bytes.push_back(static_cast<uint8_t>((aValue >> 8) & 0xFFu));
        m_bytes.push_back(static_cast<uint8_t>(aValue & 0xFFu));
    }
    void writeU32(uint32_t aValue) {
        m_bytes.push_back(static_cast<uint8_t>(aValue >> 24));
        m_bytes.push_back(static_cast<uint8_t>((aValue >> 16) & 0xFFu));
        m_bytes.push_back(static_cast<uint8_t>((aValue >> 8) & 0xFFu));
        m_bytes.push_back(static_cast<uint8_t>(aValue & 0xFFu));
    }
    void writeI32(int32_t aValue) { writeU32(static_cast<uint32_t>(aValue)); }
    void writeI64(int64_t aValue) {
        writeU32(static_cast<uint32_t>(static_cast<uint64_t>(aValue) >> 32));
        writeU32(static_cast<uint32_t>(static_cast<uint64_t>(aValue) & 0xFFFFFFFFu));
    }
    void writeFixed(Fixed aValue) { writeI32(aValue.raw); }
    void writeF2DOT14(F2DOT14 aValue) { writeI16(aValue.raw); }
    void writeTag(TableTag aTag) { writeU32(aTag); }
    void writeBytes(const uint8_t* apBytes, std::size_t aByteCount) {
        if (apBytes == nullptr && aByteCount != 0) {
            m_ok = false;
            return;
        }
        m_bytes.insert(m_bytes.end(), apBytes, apBytes + aByteCount);
    }
    void appendStream(const Stream& aOther) {
        m_bytes.insert(m_bytes.end(), aOther.m_bytes.begin(), aOther.m_bytes.end());
    }
    // table checksums are computed on 4-byte units : the writer pads every
    // assembled table to that boundary before appending the next one
    void padToLong() {
        while ((m_bytes.size() % 4u) != 0u) {
            m_bytes.push_back(0u);
        }
    }

    // --- reads : at the cursor, advancing. 0 + latched error on overflow ---
    uint8_t readU8() {
        if (!m_canRead(1)) {
            return 0;
        }
        return m_bytes[m_readPos++];
    }
    uint16_t readU16() {
        if (!m_canRead(2)) {
            return 0;
        }
        const uint16_t high = m_bytes[m_readPos];
        const uint16_t low = m_bytes[m_readPos + 1];
        m_readPos += 2;
        return static_cast<uint16_t>((high << 8) | low);
    }
    // the unsigned-to-signed cast is implementation defined pre-C++20 but
    // two's complement everywhere ezlibs builds (msvc, clang, gcc)
    int16_t readI16() { return static_cast<int16_t>(readU16()); }
    uint32_t readU24() {
        if (!m_canRead(3)) {
            return 0;
        }
        const uint32_t byte0 = m_bytes[m_readPos];
        const uint32_t byte1 = m_bytes[m_readPos + 1];
        const uint32_t byte2 = m_bytes[m_readPos + 2];
        m_readPos += 3;
        return (byte0 << 16) | (byte1 << 8) | byte2;
    }
    uint32_t readU32() {
        if (!m_canRead(4)) {
            return 0;
        }
        const uint32_t byte0 = m_bytes[m_readPos];
        const uint32_t byte1 = m_bytes[m_readPos + 1];
        const uint32_t byte2 = m_bytes[m_readPos + 2];
        const uint32_t byte3 = m_bytes[m_readPos + 3];
        m_readPos += 4;
        return (byte0 << 24) | (byte1 << 16) | (byte2 << 8) | byte3;
    }
    int32_t readI32() { return static_cast<int32_t>(readU32()); }
    int64_t readI64() {
        const uint64_t high = readU32();
        const uint64_t low = readU32();
        return static_cast<int64_t>((high << 32) | low);
    }
    Fixed readFixed() { return Fixed(readI32()); }
    F2DOT14 readF2DOT14() { return F2DOT14(readI16()); }
    TableTag readTag() { return readU32(); }
    // all-or-nothing : a partial trailing read never fills half a buffer
    bool readBytes(uint8_t* aoBytes, std::size_t aByteCount) {
        if (aoBytes == nullptr || !m_canRead(aByteCount)) {
            return false;
        }
        if (aByteCount != 0) {
            std::memcpy(aoBytes, &m_bytes[m_readPos], aByteCount);
        }
        m_readPos += aByteCount;
        return true;
    }
    std::string readString(std::size_t aLength) {
        if (!m_canRead(aLength)) {
            return std::string();
        }
        if (aLength == 0) {
            return std::string();  // an empty string at the very end : legal, nothing to index
        }
        std::string ret(reinterpret_cast<const char*>(&m_bytes[m_readPos]), aLength);
        m_readPos += aLength;
        return ret;
    }

private:
    bool m_canRead(std::size_t aByteCount) {
        if (aByteCount > m_bytes.size() - m_readPos) {  // m_readPos <= size() always holds
            m_ok = false;
            return false;
        }
        return true;
    }
};

}  // namespace ttf
}  // namespace ez
