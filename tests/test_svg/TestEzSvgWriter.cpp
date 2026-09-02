#include <TestEzSvgWriter.h>

#include <cstdio>
#include <string>

#include <ezlibs/ezSvg/ezSvg.hpp>
#include <ezlibs/ezCTest.hpp>

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

// what the writer writes, the reader reads back : two painted shapes, in
// the order they were added, with their fills
bool TestEzSvgWriter_RoundTripsThroughTheReader() {
    const char* pFilePath = "ezsvg_round_trip.svg";
    {
        ez::svg::Writer writer(ez::math::uvec2(100U, 50U));
        writer.addRectangle(ez::math::uvec2(10U, 10U), ez::math::uvec2(30U, 20U), "red");
        writer.addCircle(ez::math::uvec2(70U, 25U), 10, "blue");
        writer.exportToFile(pFilePath);
    }
    ez::svg::Document document;
    std::string error;
    const bool read = ez::svg::Reader::parseFile(pFilePath, document, error);
    std::remove(pFilePath);
    CTEST_ASSERT(read);
    CTEST_ASSERT(document.width == 100.0);
    CTEST_ASSERT(document.height == 50.0);
    CTEST_ASSERT(document.shapes.size() == 2u);
    CTEST_ASSERT(document.shapes[0].fill.kind == ez::svg::PaintKind::Color);
    CTEST_ASSERT(document.shapes[0].fill.red == 255);
    CTEST_ASSERT(document.shapes[0].subPaths[0].segments.size() == 4u);
    CTEST_ASSERT(document.shapes[1].fill.blue == 255);
    CTEST_ASSERT(document.shapes[1].subPaths[0].segments.size() == 4u);
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzSvgWriter(const std::string& vTest) {
    IfTestExist(TestEzSvgWriter_RoundTripsThroughTheReader);
    return false;
}
