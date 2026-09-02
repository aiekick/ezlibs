#include <TestEzSvgReader.h>

#include <cmath>
#include <string>

#include <ezlibs/ezSvg/ezSvg.hpp>
#include <ezlibs/ezCTest.hpp>

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

namespace {

bool local_near(const ez::svg::Point& aPoint, double aX, double aY, double aTolerance = 1e-6) {
    return (std::fabs(aPoint.x - aX) < aTolerance) && (std::fabs(aPoint.y - aY) < aTolerance);
}

bool local_read(const char* aText, ez::svg::Document& aoDocument) {
    std::string error;
    return ez::svg::Reader::parse(aText, aoDocument, error);
}

bool local_isColor(const ez::svg::Paint& aPaint, int32_t aRed, int32_t aGreen, int32_t aBlue) {
    return (aPaint.kind == ez::svg::PaintKind::Color) && (aPaint.red == aRed) && (aPaint.green == aGreen) && (aPaint.blue == aBlue);
}

bool local_warningMentions(const ez::svg::Document& aDocument, const char* aText) {
    for (const auto& warning : aDocument.warnings) {
        if (warning.find(aText) != std::string::npos) {
            return true;
        }
    }
    return false;
}

}  // namespace

// the simplest icon : one path, the initial fill (black), the viewBox
bool TestEzSvgReader_ASquarePathIsOneShape() {
    ez::svg::Document document;
    CTEST_ASSERT(local_read("<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\"><path id=\"box\" d=\"M0 0h24v24H0z\"/></svg>", document));
    CTEST_ASSERT(document.hasViewBox);
    CTEST_ASSERT(document.viewBoxWidth == 24.0);
    CTEST_ASSERT(document.viewBoxHeight == 24.0);
    CTEST_ASSERT(document.shapes.size() == 1u);
    const ez::svg::Shape& shape = document.shapes[0];
    CTEST_ASSERT(shape.id == "box");
    CTEST_ASSERT(local_isColor(shape.fill, 0, 0, 0));
    CTEST_ASSERT(shape.fillRule == ez::svg::FillRule::NonZero);
    CTEST_ASSERT(shape.opacity == 1.0);
    CTEST_ASSERT(shape.subPaths.size() == 1u);
    CTEST_ASSERT(shape.subPaths[0].segments.size() == 3u);
    CTEST_ASSERT(shape.subPaths[0].closed);
    CTEST_ASSERT(document.warnings.empty());
    return true;
}

bool TestEzSvgReader_ACircleBecomesCubics() {
    ez::svg::Document document;
    CTEST_ASSERT(local_read("<svg><circle cx=\"12\" cy=\"12\" r=\"10\" fill=\"#0f0\"/></svg>", document));
    CTEST_ASSERT(document.shapes.size() == 1u);
    const ez::svg::Shape& shape = document.shapes[0];
    CTEST_ASSERT(local_isColor(shape.fill, 0, 255, 0));
    CTEST_ASSERT(shape.subPaths.size() == 1u);
    CTEST_ASSERT(shape.subPaths[0].segments.size() == 4u);
    CTEST_ASSERT(shape.subPaths[0].closed);
    CTEST_ASSERT(local_near(shape.subPaths[0].start, 22.0, 12.0));
    for (const auto& segment : shape.subPaths[0].segments) {
        CTEST_ASSERT(segment.kind == ez::svg::SegmentKind::Cubic);
    }
    CTEST_ASSERT(local_near(shape.subPaths[0].segments[1].end, 2.0, 12.0));
    return true;
}

// nested groups compose their transforms, the innermost applying first
bool TestEzSvgReader_GroupTransformsCompose() {
    ez::svg::Document document;
    CTEST_ASSERT(local_read("<svg><g transform=\"translate(10 0)\"><g transform=\"scale(2)\"><rect x=\"1\" y=\"1\" width=\"2\" height=\"2\"/></g></g></svg>", document));
    CTEST_ASSERT(document.shapes.size() == 1u);
    const ez::svg::SubPath& rect = document.shapes[0].subPaths[0];
    CTEST_ASSERT(local_near(rect.start, 12.0, 2.0));
    CTEST_ASSERT(local_near(rect.segments[0].end, 16.0, 2.0));
    CTEST_ASSERT(local_near(rect.segments[1].end, 16.0, 6.0));
    return true;
}

