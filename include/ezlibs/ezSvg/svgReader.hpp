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
// the READER : an svg text (parsed by ezXml) walked into a Document —
// the painted shapes in paint order, their points transformed into the
// document frame. the subset of a glyph or a plot : svg, g, path and
// the basic shapes ; fill, fill-rule, fill-opacity, opacity, display,
// visibility, transform, style ; the viewBox and the lengths. what it
// does not resolve (use, text, css style sheets, gradients, clips,
// masks, strokes) is IGNORED AND COUNTED in the warnings, never silent

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "../ezXml.hpp"
#include "../ezFile.hpp"
#include "svgTypes.hpp"
#include "svgScanner.hpp"
#include "svgPathParser.hpp"
#include "svgTransform.hpp"
#include "svgColor.hpp"
#include "svgStyle.hpp"

namespace ez {
namespace svg {

class Reader {
private:
    // what an element inherits from its ancestors
    struct State {
        Matrix ctm;
        Paint fill;
        FillRule fillRule{FillRule::NonZero};
        double fillOpacity{1.0};
        double opacity{1.0};
        bool hidden{false};
        bool strokeSet{false};
    };
    // the tallies of one parse, turned into warnings at the end
    struct Tally {
        std::map<std::string, int32_t> ignoredElements;
        int32_t strokeOnlyShapes{0};
        int32_t unsupportedPaints{0};
        int32_t unparsedLengths{0};
        std::vector<std::string> notes;
    };

public:
    static bool parse(const std::string& aText, Document& aoDocument, std::string& aoError) {
        aoDocument = Document();
        aoError.clear();
        ez::Xml xml;
        if (!xml.parseString(aText)) {
            aoError = "the xml does not parse";
            return false;
        }
        const ez::xml::Node* pRoot = nullptr;
        const auto& topNodes = xml.getRoot().getChildren();
        for (std::size_t nodeIdx = 0; nodeIdx < topNodes.size(); ++nodeIdx) {
            if (m_localName(topNodes[nodeIdx].getName()) == "svg") {
                pRoot = &topNodes[nodeIdx];
                break;
            }
        }
        if (pRoot == nullptr) {
            aoError = "no svg root element";
            return false;
        }
        Tally tally;
        m_readRoot(*pRoot, aoDocument, tally);
        State state;
        m_applyPresentation(*pRoot, state, tally);
        if (!state.hidden) {
            m_walkChildren(*pRoot, state, aoDocument, tally);
        }
        m_emitWarnings(tally, aoDocument);
        return true;
    }
    static bool parseFile(const std::string& aFilePath, Document& aoDocument, std::string& aoError) {
        const std::string text = ez::file::loadFileToString(aFilePath, false);
        if (text.empty()) {
            aoDocument = Document();
            aoError = "the file is empty or unreadable : " + aFilePath;
            return false;
        }
        return parse(text, aoDocument, aoError);
    }

private:
    static std::string m_localName(const std::string& aName) {
        const std::size_t colon = aName.find(':');
        return (colon == std::string::npos) ? aName : aName.substr(colon + 1);
    }
    static std::string m_attribute(const ez::xml::Node& aNode, const char* aName) {
        return aNode.isAttributeExist(aName) ? aNode.getAttribute<std::string>(aName) : std::string();
    }
    static std::string m_elementLabel(const ez::xml::Node& aNode) {
        const std::string id = m_attribute(aNode, "id");
        return id.empty() ? m_localName(aNode.getName()) : (m_localName(aNode.getName()) + "#" + id);
    }
    // a length : a number and a unit, in css pixels (96 per inch).
    // false on a relative unit (percent, em, ex) this reader cannot resolve
    static bool m_parseLength(const std::string& aText, double& aoPixels) {
        const std::string text = StyleParser::trim(aText);
        Scanner scanner(text);
        double value = 0.0;
        if (!scanner.readNumber(value)) {
            return false;
        }
        std::string unit;
        while (!scanner.atEnd()) {
            unit += scanner.peek();
            scanner.advance();
        }
        unit = StyleParser::toLower(StyleParser::trim(unit));
        double factor = 1.0;
        if (unit.empty() || (unit == "px")) {
            factor = 1.0;
        } else if (unit == "pt") {
            factor = 96.0 / 72.0;
        } else if (unit == "pc") {
            factor = 16.0;
        } else if (unit == "mm") {
            factor = 96.0 / 25.4;
        } else if (unit == "cm") {
            factor = 96.0 / 2.54;
        } else if (unit == "in") {
            factor = 96.0;
        } else {
            return false;
        }
        aoPixels = value * factor;
        return true;
    }
    static void m_readRoot(const ez::xml::Node& aRoot, Document& aoDocument, Tally& aoTally) {
        const std::string viewBox = m_attribute(aRoot, "viewBox");
        if (!viewBox.empty()) {
            Scanner scanner(viewBox);
            double values[4] = {0.0, 0.0, 0.0, 0.0};
            std::size_t count = 0;
            while (count < 4u) {
                scanner.skipSeparators();
                if (!scanner.readNumber(values[count])) {
                    break;
                }
                ++count;
            }
            if ((count == 4u) && (values[2] > 0.0) && (values[3] > 0.0)) {
                aoDocument.hasViewBox = true;
                aoDocument.viewBoxX = values[0];
                aoDocument.viewBoxY = values[1];
                aoDocument.viewBoxWidth = values[2];
                aoDocument.viewBoxHeight = values[3];
            } else {
                aoTally.notes.push_back("bad viewBox : " + viewBox);
            }
        }
        const std::string width = m_attribute(aRoot, "width");
        if (!width.empty() && !m_parseLength(width, aoDocument.width)) {
            ++aoTally.unparsedLengths;
        }
        const std::string height = m_attribute(aRoot, "height");
        if (!height.empty() && !m_parseLength(height, aoDocument.height)) {
            ++aoTally.unparsedLengths;
        }
    }
    // the presentation attributes and the style attribute of one element
    // folded into the inherited state (style wins over the attribute)
    static void m_applyPresentation(const ez::xml::Node& aNode, State& aoState, Tally& aoTally) {
        static const char* const sc_names[] = {"fill", "fill-rule", "fill-opacity", "opacity", "display", "visibility", "stroke"};
        std::map<std::string, std::string> properties;
        for (std::size_t nameIdx = 0; nameIdx < sizeof(sc_names) / sizeof(sc_names[0]); ++nameIdx) {
            if (aNode.isAttributeExist(sc_names[nameIdx])) {
                properties[sc_names[nameIdx]] = aNode.getAttribute<std::string>(sc_names[nameIdx]);
            }
        }
        const std::string style = m_attribute(aNode, "style");
        if (!style.empty()) {
            StyleParser::parse(style, properties);
        }
        std::map<std::string, std::string>::const_iterator it = properties.find("fill");
        if (it != properties.end()) {
            Paint paint;
            if (ColorParser::parse(it->second, paint)) {
                aoState.fill = paint;
                if (paint.kind == PaintKind::Unsupported) {
                    ++aoTally.unsupportedPaints;
                }
            } else if (StyleParser::toLower(StyleParser::trim(it->second)) != "inherit") {
                aoTally.notes.push_back("bad fill on " + m_elementLabel(aNode) + " : " + it->second);
            }
        }
        it = properties.find("fill-rule");
        if (it != properties.end()) {
            const std::string rule = StyleParser::toLower(StyleParser::trim(it->second));
            if (rule == "evenodd") {
                aoState.fillRule = FillRule::EvenOdd;
            } else if (rule == "nonzero") {
                aoState.fillRule = FillRule::NonZero;
            }
        }
        it = properties.find("fill-opacity");
        if (it != properties.end()) {
            aoState.fillOpacity = m_parseOpacity(it->second, aoState.fillOpacity);
        }
        it = properties.find("opacity");
        if (it != properties.end()) {
            aoState.opacity *= m_parseOpacity(it->second, 1.0);
        }
        it = properties.find("display");
        if ((it != properties.end()) && (StyleParser::toLower(StyleParser::trim(it->second)) == "none")) {
            aoState.hidden = true;
        }
        it = properties.find("visibility");
        if (it != properties.end()) {
            const std::string visibility = StyleParser::toLower(StyleParser::trim(it->second));
            if ((visibility == "hidden") || (visibility == "collapse")) {
                aoState.hidden = true;
            } else if (visibility == "visible") {
                aoState.hidden = false;
            }
        }
        it = properties.find("stroke");
        if (it != properties.end()) {
            aoState.strokeSet = (StyleParser::toLower(StyleParser::trim(it->second)) != "none");
        }
        const std::string transform = m_attribute(aNode, "transform");
        if (!transform.empty()) {
            Matrix local;
            std::string error;
            if (TransformParser::parse(transform, local, error)) {
                aoState.ctm = aoState.ctm.multiply(local);
            } else {
                aoTally.notes.push_back("bad transform on " + m_elementLabel(aNode) + " : " + error);
            }
        }
    }
    static double m_parseOpacity(const std::string& aText, double aFallback) {
        const std::string text = StyleParser::trim(aText);
        Scanner scanner(text);
        double value = 0.0;
        if (!scanner.readNumber(value)) {
            return aFallback;
        }
        if (scanner.peek() == '%') {
            value /= 100.0;
        }
        return (value < 0.0) ? 0.0 : ((value > 1.0) ? 1.0 : value);
    }
    static void m_walkChildren(const ez::xml::Node& aNode, const State& aState, Document& aoDocument, Tally& aoTally) {
        const auto& children = aNode.getChildren();
        for (std::size_t childIdx = 0; childIdx < children.size(); ++childIdx) {
            m_walkElement(children[childIdx], aState, aoDocument, aoTally);
        }
    }
    static void m_walkElement(const ez::xml::Node& aNode, const State& aParent, Document& aoDocument, Tally& aoTally) {
        const std::string name = m_localName(aNode.getName());
        if (name.empty()) {
            return;  // a comment, a prologue : not an element
        }
        if ((name == "title") || (name == "desc") || (name == "metadata")) {
            return;  // the prose of the file, nothing to draw
        }
        State state = aParent;
        m_applyPresentation(aNode, state, aoTally);
        if (state.hidden) {
            return;
        }
        if ((name == "g") || (name == "a") || (name == "switch")) {
            m_walkChildren(aNode, state, aoDocument, aoTally);
        } else if (name == "svg") {
            aoTally.notes.push_back("nested svg walked as a group : " + m_elementLabel(aNode));
            m_walkChildren(aNode, state, aoDocument, aoTally);
        } else if (name == "path") {
            std::vector<SubPath> subPaths;
            std::string error;
            if (!PathParser::parse(m_attribute(aNode, "d"), subPaths, error)) {
                aoTally.notes.push_back("bad path data on " + m_elementLabel(aNode) + " : " + error);
            }
            m_emitShape(aNode, subPaths, state, aoDocument, aoTally);
        } else if (name == "rect") {
            std::vector<SubPath> subPaths;
            m_rect(aNode, subPaths);
            m_emitShape(aNode, subPaths, state, aoDocument, aoTally);
        } else if ((name == "circle") || (name == "ellipse")) {
            std::vector<SubPath> subPaths;
            m_ellipse(aNode, name == "circle", subPaths);
            m_emitShape(aNode, subPaths, state, aoDocument, aoTally);
        } else if ((name == "polyline") || (name == "polygon")) {
            std::vector<SubPath> subPaths;
            m_points(aNode, name == "polygon", subPaths);
            m_emitShape(aNode, subPaths, state, aoDocument, aoTally);
        } else if (name == "line") {
            if (state.strokeSet) {
                ++aoTally.strokeOnlyShapes;  // a line has no inside : stroke only by nature
            }
        } else {
            ++aoTally.ignoredElements[name];
        }
    }
    static void m_emitShape(const ez::xml::Node& aNode, const std::vector<SubPath>& aSubPaths, const State& aState, Document& aoDocument, Tally& aoTally) {
        if (aSubPaths.empty()) {
            return;
        }
        if (!aState.fill.isVisible()) {
            if (aState.strokeSet) {
                ++aoTally.strokeOnlyShapes;
            }
            return;
        }
        Shape shape;
        shape.id = m_attribute(aNode, "id");
        shape.fill = aState.fill;
        shape.fillRule = aState.fillRule;
        shape.opacity = aState.fillOpacity * aState.opacity;
        shape.subPaths = aSubPaths;
        if (!aState.ctm.isIdentity()) {
            for (std::size_t pathIdx = 0; pathIdx < shape.subPaths.size(); ++pathIdx) {
                SubPath& subPath = shape.subPaths[pathIdx];
                subPath.start = aState.ctm.apply(subPath.start);
                for (std::size_t segmentIdx = 0; segmentIdx < subPath.segments.size(); ++segmentIdx) {
                    Segment& segment = subPath.segments[segmentIdx];
                    segment.end = aState.ctm.apply(segment.end);
                    if (segment.kind == SegmentKind::Cubic) {
                        segment.control0 = aState.ctm.apply(segment.control0);
                        segment.control1 = aState.ctm.apply(segment.control1);
                    }
                }
            }
        }
        aoDocument.shapes.push_back(shape);
    }
    static double m_number(const ez::xml::Node& aNode, const char* aName, double aFallback) {
        const std::string text = m_attribute(aNode, aName);
        if (text.empty()) {
            return aFallback;
        }
        double pixels = 0.0;
        return m_parseLength(text, pixels) ? pixels : aFallback;
    }
    // the corner of a rounded rectangle and the quarter of an ellipse :
    // one cubic each, the circle constant
    static double m_kappa() {
        return 0.5522847498307936;
    }
    static void m_rect(const ez::xml::Node& aNode, std::vector<SubPath>& aoSubPaths) {
        const double x = m_number(aNode, "x", 0.0);
        const double y = m_number(aNode, "y", 0.0);
        const double width = m_number(aNode, "width", 0.0);
        const double height = m_number(aNode, "height", 0.0);
        if ((width <= 0.0) || (height <= 0.0)) {
            return;
        }
        double radiusX = m_number(aNode, "rx", -1.0);
        double radiusY = m_number(aNode, "ry", -1.0);
        if ((radiusX < 0.0) && (radiusY < 0.0)) {
            radiusX = 0.0;
            radiusY = 0.0;
        } else if (radiusX < 0.0) {
            radiusX = radiusY;
        } else if (radiusY < 0.0) {
            radiusY = radiusX;
        }
        radiusX = (radiusX > width * 0.5) ? (width * 0.5) : radiusX;
        radiusY = (radiusY > height * 0.5) ? (height * 0.5) : radiusY;
        SubPath subPath;
        subPath.closed = true;
        if ((radiusX <= 0.0) || (radiusY <= 0.0)) {
            subPath.start = Point(x, y);
            subPath.segments.push_back(Segment::line(Point(x + width, y)));
            subPath.segments.push_back(Segment::line(Point(x + width, y + height)));
            subPath.segments.push_back(Segment::line(Point(x, y + height)));
            subPath.segments.push_back(Segment::line(Point(x, y)));
        } else {
            const double kx = radiusX * m_kappa();
            const double ky = radiusY * m_kappa();
            const double right = x + width;
            const double bottom = y + height;
            subPath.start = Point(x + radiusX, y);
            subPath.segments.push_back(Segment::line(Point(right - radiusX, y)));
            subPath.segments.push_back(Segment::cubic(Point(right - radiusX + kx, y), Point(right, y + radiusY - ky), Point(right, y + radiusY)));
            subPath.segments.push_back(Segment::line(Point(right, bottom - radiusY)));
            subPath.segments.push_back(Segment::cubic(Point(right, bottom - radiusY + ky), Point(right - radiusX + kx, bottom), Point(right - radiusX, bottom)));
            subPath.segments.push_back(Segment::line(Point(x + radiusX, bottom)));
            subPath.segments.push_back(Segment::cubic(Point(x + radiusX - kx, bottom), Point(x, bottom - radiusY + ky), Point(x, bottom - radiusY)));
            subPath.segments.push_back(Segment::line(Point(x, y + radiusY)));
            subPath.segments.push_back(Segment::cubic(Point(x, y + radiusY - ky), Point(x + radiusX - kx, y), Point(x + radiusX, y)));
        }
        aoSubPaths.push_back(subPath);
    }
    static void m_ellipse(const ez::xml::Node& aNode, bool aCircle, std::vector<SubPath>& aoSubPaths) {
        const double centerX = m_number(aNode, "cx", 0.0);
        const double centerY = m_number(aNode, "cy", 0.0);
        const double radiusX = aCircle ? m_number(aNode, "r", 0.0) : m_number(aNode, "rx", 0.0);
        const double radiusY = aCircle ? radiusX : m_number(aNode, "ry", 0.0);
        if ((radiusX <= 0.0) || (radiusY <= 0.0)) {
            return;
        }
        const double kx = radiusX * m_kappa();
        const double ky = radiusY * m_kappa();
        SubPath subPath;
        subPath.closed = true;
        subPath.start = Point(centerX + radiusX, centerY);
        subPath.segments.push_back(Segment::cubic(Point(centerX + radiusX, centerY + ky), Point(centerX + kx, centerY + radiusY), Point(centerX, centerY + radiusY)));
        subPath.segments.push_back(Segment::cubic(Point(centerX - kx, centerY + radiusY), Point(centerX - radiusX, centerY + ky), Point(centerX - radiusX, centerY)));
        subPath.segments.push_back(Segment::cubic(Point(centerX - radiusX, centerY - ky), Point(centerX - kx, centerY - radiusY), Point(centerX, centerY - radiusY)));
        subPath.segments.push_back(Segment::cubic(Point(centerX + kx, centerY - radiusY), Point(centerX + radiusX, centerY - ky), Point(centerX + radiusX, centerY)));
        aoSubPaths.push_back(subPath);
    }
    static void m_points(const ez::xml::Node& aNode, bool aClosed, std::vector<SubPath>& aoSubPaths) {
        const std::string text = m_attribute(aNode, "points");
        Scanner scanner(text);
        std::vector<Point> points;
        while (true) {
            double x = 0.0;
            double y = 0.0;
            scanner.skipSeparators();
            if (!scanner.readNumber(x)) {
                break;
            }
            scanner.skipSeparators();
            if (!scanner.readNumber(y)) {
                break;
            }
            points.push_back(Point(x, y));
        }
        if (points.size() < 2u) {
            return;
        }
        SubPath subPath;
        subPath.closed = aClosed;
        subPath.start = points[0];
        for (std::size_t pointIdx = 1; pointIdx < points.size(); ++pointIdx) {
            subPath.segments.push_back(Segment::line(points[pointIdx]));
        }
        aoSubPaths.push_back(subPath);
    }
    static void m_emitWarnings(const Tally& aTally, Document& aoDocument) {
        if (!aTally.ignoredElements.empty()) {
            int32_t total = 0;
            std::string names;
            for (std::map<std::string, int32_t>::const_iterator it = aTally.ignoredElements.begin(); it != aTally.ignoredElements.end(); ++it) {
                total += it->second;
                names += (names.empty() ? "" : ", ") + it->first;
            }
            aoDocument.warnings.push_back(std::to_string(total) + " elements ignored : " + names);
        }
        if (aTally.strokeOnlyShapes > 0) {
            aoDocument.warnings.push_back(std::to_string(aTally.strokeOnlyShapes) + " shapes ignored : stroke only (no fill)");
        }
        if (aTally.unsupportedPaints > 0) {
            aoDocument.warnings.push_back(std::to_string(aTally.unsupportedPaints) + " fills unsupported (gradient, pattern) : the shapes are kept, their paint is unknown");
        }
        if (aTally.unparsedLengths > 0) {
            aoDocument.warnings.push_back(std::to_string(aTally.unparsedLengths) + " lengths in a relative unit ignored (percent, em, ex)");
        }
        for (std::size_t noteIdx = 0; noteIdx < aTally.notes.size(); ++noteIdx) {
            aoDocument.warnings.push_back(aTally.notes[noteIdx]);
        }
    }
};

}  // namespace svg
}  // namespace ez
