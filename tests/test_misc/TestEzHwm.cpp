#include <TestEzHwm.h>
#include <ezlibs/ezCTest.hpp>
#include <ezlibs/ezHwm.hpp>

#include <chrono>
#include <cmath>

namespace {

// keeps this process on the cpu for a while so the sampler has something
// real to measure. the accumulator is observed at the end so the optimizer
// cannot elide the loop
void burnCpu(int64_t aDurationMs) {
    const auto startTime = std::chrono::steady_clock::now();
    double accumulator = 0.0;
    uint64_t iterationIndex = 0;
    while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count() < aDurationMs) {
        accumulator += std::sin(static_cast<double>(iterationIndex) * 0.001);
        ++iterationIndex;
    }
    volatile double observableSink = accumulator;
    (void)observableSink;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

// create() inits the sampler and returns null when the platform counters
// refuse : a non-null sampler measures straight away. before that contract,
// create() handed back an UNinitialized sampler whose sample() returned
// early, and every caller that trusted it read 0% forever
bool TestEzHwm_CreateReturnsInitializedSampler() {
    ez::hwm::Sampler::SamplerPtr sampler = ez::hwm::Sampler::create();
    CTEST_ASSERT(sampler != nullptr);
    sampler->setMinSampleIntervalMs(0);  // no throttle : every sample() recomputes
    burnCpu(400);
    sampler->sample();
    CTEST_ASSERT_MESSAGE(sampler->getCpuUsagePercent() > 0.0, "the sampler measured nothing : create() did not init it");
    return true;
}

// whatever the platform reports, the served percents stay a percentage
bool TestEzHwm_PercentsStayInRange() {
    ez::hwm::Sampler::SamplerPtr sampler = ez::hwm::Sampler::create();
    CTEST_ASSERT(sampler != nullptr);
    sampler->setMinSampleIntervalMs(0);
    burnCpu(100);
    sampler->sample();
    const double cpuUsagePercent = sampler->getCpuUsagePercent();
    CTEST_ASSERT(cpuUsagePercent >= 0.0 && cpuUsagePercent <= 100.0);
    const double gpuUsagePercent = sampler->getGpuUsagePercent();
    CTEST_ASSERT(gpuUsagePercent >= 0.0 && gpuUsagePercent <= 100.0);
    // a gpu-less process reports no gpu measure and a flat zero, never garbage
    if (!sampler->isGpuMeasureAvailable()) {
        CTEST_ASSERT(gpuUsagePercent == 0.0);
    }
    return true;
}

// unit() releases the platform state and clears the served measures : a
// later sample() is a no-op, not a crash on the freed state
bool TestEzHwm_SampleAfterUnitIsSafe() {
    ez::hwm::Sampler::SamplerPtr sampler = ez::hwm::Sampler::create();
    CTEST_ASSERT(sampler != nullptr);
    sampler->setMinSampleIntervalMs(0);
    burnCpu(100);
    sampler->sample();
    CTEST_ASSERT(sampler->getCpuUsagePercent() > 0.0);
    sampler->unit();
    CTEST_ASSERT(sampler->getCpuUsagePercent() == 0.0);
    CTEST_ASSERT(!sampler->isGpuMeasureAvailable());
    sampler->sample();  // no platform state left : must stay a no-op
    CTEST_ASSERT(sampler->getCpuUsagePercent() == 0.0);
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzHwm(const std::string& vTest) {
    IfTestExist(TestEzHwm_CreateReturnsInitializedSampler);
    else IfTestExist(TestEzHwm_PercentsStayInRange);
    else IfTestExist(TestEzHwm_SampleAfterUnitIsSafe);
    return false;
}
