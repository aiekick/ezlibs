#include <TestEzSvgTransform.h>

#include <cmath>
#include <string>

#include <ezlibs/ezSvg/ezSvg.hpp>
#include <ezlibs/ezCTest.hpp>

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

namespace {

bool local_maps(const char* aTransform, double aX, double aY, double aExpectedX, double aExpectedY) {
    ez::svg::Matrix matrix;
    std::string error;
    if (!ez::svg::TransformParser::parse(aTransform, matrix, error)) {
        return false;
    }
    const ez::svg::Point point = matrix.apply(ez::svg::Point(aX, aY));
    return (std::fabs(point.x - aExpectedX) < 1e-6) && (std::fabs(point.y - aExpectedY) < 1e-6);
}

}  // namespace

bool TestEzSvgTransform_TranslateMovesAPoint() {
    CTEST_ASSERT(local_maps("translate(10 20)", 1.0, 1.0, 11.0, 21.0));
    CTEST_ASSERT(local_maps("translate(5)", 1.0, 1.0, 6.0, 1.0));
    CTEST_ASSERT(local_maps("translate(10,20)", 0.0, 0.0, 10.0, 20.0));
    return true;
}

// rotate(a cx cy) turns about the center : a quarter turn in the svg
// frame (y down) sends the point to the right of the center below it
bool TestEzSvgTransform_RotateAroundACenter() {
    CTEST_ASSERT(local_maps("rotate(90 10 10)", 20.0, 10.0, 10.0, 20.0));
    CTEST_ASSERT(local_maps("rotate(90)", 1.0, 0.0, 0.0, 1.0));
    CTEST_ASSERT(local_maps("rotate(180)", 1.0, 0.0, -1.0, 0.0));
    return true;
}

// a list composes in order, the rightmost applying first to the point
bool TestEzSvgTransform_AListAppliesRightToLeft() {
    CTEST_ASSERT(local_maps("translate(10 0) scale(2)", 1.0, 1.0, 12.0, 2.0));
    CTEST_ASSERT(local_maps("scale(2) translate(10 0)", 1.0, 1.0, 22.0, 2.0));
    CTEST_ASSERT(local_maps("scale(2 3)", 1.0, 1.0, 2.0, 3.0));
    return true;
}

bool TestEzSvgTransform_MatrixAndSkew() {
    CTEST_ASSERT(local_maps("matrix(1 0 0 1 5 5)", 1.0, 1.0, 6.0, 6.0));
    CTEST_ASSERT(local_maps("matrix(2 0 0 2 0 0)", 1.0, 1.0, 2.0, 2.0));
    CTEST_ASSERT(local_maps("skewX(45)", 1.0, 1.0, 2.0, 1.0));
    CTEST_ASSERT(local_maps("skewY(45)", 1.0, 1.0, 1.0, 2.0));
    return true;
}

bool TestEzSvgTransform_RefusesGarbage() {
    ez::svg::Matrix matrix;
    std::string error;
    CTEST_ASSERT(!ez::svg::TransformParser::parse("rotate(", matrix, error));
    CTEST_ASSERT(!error.empty());
    CTEST_ASSERT(!ez::svg::TransformParser::parse("spin(1)", matrix, error));
    CTEST_ASSERT(!ez::svg::TransformParser::parse("scale(1 2 3)", matrix, error));
    CTEST_ASSERT(!ez::svg::TransformParser::parse("rotate(1 2)", matrix, error));
    CTEST_ASSERT(matrix.isIdentity());
    CTEST_ASSERT(ez::svg::TransformParser::parse("", matrix, error));
    CTEST_ASSERT(matrix.isIdentity());
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzSvgTransform(const std::string& vTest) {
    IfTestExist(TestEzSvgTransform_TranslateMovesAPoint);
    else IfTestExist(TestEzSvgTransform_RotateAroundACenter);
    else IfTestExist(TestEzSvgTransform_AListAppliesRightToLeft);
    else IfTestExist(TestEzSvgTransform_MatrixAndSkew);
    else IfTestExist(TestEzSvgTransform_RefusesGarbage);
    return false;
}
