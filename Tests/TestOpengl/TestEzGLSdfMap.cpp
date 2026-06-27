#include "TestEzGLSdfMap.h"
#include "glContext.h"
#include <ezlibs/ezGL/ezGL.hpp>
#include <ezlibs/ezCTest.hpp>

#include <vector>

// SdfMap needs compute shaders : ask for a GL 4.3 core context explicitly.
static bool s_initComputeContext() {
    return GLContext::initGLContext(4, 3);
}

// World-space center of texel (vPx, vPy), matching the bake mapping
// p = mix(worldMin, worldMax, (px + 0.5) / res). Sampling here hits a texel
// center, so the bilinear lookup returns the stored value with no interpolation.
static ez::math::fvec2 s_texelCenter(
    const int vPx, const int vPy, const int vResX, const int vResY, const ez::math::fvec2& vWorldMin, const ez::math::fvec2& vWorldMax) {
    const ez::math::fvec2 extent = vWorldMax - vWorldMin;
    const float normX = (static_cast<float>(vPx) + 0.5f) / static_cast<float>(vResX);
    const float normY = (static_cast<float>(vPy) + 0.5f) / static_cast<float>(vResY);
    return ez::math::fvec2(vWorldMin.x + normX * extent.x, vWorldMin.y + normY * extent.y);
}

bool TestEzGL_SdfMap_Create() {
    CTEST_ASSERT(s_initComputeContext());
    auto sdfMap = ez::gl::SdfMap::create("TestSdfMap");
    CTEST_ASSERT(sdfMap != nullptr);
    CTEST_ASSERT(!sdfMap->isBaked());
    GLContext::unitGLContext();
    return true;
}

bool TestEzGL_SdfMap_BakeSingleBox() {
    CTEST_ASSERT(s_initComputeContext());
    auto sdfMap = ez::gl::SdfMap::create("TestSdfMap");
    CTEST_ASSERT(sdfMap != nullptr);

    const ez::math::fvec2 worldMin(-10.0f, -10.0f);
    const ez::math::fvec2 worldMax(10.0f, 10.0f);
    const int resX = 64;
    const int resY = 64;

    const ez::math::fAABB box(ez::math::fvec2(-3.0f, -2.0f), ez::math::fvec2(4.0f, 3.0f));
    std::vector<ez::gl::SdfMap::BoxData> boxes;
    boxes.push_back(ez::gl::SdfMap::BoxData{ez::math::fvec2(-3.0f, -2.0f), ez::math::fvec2(4.0f, 3.0f)});

    CTEST_ASSERT(sdfMap->bake(boxes, worldMin, worldMax, resX, resY));
    CTEST_ASSERT(sdfMap->isBaked());
    CTEST_ASSERT(sdfMap->getField().size() == static_cast<size_t>(resX) * static_cast<size_t>(resY));
    CTEST_ASSERT(sdfMap->getPreviewTextureId() != 0U);  // colorized preview produced for the debug view

    // sample at a few texel centers and compare to the CPU reference. At a texel
    // center the bilinear lookup is exact, so the only error is the R32F round-trip.
    const int probes[][2] = {{32, 32}, {10, 10}, {50, 40}, {5, 60}, {32, 5}};
    for (size_t i = 0; i < sizeof(probes) / sizeof(probes[0]); ++i) {
        const ez::math::fvec2 point = s_texelCenter(probes[i][0], probes[i][1], resX, resY, worldMin, worldMax);
        const float gpu = sdfMap->sample(point);
        const float cpu = ez::math::sdfToBox(point, box);
        CTEST_ASSERT(ez::math::isEqual(gpu, cpu, 1e-2f));
    }
    GLContext::unitGLContext();
    return true;
}

