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
// the SCANNER the svg grammars share : path data, transform lists, point
// lists, colors. svg numbers are locale-free (a dot, an optional
// exponent) and glued at will ("-1.5-2" is two of them, "1.5.5" too) :
// the reader never calls the c locale

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>

namespace ez {
namespace svg {

class Scanner {
private:
    std::string m_text;  // a COPY : a scanner built on a temporary stays valid
    std::size_t m_pos{0};

public:
    explicit Scanner(const std::string& aText) : m_text(aText) {}
    bool atEnd() const {
        return m_pos >= m_text.size();
    }
    char peek() const {
        return atEnd() ? '\0' : m_text[m_pos];
    }
    void advance() {
        if (!atEnd()) {
            ++m_pos;
        }
    }
    std::size_t getPos() const {
        return m_pos;
    }
    void skipSpaces() {
        while (!atEnd() && isSpace(m_text[m_pos])) {
            ++m_pos;
        }
    }
    // white space and commas : the separators of every svg list
    void skipSeparators() {
        while (!atEnd() && (isSpace(m_text[m_pos]) || (m_text[m_pos] == ','))) {
            ++m_pos;
        }
    }
    // after the spaces, is the next character the start of a number ?
    bool isNumberStart() const {
        std::size_t pos = m_pos;
        while ((pos < m_text.size()) && (isSpace(m_text[pos]) || (m_text[pos] == ','))) {
            ++pos;
        }
        if (pos >= m_text.size()) {
            return false;
        }
        const char ch = m_text[pos];
        return isDigit(ch) || (ch == '.') || (ch == '+') || (ch == '-');
    }
    // a number : sign, digits, a dot, digits, an exponent — at least one
    // digit somewhere. false leaves the position untouched
    bool readNumber(double& aoValue) {
        std::size_t pos = m_pos;
        bool negative = false;
        if ((pos < m_text.size()) && ((m_text[pos] == '+') || (m_text[pos] == '-'))) {
            negative = (m_text[pos] == '-');
            ++pos;
        }
        double mantissa = 0.0;
        bool anyDigit = false;
        while ((pos < m_text.size()) && isDigit(m_text[pos])) {
            mantissa = mantissa * 10.0 + static_cast<double>(m_text[pos] - '0');
            anyDigit = true;
            ++pos;
        }
        if ((pos < m_text.size()) && (m_text[pos] == '.')) {
            ++pos;
            double scale = 0.1;
            while ((pos < m_text.size()) && isDigit(m_text[pos])) {
                mantissa += static_cast<double>(m_text[pos] - '0') * scale;
                scale *= 0.1;
                anyDigit = true;
                ++pos;
            }
        }
        if (!anyDigit) {
            return false;
        }
        if ((pos < m_text.size()) && ((m_text[pos] == 'e') || (m_text[pos] == 'E'))) {
            std::size_t exponentPos = pos + 1;
            bool exponentNegative = false;
            if ((exponentPos < m_text.size()) && ((m_text[exponentPos] == '+') || (m_text[exponentPos] == '-'))) {
                exponentNegative = (m_text[exponentPos] == '-');
                ++exponentPos;
            }
            int32_t exponent = 0;
            bool anyExponentDigit = false;
            while ((exponentPos < m_text.size()) && isDigit(m_text[exponentPos])) {
                exponent = exponent * 10 + (m_text[exponentPos] - '0');
                anyExponentDigit = true;
                ++exponentPos;
            }
            if (anyExponentDigit) {  // "1e" alone : the e is not ours
                mantissa *= std::pow(10.0, static_cast<double>(exponentNegative ? -exponent : exponent));
                pos = exponentPos;
            }
        }
        aoValue = negative ? -mantissa : mantissa;
        m_pos = pos;
        return true;
    }
    // an arc flag : a single '0' or '1', glued or not
    bool readFlag(bool& aoFlag) {
        if (atEnd()) {
            return false;
        }
        const char ch = m_text[m_pos];
        if ((ch != '0') && (ch != '1')) {
            return false;
        }
        aoFlag = (ch == '1');
        ++m_pos;
        return true;
    }
    // an identifier : a letter or an underscore, then letters, digits,
    // underscores and dashes
    bool readIdentifier(std::string& aoName) {
        std::size_t pos = m_pos;
        if ((pos >= m_text.size()) || !(isAlpha(m_text[pos]) || (m_text[pos] == '_'))) {
            return false;
        }
        while ((pos < m_text.size()) && (isAlpha(m_text[pos]) || isDigit(m_text[pos]) || (m_text[pos] == '_') || (m_text[pos] == '-'))) {
            ++pos;
        }
        aoName = m_text.substr(m_pos, pos - m_pos);
        m_pos = pos;
        return true;
    }
    // the next non-space character must be aChar : consumed, or false
    bool expect(char aChar) {
        skipSpaces();
        if (peek() != aChar) {
            return false;
        }
        ++m_pos;
        return true;
    }
    static bool isSpace(char aChar) {
        return (aChar == ' ') || (aChar == '\t') || (aChar == '\n') || (aChar == '\r') || (aChar == '\f') || (aChar == '\v');
    }
    static bool isDigit(char aChar) {
        return (aChar >= '0') && (aChar <= '9');
    }
    static bool isAlpha(char aChar) {
        return ((aChar >= 'a') && (aChar <= 'z')) || ((aChar >= 'A') && (aChar <= 'Z'));
    }
};

}  // namespace svg
}  // namespace ez
