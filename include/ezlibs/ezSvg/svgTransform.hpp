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
// the TRANSFORM LIST grammar (svg 1.1 7.6) : matrix, translate, scale,
// rotate (around a center or not), skewX, skewY — a list composes in
// order, the rightmost applies first to a point

#include <cstddef>
#include <string>
#include <vector>

#include "svgTypes.hpp"
#include "svgScanner.hpp"

namespace ez {
namespace svg {

class TransformParser {
public:
    // false with a readable error ; aoMatrix is the identity then
    static bool parse(const std::string& aText, Matrix& aoMatrix, std::string& aoError) {
        aoError.clear();
        aoMatrix = Matrix();
        Scanner scanner(aText);
        while (true) {
            scanner.skipSeparators();
            if (scanner.atEnd()) {
                return true;
            }
            std::string name;
            if (!scanner.readIdentifier(name)) {
                aoError = "a transform name is expected at " + std::to_string(scanner.getPos());
                aoMatrix = Matrix();
                return false;
            }
            if (!scanner.expect('(')) {
                aoError = "an opening parenthesis is expected after " + name;
                aoMatrix = Matrix();
                return false;
            }
            std::vector<double> values;
            while (true) {
                scanner.skipSeparators();
                if (scanner.peek() == ')') {
                    scanner.advance();
                    break;
                }
                double value = 0.0;
                if (!scanner.readNumber(value)) {
                    aoError = "a number or a closing parenthesis is expected in " + name;
                    aoMatrix = Matrix();
                    return false;
                }
                values.push_back(value);
            }
            Matrix local;
            if (!m_build(name, values, local)) {
                aoError = "bad transform : " + name + " with " + std::to_string(values.size()) + " values";
                aoMatrix = Matrix();
                return false;
            }
            aoMatrix = aoMatrix.multiply(local);
        }
    }

private:
    static bool m_build(const std::string& aName, const std::vector<double>& aValues, Matrix& aoMatrix) {
        if (aName == "matrix") {
            if (aValues.size() != 6u) {
                return false;
            }
            aoMatrix = Matrix(aValues[0], aValues[1], aValues[2], aValues[3], aValues[4], aValues[5]);
            return true;
        }
        if (aName == "translate") {
            if ((aValues.size() != 1u) && (aValues.size() != 2u)) {
                return false;
            }
            aoMatrix = Matrix::translation(aValues[0], (aValues.size() == 2u) ? aValues[1] : 0.0);
            return true;
        }
        if (aName == "scale") {
            if ((aValues.size() != 1u) && (aValues.size() != 2u)) {
                return false;
            }
            aoMatrix = Matrix::scaling(aValues[0], (aValues.size() == 2u) ? aValues[1] : aValues[0]);
            return true;
        }
        if (aName == "rotate") {
            if (aValues.size() == 1u) {
                aoMatrix = Matrix::rotation(aValues[0]);
                return true;
            }
            if (aValues.size() == 3u) {
                aoMatrix = Matrix::rotationAround(aValues[0], aValues[1], aValues[2]);
                return true;
            }
            return false;
        }
        if (aName == "skewX") {
            if (aValues.size() != 1u) {
                return false;
            }
            aoMatrix = Matrix::skewingX(aValues[0]);
            return true;
        }
        if (aName == "skewY") {
            if (aValues.size() != 1u) {
                return false;
            }
            aoMatrix = Matrix::skewingY(aValues[0]);
            return true;
        }
        return false;
    }
};

}  // namespace svg
}  // namespace ez
