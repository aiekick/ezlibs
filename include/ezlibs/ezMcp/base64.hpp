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

// ezMcp is part of the ezLibs project : https://github.com/aiekick/ezLibs.git

// Base64 encoding (RFC 4648, standard alphabet, padded) : the bytes of an
// image inside an MCP tool result.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ez {
namespace mcp {

class Base64 {
public:
    static std::string encode(const std::vector<uint8_t>& aBytes) {
        static const char* const sAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;
        out.reserve(((aBytes.size() + 2U) / 3U) * 4U);
        size_t idx = 0U;
        for (; (idx + 3U) <= aBytes.size(); idx += 3U) {
            const uint32_t triple = (static_cast<uint32_t>(aBytes[idx]) << 16U) | (static_cast<uint32_t>(aBytes[idx + 1U]) << 8U) | aBytes[idx + 2U];
            out += sAlphabet[(triple >> 18U) & 0x3FU];
            out += sAlphabet[(triple >> 12U) & 0x3FU];
            out += sAlphabet[(triple >> 6U) & 0x3FU];
            out += sAlphabet[triple & 0x3FU];
        }
        const size_t rest = aBytes.size() - idx;
        if (rest == 1U) {
            const uint32_t single = static_cast<uint32_t>(aBytes[idx]) << 16U;
            out += sAlphabet[(single >> 18U) & 0x3FU];
            out += sAlphabet[(single >> 12U) & 0x3FU];
            out += "==";
        } else if (rest == 2U) {
            const uint32_t pair = (static_cast<uint32_t>(aBytes[idx]) << 16U) | (static_cast<uint32_t>(aBytes[idx + 1U]) << 8U);
            out += sAlphabet[(pair >> 18U) & 0x3FU];
            out += sAlphabet[(pair >> 12U) & 0x3FU];
            out += sAlphabet[(pair >> 6U) & 0x3FU];
            out += '=';
        }
        return out;
    }
};

}  // namespace mcp
}  // namespace ez
