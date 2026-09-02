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
// the PAINT grammar of fill and stroke : none, currentColor, transparent,
// #rgb #rgba #rrggbb #rrggbbaa, rgb() rgba() (numbers or percents),
// hsl() hsla(), the 147 svg color keywords (and rebeccapurple), and a
// url(#reference) kept as an UNSUPPORTED paint (a gradient, a pattern)

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>

#include "svgTypes.hpp"
#include "svgScanner.hpp"
#include "svgStyle.hpp"

namespace ez {
namespace svg {

class ColorParser {
private:
    struct NamedColor {
        const char* name;
        uint8_t red;
        uint8_t green;
        uint8_t blue;
    };

public:
    // false when the text is no paint at all (aoPaint untouched then)
    static bool parse(const std::string& aText, Paint& aoPaint) {
        const std::string value = StyleParser::toLower(StyleParser::trim(aText));
        if (value.empty()) {
            return false;
        }
        if (value == "none") {
            aoPaint = Paint::none();
            return true;
        }
        if (value == "currentcolor") {
            aoPaint = Paint::currentColor();
            return true;
        }
        if (value == "transparent") {
            aoPaint = Paint::color(0, 0, 0, 0);
            return true;
        }
        if (value[0] == '#') {
            return m_parseHex(value.substr(1), aoPaint);
        }
        if ((value.compare(0, 4, "rgb(") == 0) || (value.compare(0, 5, "rgba(") == 0)) {
            return m_parseRgb(value.substr(value.find('(') + 1), aoPaint);
        }
        if ((value.compare(0, 4, "hsl(") == 0) || (value.compare(0, 5, "hsla(") == 0)) {
            return m_parseHsl(value.substr(value.find('(') + 1), aoPaint);
        }
        if (value.compare(0, 4, "url(") == 0) {
            const std::size_t close = value.find(')');
            aoPaint = Paint::unsupported(StyleParser::trim(value.substr(4, (close == std::string::npos) ? std::string::npos : (close - 4))));
            return true;
        }
        const NamedColor* pNamed = m_findNamed(value);
        if (pNamed != nullptr) {
            aoPaint = Paint::color(pNamed->red, pNamed->green, pNamed->blue);
            return true;
        }
        return false;
    }

private:
    static int32_t m_hexDigit(char aChar) {
        if ((aChar >= '0') && (aChar <= '9')) {
            return aChar - '0';
        }
        if ((aChar >= 'a') && (aChar <= 'f')) {
            return aChar - 'a' + 10;
        }
        return -1;
    }
    static bool m_parseHex(const std::string& aDigits, Paint& aoPaint) {
        const std::size_t count = aDigits.size();
        if ((count != 3u) && (count != 4u) && (count != 6u) && (count != 8u)) {
            return false;
        }
        int32_t values[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        for (std::size_t digitIdx = 0; digitIdx < count; ++digitIdx) {
            values[digitIdx] = m_hexDigit(aDigits[digitIdx]);
            if (values[digitIdx] < 0) {
                return false;
            }
        }
        uint8_t channels[4] = {0, 0, 0, 255};
        if (count <= 4u) {
            for (std::size_t channelIdx = 0; channelIdx < count; ++channelIdx) {
                channels[channelIdx] = static_cast<uint8_t>(values[channelIdx] * 17);  // the short form doubles its digit
            }
        } else {
            for (std::size_t channelIdx = 0; channelIdx < count / 2u; ++channelIdx) {
                channels[channelIdx] = static_cast<uint8_t>(values[channelIdx * 2u] * 16 + values[channelIdx * 2u + 1u]);
            }
        }
        aoPaint = Paint::color(channels[0], channels[1], channels[2], channels[3]);
        return true;
    }
    // one component of rgb() / hsl() : a number, an optional percent sign
    static bool m_readComponent(Scanner& aScanner, double& aoValue, bool& aoPercent) {
        aScanner.skipSeparators();
        while (aScanner.peek() == '/') {  // the css4 alpha separator
            aScanner.advance();
            aScanner.skipSeparators();
        }
        if (!aScanner.readNumber(aoValue)) {
            return false;
        }
        aoPercent = false;
        if (aScanner.peek() == '%') {
            aoPercent = true;
            aScanner.advance();
        }
        return true;
    }
    static uint8_t m_clampChannel(double aValue) {
        if (aValue <= 0.0) {
            return 0;
        }
        if (aValue >= 255.0) {
            return 255;
        }
        return static_cast<uint8_t>(aValue + 0.5);
    }
    static uint8_t m_alphaChannel(double aValue, bool aPercent) {
        const double unit = aPercent ? (aValue / 100.0) : aValue;
        return m_clampChannel(unit * 255.0);
    }
    static bool m_parseRgb(const std::string& aInside, Paint& aoPaint) {
        Scanner scanner(aInside);
        double values[4] = {0.0, 0.0, 0.0, 1.0};
        bool percents[4] = {false, false, false, false};
        std::size_t count = 0;
        while (count < 4u) {
            if (!m_readComponent(scanner, values[count], percents[count])) {
                break;
            }
            ++count;
        }
        if (count < 3u) {
            return false;
        }
        uint8_t channels[3] = {0, 0, 0};
        for (std::size_t channelIdx = 0; channelIdx < 3u; ++channelIdx) {
            channels[channelIdx] = m_clampChannel(percents[channelIdx] ? (values[channelIdx] * 255.0 / 100.0) : values[channelIdx]);
        }
        const uint8_t alpha = (count == 4u) ? m_alphaChannel(values[3], percents[3]) : static_cast<uint8_t>(255);
        aoPaint = Paint::color(channels[0], channels[1], channels[2], alpha);
        return true;
    }
    static double m_hueToChannel(double aP, double aQ, double aT) {
        double t = aT;
        if (t < 0.0) {
            t += 1.0;
        }
        if (t > 1.0) {
            t -= 1.0;
        }
        if (t < 1.0 / 6.0) {
            return aP + (aQ - aP) * 6.0 * t;
        }
        if (t < 0.5) {
            return aQ;
        }
        if (t < 2.0 / 3.0) {
            return aP + (aQ - aP) * (2.0 / 3.0 - t) * 6.0;
        }
        return aP;
    }
    static bool m_parseHsl(const std::string& aInside, Paint& aoPaint) {
        Scanner scanner(aInside);
        double values[4] = {0.0, 0.0, 0.0, 1.0};
        bool percents[4] = {false, false, false, false};
        std::size_t count = 0;
        while (count < 4u) {
            if (!m_readComponent(scanner, values[count], percents[count])) {
                break;
            }
            ++count;
        }
        if (count < 3u) {
            return false;
        }
        const double hue = std::fmod(std::fmod(values[0], 360.0) + 360.0, 360.0) / 360.0;
        const double saturation = (values[1] > 100.0) ? 1.0 : ((values[1] < 0.0) ? 0.0 : (values[1] / 100.0));
        const double lightness = (values[2] > 100.0) ? 1.0 : ((values[2] < 0.0) ? 0.0 : (values[2] / 100.0));
        double red = lightness;
        double green = lightness;
        double blue = lightness;
        if (saturation > 0.0) {
            const double q = (lightness < 0.5) ? (lightness * (1.0 + saturation)) : (lightness + saturation - lightness * saturation);
            const double p = 2.0 * lightness - q;
            red = m_hueToChannel(p, q, hue + 1.0 / 3.0);
            green = m_hueToChannel(p, q, hue);
            blue = m_hueToChannel(p, q, hue - 1.0 / 3.0);
        }
        const uint8_t alpha = (count == 4u) ? m_alphaChannel(values[3], percents[3]) : static_cast<uint8_t>(255);
        aoPaint = Paint::color(m_clampChannel(red * 255.0), m_clampChannel(green * 255.0), m_clampChannel(blue * 255.0), alpha);
        return true;
    }
    static const NamedColor* m_findNamed(const std::string& aName) {
        // clang-format off
        static const NamedColor sc_colors[] = {
            {"aliceblue", 240, 248, 255}, {"antiquewhite", 250, 235, 215}, {"aqua", 0, 255, 255}, {"aquamarine", 127, 255, 212},
            {"azure", 240, 255, 255}, {"beige", 245, 245, 220}, {"bisque", 255, 228, 196}, {"black", 0, 0, 0},
            {"blanchedalmond", 255, 235, 205}, {"blue", 0, 0, 255}, {"blueviolet", 138, 43, 226}, {"brown", 165, 42, 42},
            {"burlywood", 222, 184, 135}, {"cadetblue", 95, 158, 160}, {"chartreuse", 127, 255, 0}, {"chocolate", 210, 105, 30},
            {"coral", 255, 127, 80}, {"cornflowerblue", 100, 149, 237}, {"cornsilk", 255, 248, 220}, {"crimson", 220, 20, 60},
            {"cyan", 0, 255, 255}, {"darkblue", 0, 0, 139}, {"darkcyan", 0, 139, 139}, {"darkgoldenrod", 184, 134, 11},
            {"darkgray", 169, 169, 169}, {"darkgreen", 0, 100, 0}, {"darkgrey", 169, 169, 169}, {"darkkhaki", 189, 183, 107},
            {"darkmagenta", 139, 0, 139}, {"darkolivegreen", 85, 107, 47}, {"darkorange", 255, 140, 0}, {"darkorchid", 153, 50, 204},
            {"darkred", 139, 0, 0}, {"darksalmon", 233, 150, 122}, {"darkseagreen", 143, 188, 143}, {"darkslateblue", 72, 61, 139},
            {"darkslategray", 47, 79, 79}, {"darkslategrey", 47, 79, 79}, {"darkturquoise", 0, 206, 209}, {"darkviolet", 148, 0, 211},
            {"deeppink", 255, 20, 147}, {"deepskyblue", 0, 191, 255}, {"dimgray", 105, 105, 105}, {"dimgrey", 105, 105, 105},
            {"dodgerblue", 30, 144, 255}, {"firebrick", 178, 34, 34}, {"floralwhite", 255, 250, 240}, {"forestgreen", 34, 139, 34},
            {"fuchsia", 255, 0, 255}, {"gainsboro", 220, 220, 220}, {"ghostwhite", 248, 248, 255}, {"gold", 255, 215, 0},
            {"goldenrod", 218, 165, 32}, {"gray", 128, 128, 128}, {"grey", 128, 128, 128}, {"green", 0, 128, 0},
            {"greenyellow", 173, 255, 47}, {"honeydew", 240, 255, 240}, {"hotpink", 255, 105, 180}, {"indianred", 205, 92, 92},
            {"indigo", 75, 0, 130}, {"ivory", 255, 255, 240}, {"khaki", 240, 230, 140}, {"lavender", 230, 230, 250},
            {"lavenderblush", 255, 240, 245}, {"lawngreen", 124, 252, 0}, {"lemonchiffon", 255, 250, 205}, {"lightblue", 173, 216, 230},
            {"lightcoral", 240, 128, 128}, {"lightcyan", 224, 255, 255}, {"lightgoldenrodyellow", 250, 250, 210}, {"lightgray", 211, 211, 211},
            {"lightgreen", 144, 238, 144}, {"lightgrey", 211, 211, 211}, {"lightpink", 255, 182, 193}, {"lightsalmon", 255, 160, 122},
            {"lightseagreen", 32, 178, 170}, {"lightskyblue", 135, 206, 250}, {"lightslategray", 119, 136, 153}, {"lightslategrey", 119, 136, 153},
            {"lightsteelblue", 176, 196, 222}, {"lightyellow", 255, 255, 224}, {"lime", 0, 255, 0}, {"limegreen", 50, 205, 50},
            {"linen", 250, 240, 230}, {"magenta", 255, 0, 255}, {"maroon", 128, 0, 0}, {"mediumaquamarine", 102, 205, 170},
            {"mediumblue", 0, 0, 205}, {"mediumorchid", 186, 85, 211}, {"mediumpurple", 147, 112, 219}, {"mediumseagreen", 60, 179, 113},
            {"mediumslateblue", 123, 104, 238}, {"mediumspringgreen", 0, 250, 154}, {"mediumturquoise", 72, 209, 204}, {"mediumvioletred", 199, 21, 133},
            {"midnightblue", 25, 25, 112}, {"mintcream", 245, 255, 250}, {"mistyrose", 255, 228, 225}, {"moccasin", 255, 228, 181},
            {"navajowhite", 255, 222, 173}, {"navy", 0, 0, 128}, {"oldlace", 253, 245, 230}, {"olive", 128, 128, 0},
            {"olivedrab", 107, 142, 35}, {"orange", 255, 165, 0}, {"orangered", 255, 69, 0}, {"orchid", 218, 112, 214},
            {"palegoldenrod", 238, 232, 170}, {"palegreen", 152, 251, 152}, {"paleturquoise", 175, 238, 238}, {"palevioletred", 219, 112, 147},
            {"papayawhip", 255, 239, 213}, {"peachpuff", 255, 218, 185}, {"peru", 205, 133, 63}, {"pink", 255, 192, 203},
            {"plum", 221, 160, 221}, {"powderblue", 176, 224, 230}, {"purple", 128, 0, 128}, {"rebeccapurple", 102, 51, 153},
            {"red", 255, 0, 0}, {"rosybrown", 188, 143, 143}, {"royalblue", 65, 105, 225}, {"saddlebrown", 139, 69, 19},
            {"salmon", 250, 128, 114}, {"sandybrown", 244, 164, 96}, {"seagreen", 46, 139, 87}, {"seashell", 255, 245, 238},
            {"sienna", 160, 82, 45}, {"silver", 192, 192, 192}, {"skyblue", 135, 206, 235}, {"slateblue", 106, 90, 205},
            {"slategray", 112, 128, 144}, {"slategrey", 112, 128, 144}, {"snow", 255, 250, 250}, {"springgreen", 0, 255, 127},
            {"steelblue", 70, 130, 180}, {"tan", 210, 180, 140}, {"teal", 0, 128, 128}, {"thistle", 216, 191, 216},
            {"tomato", 255, 99, 71}, {"turquoise", 64, 224, 208}, {"violet", 238, 130, 238}, {"wheat", 245, 222, 179},
            {"white", 255, 255, 255}, {"whitesmoke", 245, 245, 245}, {"yellow", 255, 255, 0}, {"yellowgreen", 154, 205, 50}};
        // clang-format on
        const std::size_t count = sizeof(sc_colors) / sizeof(sc_colors[0]);
        for (std::size_t colorIdx = 0; colorIdx < count; ++colorIdx) {
            if (aName == sc_colors[colorIdx].name) {
                return &sc_colors[colorIdx];
            }
        }
        return nullptr;
    }
};

}  // namespace svg
}  // namespace ez
