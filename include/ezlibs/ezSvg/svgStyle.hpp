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

// ezSvg is part of the ezLibs project : https://github.com/aiekick/ezLibs.git
//
// the STYLE attribute (style="fill:#fff; fill-opacity:.5") split into
// its properties — keys lower cased and trimmed, values trimmed. a
// style property wins over the presentation attribute of the same
// name (the css cascade of svg 1.1 6.4)

#include <cstddef>
#include <map>
#include <string>

namespace ez {
namespace svg {

class StyleParser {
public:
    static void parse(const std::string& aStyle, std::map<std::string, std::string>& aoProperties) {
        std::size_t pos = 0;
        while (pos < aStyle.size()) {
            std::size_t end = aStyle.find(';', pos);
            if (end == std::string::npos) {
                end = aStyle.size();
            }
            const std::string declaration = aStyle.substr(pos, end - pos);
            const std::size_t colon = declaration.find(':');
            if (colon != std::string::npos) {
                const std::string key = toLower(trim(declaration.substr(0, colon)));
                const std::string value = trim(declaration.substr(colon + 1));
                if (!key.empty()) {
                    aoProperties[key] = value;
                }
            }
            pos = end + 1;
        }
    }
    static std::string trim(const std::string& aText) {
        std::size_t first = 0;
        std::size_t last = aText.size();
        while ((first < last) && isSpace(aText[first])) {
            ++first;
        }
        while ((last > first) && isSpace(aText[last - 1])) {
            --last;
        }
        return aText.substr(first, last - first);
    }
    static std::string toLower(const std::string& aText) {
        std::string result = aText;
        for (std::size_t charIdx = 0; charIdx < result.size(); ++charIdx) {
            const char ch = result[charIdx];
            if ((ch >= 'A') && (ch <= 'Z')) {
                result[charIdx] = static_cast<char>(ch - 'A' + 'a');
            }
        }
        return result;
    }

private:
    static bool isSpace(char aChar) {
        return (aChar == ' ') || (aChar == '\t') || (aChar == '\n') || (aChar == '\r') || (aChar == '\f') || (aChar == '\v');
    }
};

}  // namespace svg
}  // namespace ez
