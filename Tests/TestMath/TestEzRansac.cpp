#include <ezlibs/ezRansac.hpp>
#include <ezlibs/ezMath.hpp>
#include <ezlibs/ezCTest.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244)
#pragma warning(disable : 4305)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#endif

////////////////////////////////////////////////////////////////////////////
// Toy 2-D line fitting case used by several tests below.
////////////////////////////////////////////////////////////////////////////

namespace {

struct point2D {
    float x;
    float y;
};

// Line in slope-intercept form: y = a*x + b
struct line2D {
    float a;
    float b;
};

// Fit a line through 2 points. Returns false if the points are vertically
// aligned (degenerate model). C++11-compatible out-parameter style.
bool estimateLine(const std::vector<point2D>& aSamples, line2D& aoOutLine) {
    if (aSamples.size() < 2) {
        return false;
    }
    const float dx = aSamples[1].x - aSamples[0].x;
    if (std::abs(dx) < 1e-6f) {
        return false;
    }
    aoOutLine.a = (aSamples[1].y - aSamples[0].y) / dx;
    aoOutLine.b = aSamples[0].y - aoOutLine.a * aSamples[0].x;
    return true;
}

// Inlier if the absolute vertical distance to the line is below threshold.
struct lineInlierTester {
    float threshold;
    bool operator()(const line2D& aLine, const point2D& aSample) const {
        const float predicted = aLine.a * aSample.x + aLine.b;
        return std::abs(predicted - aSample.y) < threshold;
    }
};

// Build a synthetic data set: aInlierCount points along y = aTrueA*x + aTrueB
// (with optional small noise) plus aOutlierCount points scattered far away.
std::vector<point2D> makeLineDataset(
    float aTrueA,
    float aTrueB,
    std::size_t aInlierCount,
    std::size_t aOutlierCount,
    float aNoiseAmplitude) {
    std::vector<point2D> samples;
    samples.reserve(aInlierCount + aOutlierCount);
    // Deterministic pseudo-random for reproducibility (don't pull <random>
    // here; a tiny LCG is more than enough for test data).
    std::uint32_t seed = 12345;
    auto next = [&seed]() {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<float>(seed & 0xFFFF) / 65535.0f;  // [0, 1]
    };
    for (std::size_t i = 0; i < aInlierCount; ++i) {
        const float x = static_cast<float>(i) * 0.1f;
        const float noise = (next() - 0.5f) * aNoiseAmplitude;
        samples.push_back({x, aTrueA * x + aTrueB + noise});
    }
    for (std::size_t i = 0; i < aOutlierCount; ++i) {
        const float x = next() * 10.0f;
        const float y = next() * 100.0f - 50.0f;  // far from the true line
        samples.push_back({x, y});
    }
    return samples;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzRansac_EmptySamplesReturnFailure() {
    std::vector<point2D> empty;
    ez::ransacConfig config;
    config.minModelSamples = 2;
    auto result = ez::ransac<line2D, point2D>(empty, config, estimateLine, lineInlierTester{0.1f});
    CTEST_ASSERT(!result.success);
    return true;
}

bool TestEzRansac_TooFewSamplesReturnFailure() {
    // Need at least 2 * minModelSamples = 4 samples.
    std::vector<point2D> samples = {{0, 0}, {1, 1}, {2, 2}};
    ez::ransacConfig config;
    config.minModelSamples = 2;
    auto result = ez::ransac<line2D, point2D>(samples, config, estimateLine, lineInlierTester{0.1f});
    CTEST_ASSERT(!result.success);
    return true;
}

bool TestEzRansac_FitsLineThroughOutlierContaminatedData() {
    // 100 inliers on y = 2x + 1, 30 outliers, small noise.
    auto samples = makeLineDataset(2.0f, 1.0f, 100, 30, 0.01f);
    ez::ransacConfig config;
    config.minModelSamples = 2;
    config.confidence = 0.99f;
    config.maxIterations = 5000;
    config.randomSeed = 42;

    auto result = ez::ransac<line2D, point2D>(samples, config, estimateLine, lineInlierTester{0.05f});

    CTEST_ASSERT(result.success);
    // Most of the 100 inliers should be recovered; the bound is loose to
    // tolerate the deterministic-but-arbitrary RNG draw and the noise.
    CTEST_ASSERT(result.inlierIndices.size() >= 90);
    // Slope and intercept should be close to the true values.
    CTEST_ASSERT(ez::isEqual(result.model.a, 2.0f, 0.05f));
    CTEST_ASSERT(ez::isEqual(result.model.b, 1.0f, 0.05f));
    return true;
}

bool TestEzRansac_AllInliersConvergesQuickly() {
    // Perfect noiseless data: the very first valid estimate must match
    // every sample, so the adaptive iteration count drops to 1 immediately.
    auto samples = makeLineDataset(3.0f, -2.0f, 50, 0, 0.0f);
    ez::ransacConfig config;
    config.minModelSamples = 2;
    config.maxIterations = 1000;
    config.randomSeed = 7;

    auto result = ez::ransac<line2D, point2D>(samples, config, estimateLine, lineInlierTester{0.001f});
    CTEST_ASSERT(result.success);
    CTEST_ASSERT(result.inlierIndices.size() == samples.size());
    // The adaptive update should kick in quickly; we don't assert the
    // exact iteration count (RNG-dependent) but it must be small.
    CTEST_ASSERT(result.iterations < 100);
    return true;
}

bool TestEzRansac_ProsacReachesSameSolution() {
    // Same dataset as the contaminated test, but with samplesAreSorted = true.
    // Samples here are not actually sorted by quality (no quality info), so
    // PROSAC degrades to a slightly different sampling order, but it must
    // still find the line.
    auto samples = makeLineDataset(2.0f, 1.0f, 100, 30, 0.01f);
    ez::ransacConfig config;
    config.minModelSamples = 2;
    config.confidence = 0.99f;
    config.maxIterations = 5000;
    config.samplesAreSorted = true;
    config.randomSeed = 123;

    auto result = ez::ransac<line2D, point2D>(samples, config, estimateLine, lineInlierTester{0.05f});
    CTEST_ASSERT(result.success);
    CTEST_ASSERT(result.inlierIndices.size() >= 90);
    return true;
}

bool TestEzRansac_DegenerateEstimatorIsSkipped() {
    // All samples on the same x: estimator returns nullopt for every draw.
    // The function must terminate without succeeding, not loop forever.
    std::vector<point2D> samples;
    for (std::size_t i = 0; i < 20; ++i) {
        samples.push_back({1.0f, static_cast<float>(i)});  // all share x = 1
    }
    ez::ransacConfig config;
    config.minModelSamples = 2;
    config.maxIterations = 500;
    config.randomSeed = 99;

    auto result = ez::ransac<line2D, point2D>(samples, config, estimateLine, lineInlierTester{0.5f});
    CTEST_ASSERT(!result.success);
    CTEST_ASSERT(result.iterations == 500);  // stopped on iteration cap
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzRansac(const std::string& vTest) {
    IfTestExist(TestEzRansac_EmptySamplesReturnFailure);
    else IfTestExist(TestEzRansac_TooFewSamplesReturnFailure);
    else IfTestExist(TestEzRansac_FitsLineThroughOutlierContaminatedData);
    else IfTestExist(TestEzRansac_AllInliersConvergesQuickly);
    else IfTestExist(TestEzRansac_ProsacReachesSameSolution);
    else IfTestExist(TestEzRansac_DegenerateEstimatorIsSkipped);
    return false;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
#pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
