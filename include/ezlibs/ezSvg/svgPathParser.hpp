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
// the PATH DATA grammar (the d attribute, svg 1.1 8.3) : M m L l H h V v
// C c S s Q q T t A a Z z, the implicit repeats, the reflected controls
// of S and T. the output is ABSOLUTE and reduced to lines and cubics :
// a quadratic is elevated, an arc becomes cubics (ezBezier), a zero
// radius arc a line — what a pen or a mesher wants, nothing to unpack

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "../ezMath/ezBezier.hpp"
#include "svgTypes.hpp"
#include "svgScanner.hpp"

namespace ez {
namespace svg {

class PathParser {
private:
    // the running state of one parse
    struct State {
        std::vector<SubPath>* pOut{nullptr};
        SubPath current;
        bool open{false};
        bool everOpened{false};
        Point point;
        Point start;
        Point lastCubicControl;
        Point lastQuadControl;
        char lastCommand{'\0'};
    };

public:
    // false with a readable error (the character position), aoSubPaths
    // then holds what parsed before the fault
    static bool parse(const std::string& aData, std::vector<SubPath>& aoSubPaths, std::string& aoError) {
        aoError.clear();
        Scanner scanner(aData);
        State state;
        state.pOut = &aoSubPaths;
        char command = '\0';
        bool relative = false;
        while (true) {
            scanner.skipSeparators();
            if (scanner.atEnd()) {
                break;
            }
            const char ch = scanner.peek();
            if (Scanner::isAlpha(ch)) {
                command = static_cast<char>(((ch >= 'a') && (ch <= 'z')) ? (ch - 'a' + 'A') : ch);
                relative = ((ch >= 'a') && (ch <= 'z'));
                scanner.advance();
                if (std::string("MLHVCSQTAZ").find(command) == std::string::npos) {
                    return m_fail(aoError, "unknown path command", scanner.getPos());
                }
            } else if (command == '\0') {
                return m_fail(aoError, "a number before any command", scanner.getPos());
            } else if (command == 'Z') {
                return m_fail(aoError, "a number after a close, a command is expected", scanner.getPos());
            } else if (command == 'M') {
                command = 'L';  // the implicit lineto after a moveto keeps its relativity
            }
            if ((command != 'M') && !state.everOpened) {
                return m_fail(aoError, "a path must begin with a moveto", scanner.getPos());
            }
            if (command == 'Z') {
                m_close(state);
                state.lastCommand = 'Z';
                continue;
            }
            if (!m_command(scanner, command, relative, state, aoError)) {
                return false;
            }
            state.lastCommand = command;
        }
        m_flush(state);
        return true;
    }

private:
    static bool m_fail(std::string& aoError, const char* aWhat, std::size_t aPos) {
        aoError = std::string(aWhat) + " at " + std::to_string(aPos);
        return false;
    }
    static bool m_readPoint(Scanner& aScanner, bool aRelative, const Point& aReference, Point& aoPoint) {
        double x = 0.0;
        double y = 0.0;
        aScanner.skipSeparators();
        if (!aScanner.readNumber(x)) {
            return false;
        }
        aScanner.skipSeparators();
        if (!aScanner.readNumber(y)) {
            return false;
        }
        aoPoint = aRelative ? Point(aReference.x + x, aReference.y + y) : Point(x, y);
        return true;
    }
    static bool m_readValue(Scanner& aScanner, double& aoValue) {
        aScanner.skipSeparators();
        return aScanner.readNumber(aoValue);
    }
    static void m_flush(State& aoState) {
        if (aoState.open && !aoState.current.segments.empty()) {
            aoState.pOut->push_back(aoState.current);
        }
        aoState.current = SubPath();
        aoState.open = false;
    }
    // a segment on a closed path re-opens a sub path at the current point
    static void m_ensureOpen(State& aoState) {
        if (!aoState.open) {
            aoState.current = SubPath();
            aoState.current.start = aoState.point;
            aoState.start = aoState.point;
            aoState.open = true;
            aoState.everOpened = true;
        }
    }
    static void m_close(State& aoState) {
        if (aoState.open) {
            aoState.current.closed = true;
            m_flush(aoState);
        }
        aoState.point = aoState.start;
    }
    static void m_line(State& aoState, const Point& aTarget) {
        m_ensureOpen(aoState);
        aoState.current.segments.push_back(Segment::line(aTarget));
        aoState.point = aTarget;
    }
    static void m_cubic(State& aoState, const Point& aControl0, const Point& aControl1, const Point& aTarget) {
        m_ensureOpen(aoState);
        aoState.current.segments.push_back(Segment::cubic(aControl0, aControl1, aTarget));
        aoState.point = aTarget;
    }
    static Point m_reflect(const Point& aPivot, const Point& aPoint) {
        return Point(2.0 * aPivot.x - aPoint.x, 2.0 * aPivot.y - aPoint.y);
    }
    static bool m_command(Scanner& aScanner, char aCommand, bool aRelative, State& aoState, std::string& aoError) {
        const std::size_t pos = aScanner.getPos();
        switch (aCommand) {
            case 'M': {
                Point target;
                if (!m_readPoint(aScanner, aRelative, aoState.point, target)) {
                    return m_fail(aoError, "moveto needs two numbers", pos);
                }
                m_flush(aoState);
                aoState.point = target;
                aoState.start = target;
                aoState.current = SubPath();
                aoState.current.start = target;
                aoState.open = true;
                aoState.everOpened = true;
                return true;
            }
            case 'L': {
                Point target;
                if (!m_readPoint(aScanner, aRelative, aoState.point, target)) {
                    return m_fail(aoError, "lineto needs two numbers", pos);
                }
                m_line(aoState, target);
                return true;
            }
            case 'H': {
                double x = 0.0;
                if (!m_readValue(aScanner, x)) {
                    return m_fail(aoError, "horizontal lineto needs a number", pos);
                }
                m_line(aoState, Point(aRelative ? (aoState.point.x + x) : x, aoState.point.y));
                return true;
            }
            case 'V': {
                double y = 0.0;
                if (!m_readValue(aScanner, y)) {
                    return m_fail(aoError, "vertical lineto needs a number", pos);
                }
                m_line(aoState, Point(aoState.point.x, aRelative ? (aoState.point.y + y) : y));
                return true;
            }
            case 'C': {
                Point control0;
                Point control1;
                Point target;
                if (!m_readPoint(aScanner, aRelative, aoState.point, control0) || !m_readPoint(aScanner, aRelative, aoState.point, control1) ||
                    !m_readPoint(aScanner, aRelative, aoState.point, target)) {
                    return m_fail(aoError, "curveto needs six numbers", pos);
                }
                m_cubic(aoState, control0, control1, target);
                aoState.lastCubicControl = control1;
                return true;
            }
            case 'S': {
                Point control1;
                Point target;
                if (!m_readPoint(aScanner, aRelative, aoState.point, control1) || !m_readPoint(aScanner, aRelative, aoState.point, target)) {
                    return m_fail(aoError, "smooth curveto needs four numbers", pos);
                }
                const bool reflects = (aoState.lastCommand == 'C') || (aoState.lastCommand == 'S');
                const Point control0 = reflects ? m_reflect(aoState.point, aoState.lastCubicControl) : aoState.point;
                m_cubic(aoState, control0, control1, target);
                aoState.lastCubicControl = control1;
                return true;
            }
            case 'Q': {
                Point control;
                Point target;
                if (!m_readPoint(aScanner, aRelative, aoState.point, control) || !m_readPoint(aScanner, aRelative, aoState.point, target)) {
                    return m_fail(aoError, "quadratic curveto needs four numbers", pos);
                }
                m_quadratic(aoState, control, target);
                return true;
            }
            case 'T': {
                Point target;
                if (!m_readPoint(aScanner, aRelative, aoState.point, target)) {
                    return m_fail(aoError, "smooth quadratic curveto needs two numbers", pos);
                }
                const bool reflects = (aoState.lastCommand == 'Q') || (aoState.lastCommand == 'T');
                const Point control = reflects ? m_reflect(aoState.point, aoState.lastQuadControl) : aoState.point;
                m_quadratic(aoState, control, target);
                return true;
            }
            case 'A': {
                double radiusX = 0.0;
                double radiusY = 0.0;
                double rotation = 0.0;
                bool largeArc = false;
                bool sweep = false;
                Point target;
                if (!m_readValue(aScanner, radiusX) || !m_readValue(aScanner, radiusY) || !m_readValue(aScanner, rotation)) {
                    return m_fail(aoError, "arc needs its radii and rotation", pos);
                }
                aScanner.skipSeparators();
                if (!aScanner.readFlag(largeArc)) {
                    return m_fail(aoError, "arc needs its large arc flag", pos);
                }
                aScanner.skipSeparators();
                if (!aScanner.readFlag(sweep)) {
                    return m_fail(aoError, "arc needs its sweep flag", pos);
                }
                if (!m_readPoint(aScanner, aRelative, aoState.point, target)) {
                    return m_fail(aoError, "arc needs its end point", pos);
                }
                std::vector<ez::math::bezier::cubic2> cubics;
                if (ez::math::bezier::arcToCubics(aoState.point, radiusX, radiusY, rotation, largeArc, sweep, target, cubics)) {
                    for (std::size_t cubicIdx = 0; cubicIdx < cubics.size(); ++cubicIdx) {
                        m_cubic(aoState, cubics[cubicIdx].control0, cubics[cubicIdx].control1, cubics[cubicIdx].end);
                    }
                } else if ((target.x != aoState.point.x) || (target.y != aoState.point.y)) {
                    m_line(aoState, target);  // the spec : a degenerate arc is a line
                }
                return true;
            }
            default: {
                return m_fail(aoError, "unknown path command", pos);
            }
        }
    }
    static void m_quadratic(State& aoState, const Point& aControl, const Point& aTarget) {
        Point control0;
        Point control1;
        ez::math::bezier::quadraticToCubic(aoState.point, aControl, aTarget, control0, control1);
        m_cubic(aoState, control0, control1, aTarget);
        aoState.lastQuadControl = aControl;
    }
};

}  // namespace svg
}  // namespace ez