// fill and fill-opacity inherit from the group, a child may override
bool TestEzSvgReader_GroupFillIsInherited() {
    ez::svg::Document document;
    CTEST_ASSERT(local_read("<svg><g fill=\"red\" fill-opacity=\"0.5\"><path d=\"M0 0h1v1z\"/><path fill=\"blue\" d=\"M2 2h1v1z\"/></g></svg>", document));
    CTEST_ASSERT(document.shapes.size() == 2u);
    CTEST_ASSERT(local_isColor(document.shapes[0].fill, 255, 0, 0));
    CTEST_ASSERT(local_isColor(document.shapes[1].fill, 0, 0, 255));
    CTEST_ASSERT(std::fabs(document.shapes[0].opacity - 0.5) < 1e-9);
    CTEST_ASSERT(std::fabs(document.shapes[1].opacity - 0.5) < 1e-9);
    return true;
}

bool TestEzSvgReader_StyleWinsOverTheAttribute() {
    ez::svg::Document document;
    CTEST_ASSERT(local_read("<svg><path fill=\"red\" style=\"fill:#00f; fill-rule:evenodd\" d=\"M0 0h1v1z\"/></svg>", document));
    CTEST_ASSERT(document.shapes.size() == 1u);
    CTEST_ASSERT(local_isColor(document.shapes[0].fill, 0, 0, 255));
    CTEST_ASSERT(document.shapes[0].fillRule == ez::svg::FillRule::EvenOdd);
    return true;
}

// no stroking here : a stroke-only shape is not kept, but it is counted
bool TestEzSvgReader_StrokeOnlyShapesAreCountedNotKept() {
    ez::svg::Document document;
    CTEST_ASSERT(local_read("<svg><path fill=\"none\" stroke=\"black\" d=\"M0 0h10\"/><line x1=\"0\" y1=\"0\" x2=\"5\" y2=\"5\" stroke=\"red\"/><path fill=\"none\" d=\"M0 0h1v1z\"/></svg>", document));
    CTEST_ASSERT(document.shapes.empty());
    CTEST_ASSERT(local_warningMentions(document, "2 shapes ignored : stroke only"));
    return true;
}

bool TestEzSvgReader_HiddenElementsAreSkipped() {
    ez::svg::Document document;
    CTEST_ASSERT(local_read("<svg><g display=\"none\"><path d=\"M0 0h1v1z\"/></g><path visibility=\"hidden\" d=\"M0 0h1v1z\"/><path style=\"display:none\" d=\"M0 0h1v1z\"/><path d=\"M5 5h1v1z\"/></svg>", document));
    CTEST_ASSERT(document.shapes.size() == 1u);
    CTEST_ASSERT(local_near(document.shapes[0].subPaths[0].start, 5.0, 5.0));
    return true;
}

// what the reader does not resolve is named, never silent
bool TestEzSvgReader_UnsupportedElementsAreCounted() {
    ez::svg::Document document;
    CTEST_ASSERT(local_read("<svg><defs><linearGradient id=\"g\"/></defs><use href=\"#x\"/><text>hi</text><title>icon</title><path d=\"M0 0h1v1z\"/></svg>", document));
    CTEST_ASSERT(document.shapes.size() == 1u);
    CTEST_ASSERT(local_warningMentions(document, "3 elements ignored"));
    CTEST_ASSERT(local_warningMentions(document, "use"));
    CTEST_ASSERT(local_warningMentions(document, "text"));
    CTEST_ASSERT(local_warningMentions(document, "defs"));
    return true;
}

// a real file : the xml prologue, a doctype and comments before and
// inside the root
bool TestEzSvgReader_PrologueDoctypeAndCommentsPass() {
    const std::string text =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n"
        "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n"
        "<!-- Generator: by hand -->\n"
        "<svg xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" viewBox=\"0 0 24 24\">\n"
        "  <!-- the box -->\n"
        "  <path d=\"M0 0h24v24H0z\"/>\n"
        "</svg>\n";
    ez::svg::Document document;
    std::string error;
    CTEST_ASSERT(ez::svg::Reader::parse(text, document, error));
    CTEST_ASSERT(document.hasViewBox);
    CTEST_ASSERT(document.shapes.size() == 1u);
    return true;
}

bool TestEzSvgReader_LengthsWithUnits() {
    ez::svg::Document document;
    CTEST_ASSERT(local_read("<svg width=\"10mm\" height=\"1in\"><path d=\"M0 0h1v1z\"/></svg>", document));
    CTEST_ASSERT(std::fabs(document.width - 10.0 * 96.0 / 25.4) < 1e-6);
    CTEST_ASSERT(std::fabs(document.height - 96.0) < 1e-6);
    CTEST_ASSERT(!document.hasViewBox);
    ez::svg::Document relative;
    CTEST_ASSERT(local_read("<svg width=\"100%\" height=\"24px\"><path d=\"M0 0h1v1z\"/></svg>", relative));
    CTEST_ASSERT(relative.width == 0.0);
    CTEST_ASSERT(relative.height == 24.0);
    CTEST_ASSERT(local_warningMentions(relative, "relative unit"));
    return true;
}

