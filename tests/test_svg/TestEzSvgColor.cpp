#include <TestEzSvgColor.h>

#include <string>

#include <ezlibs/ezSvg/ezSvg.hpp>
#include <ezlibs/ezCTest.hpp>

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

namespace {

bool local_isColor(const char* aText, int32_t aRed, int32_t aGreen, int32_t aBlue, int32_t aAlpha = 255) {
    ez::svg::Paint paint;
    if (!ez::svg::ColorParser::parse(aText, paint)) {
        return false;
    }
    return (paint.kind == ez::svg::PaintKind::Color) && (paint.red == aRed) && (paint.green == aGreen) && (paint.blue == aBlue) && (paint.alpha == aAlpha);
}

}  // namespace

bool TestEzSvgColor_HexForms() {
    CTEST_ASSERT(local_isColor("#f00", 255, 0, 0));
    CTEST_ASSERT(local_isColor("#F00", 255, 0, 0));
    CTEST_ASSERT(local_isColor("#ff0000", 255, 0, 0));
    CTEST_ASSERT(local_isColor("#1a2b3c", 26, 43, 60));
    CTEST_ASSERT(local_isColor("#ff000080", 255, 0, 0, 128));
    CTEST_ASSERT(local_isColor("#f008", 255, 0, 0, 136));
    return true;
}

bool TestEzSvgColor_FunctionalForms() {
    CTEST_ASSERT(local_isColor("rgb(0, 128, 255)", 0, 128, 255));
    CTEST_ASSERT(local_isColor("rgb(100%, 0%, 50%)", 255, 0, 128));
    CTEST_ASSERT(local_isColor("rgba(0,0,0,0.5)", 0, 0, 0, 128));
    CTEST_ASSERT(local_isColor("rgb(255 0 0 / 50%)", 255, 0, 0, 128));
    CTEST_ASSERT(local_isColor("hsl(120, 100%, 50%)", 0, 255, 0));
    CTEST_ASSERT(local_isColor("hsl(0, 0%, 50%)", 128, 128, 128));
    return true;
}

bool TestEzSvgColor_KeywordsAndNames() {
    ez::svg::Paint paint;
    CTEST_ASSERT(ez::svg::ColorParser::parse("none", paint));
    CTEST_ASSERT(paint.kind == ez::svg::PaintKind::None);
    CTEST_ASSERT(!paint.isVisible());
    CTEST_ASSERT(ez::svg::ColorParser::parse("currentColor", paint));
    CTEST_ASSERT(paint.kind == ez::svg::PaintKind::CurrentColor);
    CTEST_ASSERT(paint.isVisible());
    CTEST_ASSERT(ez::svg::ColorParser::parse("transparent", paint));
    CTEST_ASSERT(!paint.isVisible());
    CTEST_ASSERT(local_isColor(" Red ", 255, 0, 0));
    CTEST_ASSERT(local_isColor("rebeccapurple", 102, 51, 153));
    CTEST_ASSERT(local_isColor("yellowgreen", 154, 205, 50));
    CTEST_ASSERT(local_isColor("aliceblue", 240, 248, 255));
    return true;
}

// a gradient or a pattern : the paint is kept as unsupported, with the
// reference the consumer may want to name
bool TestEzSvgColor_UnsupportedPaintKeepsItsReference() {
    ez::svg::Paint paint;
    CTEST_ASSERT(ez::svg::ColorParser::parse("url(#grad1)", paint));
    CTEST_ASSERT(paint.kind == ez::svg::PaintKind::Unsupported);
    CTEST_ASSERT(paint.reference == "#grad1");
    CTEST_ASSERT(paint.isVisible());
    return true;
}

bool TestEzSvgColor_RefusesNonsense() {
    ez::svg::Paint paint = ez::svg::Paint::color(1, 2, 3);
    CTEST_ASSERT(!ez::svg::ColorParser::parse("#12", paint));
    CTEST_ASSERT(!ez::svg::ColorParser::parse("#gg0000", paint));
    CTEST_ASSERT(!ez::svg::ColorParser::parse("blurple", paint));
    CTEST_ASSERT(!ez::svg::ColorParser::parse("", paint));
    CTEST_ASSERT(!ez::svg::ColorParser::parse("rgb(1, 2)", paint));
    CTEST_ASSERT(paint.red == 1);  // untouched on a refusal
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzSvgColor(const std::string& vTest) {
    IfTestExist(TestEzSvgColor_HexForms);
    else IfTestExist(TestEzSvgColor_FunctionalForms);
    else IfTestExist(TestEzSvgColor_KeywordsAndNames);
    else IfTestExist(TestEzSvgColor_UnsupportedPaintKeepsItsReference);
    else IfTestExist(TestEzSvgColor_RefusesNonsense);
    return false;
}