bool TestEzGL_SdfMap_BakeMultipleBoxes() {
    CTEST_ASSERT(s_initComputeContext());
    auto sdfMap = ez::gl::SdfMap::create("TestSdfMap");
    CTEST_ASSERT(sdfMap != nullptr);

    const ez::math::fvec2 worldMin(-20.0f, -20.0f);
    const ez::math::fvec2 worldMax(20.0f, 20.0f);
    const int resX = 96;
    const int resY = 96;

    std::vector<ez::math::fAABB> cpuBoxes;
    cpuBoxes.push_back(ez::math::fAABB(ez::math::fvec2(-12.0f, -10.0f), ez::math::fvec2(-4.0f, -2.0f)));
    cpuBoxes.push_back(ez::math::fAABB(ez::math::fvec2(2.0f, 1.0f), ez::math::fvec2(9.0f, 8.0f)));
    cpuBoxes.push_back(ez::math::fAABB(ez::math::fvec2(-2.0f, 6.0f), ez::math::fvec2(3.0f, 14.0f)));

    std::vector<ez::gl::SdfMap::BoxData> boxes;
    for (size_t i = 0; i < cpuBoxes.size(); ++i) {
        boxes.push_back(ez::gl::SdfMap::BoxData{cpuBoxes[i].lowerBound, cpuBoxes[i].upperBound});
    }

    CTEST_ASSERT(sdfMap->bake(boxes, worldMin, worldMax, resX, resY));

    const int probes[][2] = {{48, 48}, {20, 20}, {70, 30}, {30, 70}, {80, 80}};
    for (size_t i = 0; i < sizeof(probes) / sizeof(probes[0]); ++i) {
        const ez::math::fvec2 point = s_texelCenter(probes[i][0], probes[i][1], resX, resY, worldMin, worldMax);
        const float gpu = sdfMap->sample(point);
        const float cpu = ez::math::sdfToBoxes(point, cpuBoxes);
        CTEST_ASSERT(ez::math::isEqual(gpu, cpu, 1e-2f));
    }
    GLContext::unitGLContext();
    return true;
}

bool TestEzGL_SdfMap_EmptyBoxes() {
    CTEST_ASSERT(s_initComputeContext());
    auto sdfMap = ez::gl::SdfMap::create("TestSdfMap");
    CTEST_ASSERT(sdfMap != nullptr);

    std::vector<ez::gl::SdfMap::BoxData> boxes;  // no obstacle
    CTEST_ASSERT(sdfMap->bake(boxes, ez::math::fvec2(-5.0f, -5.0f), ez::math::fvec2(5.0f, 5.0f), 32, 32));
    // with no box the shortest distance stays at the shader's sentinel : a large value
    CTEST_ASSERT(sdfMap->sample(ez::math::fvec2(0.0f, 0.0f)) > 1.0e6f);
    GLContext::unitGLContext();
    return true;
}

bool TestEzGL_SdfMap_DegenerateWindow() {
    CTEST_ASSERT(s_initComputeContext());
    auto sdfMap = ez::gl::SdfMap::create("TestSdfMap");
    CTEST_ASSERT(sdfMap != nullptr);

    std::vector<ez::gl::SdfMap::BoxData> boxes;
    boxes.push_back(ez::gl::SdfMap::BoxData{ez::math::fvec2(0.0f, 0.0f), ez::math::fvec2(1.0f, 1.0f)});
    // zero-area window must be rejected
    CTEST_ASSERT(!sdfMap->bake(boxes, ez::math::fvec2(0.0f, 0.0f), ez::math::fvec2(0.0f, 10.0f), 16, 16));
    GLContext::unitGLContext();
    return true;
}

////////////////////////////////////////////////////////////////////////////
//// ENTRY POINT ///////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzGL_SdfMap(const std::string& vTest) {
    IfTestExist(TestEzGL_SdfMap_Create);
    else IfTestExist(TestEzGL_SdfMap_BakeSingleBox);
    else IfTestExist(TestEzGL_SdfMap_BakeMultipleBoxes);
    else IfTestExist(TestEzGL_SdfMap_EmptyBoxes);
    else IfTestExist(TestEzGL_SdfMap_DegenerateWindow);
    return false;
}