bool TestEzSvgReader_RoundedRectHasCorners() {
    ez::svg::Document document;
    CTEST_ASSERT(local_read("<svg><rect width=\"10\" height=\"10\" rx=\"2\"/></svg>", document));
    CTEST_ASSERT(document.shapes.size() == 1u);
    const ez::svg::SubPath& rect = document.shapes[0].subPaths[0];
    CTEST_ASSERT(rect.segments.size() == 8u);
    CTEST_ASSERT(local_near(rect.start, 2.0, 0.0));
    CTEST_ASSERT(rect.segments[0].kind == ez::svg::SegmentKind::Line);
    CTEST_ASSERT(rect.segments[1].kind == ez::svg::SegmentKind::Cubic);
    CTEST_ASSERT(local_near(rect.segments[1].end, 10.0, 2.0));
    CTEST_ASSERT(local_near(rect.segments[7].end, 2.0, 0.0));
    CTEST_ASSERT(rect.closed);
    return true;
}

bool TestEzSvgReader_PolygonAndPolyline() {
    ez::svg::Document document;
    CTEST_ASSERT(local_read("<svg><polygon points=\"0,0 10,0 5,10\"/><polyline points=\"0 0, 10 10\"/></svg>", document));
    CTEST_ASSERT(document.shapes.size() == 2u);
    CTEST_ASSERT(document.shapes[0].subPaths[0].closed);
    CTEST_ASSERT(document.shapes[0].subPaths[0].segments.size() == 2u);
    CTEST_ASSERT(local_near(document.shapes[0].subPaths[0].segments[1].end, 5.0, 10.0));
    CTEST_ASSERT(!document.shapes[1].subPaths[0].closed);
    CTEST_ASSERT(document.shapes[1].subPaths[0].segments.size() == 1u);
    return true;
}

bool TestEzSvgReader_RefusesADocumentWithoutSvgRoot() {
    ez::svg::Document document;
    std::string error;
    CTEST_ASSERT(!ez::svg::Reader::parse("<html><body/></html>", document, error));
    CTEST_ASSERT(!error.empty());
    CTEST_ASSERT(!ez::svg::Reader::parse("", document, error));
    return true;
}

// a gradient fill : the shape stays (its outline is the point), the
// paint is unsupported and the warning says it
bool TestEzSvgReader_UnsupportedFillKeepsTheShape() {
    ez::svg::Document document;
    CTEST_ASSERT(local_read("<svg><path fill=\"url(#g)\" d=\"M0 0h1v1z\"/></svg>", document));
    CTEST_ASSERT(document.shapes.size() == 1u);
    CTEST_ASSERT(document.shapes[0].fill.kind == ez::svg::PaintKind::Unsupported);
    CTEST_ASSERT(document.shapes[0].fill.reference == "#g");
    CTEST_ASSERT(local_warningMentions(document, "1 fills unsupported"));
    return true;
}

bool TestEzSvgReader_NamespacePrefixesAreStripped() {
    ez::svg::Document document;
    CTEST_ASSERT(local_read("<svg:svg xmlns:svg=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 8 8\"><svg:path d=\"M0 0h1v1z\"/></svg:svg>", document));
    CTEST_ASSERT(document.hasViewBox);
    CTEST_ASSERT(document.shapes.size() == 1u);
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzSvgReader(const std::string& vTest) {
    IfTestExist(TestEzSvgReader_ASquarePathIsOneShape);
    else IfTestExist(TestEzSvgReader_ACircleBecomesCubics);
    else IfTestExist(TestEzSvgReader_GroupTransformsCompose);
    else IfTestExist(TestEzSvgReader_GroupFillIsInherited);
    else IfTestExist(TestEzSvgReader_StyleWinsOverTheAttribute);
    else IfTestExist(TestEzSvgReader_StrokeOnlyShapesAreCountedNotKept);
    else IfTestExist(TestEzSvgReader_HiddenElementsAreSkipped);
    else IfTestExist(TestEzSvgReader_UnsupportedElementsAreCounted);
    else IfTestExist(TestEzSvgReader_PrologueDoctypeAndCommentsPass);
    else IfTestExist(TestEzSvgReader_LengthsWithUnits);
    else IfTestExist(TestEzSvgReader_RoundedRectHasCorners);
    else IfTestExist(TestEzSvgReader_PolygonAndPolyline);
    else IfTestExist(TestEzSvgReader_RefusesADocumentWithoutSvgRoot);
    else IfTestExist(TestEzSvgReader_UnsupportedFillKeepsTheShape);
    else IfTestExist(TestEzSvgReader_NamespacePrefixesAreStripped);
    return false;
}
