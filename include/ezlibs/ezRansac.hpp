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

// ezRansac is part of the ezLibs project : https://github.com/aiekick/ezLibs.git
//
// Generic RANSAC + PROSAC estimator. Given a set of samples that may contain
// outliers, repeatedly draws a minimal random subset, fits a candidate model,
// counts how many samples agree with it, and keeps the best. The number of
// iterations is adaptively reduced as soon as a high-quality model is found,
// using the standard formula:
//
//     k = log(1 − c) / log(1 − p^m)
//
// where c is the desired confidence (e.g. 0.99), p is the current inlier
// ratio, and m is the minimum number of samples needed to instantiate a
// model.
//
// When the input samples are pre-sorted by quality (best first), enabling
// the PROSAC sampling mode (Chum & Matas 2005) tries small "best" subsets
// first and grows the search window progressively. On real-world matching
// problems with sorted-by-confidence inputs, this often converges an order
// of magnitude faster than vanilla RANSAC.
//
// References:
//   - Fischler & Bolles, "Random Sample Consensus: A Paradigm for Model
//     Fitting...", Communications of the ACM, 1981.
//   - Chum & Matas, "Matching with PROSAC — Progressive Sample Consensus",
//     CVPR 2005.

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
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

namespace ez {

struct ransacConfig {
    // Minimum number of samples needed to instantiate a model (e.g. 4 for a
    // homography, 8 for a fundamental matrix).
    std::size_t minModelSamples{4};
    // Desired confidence that the returned model is built from inliers only.
    // Standard range is [0.95, 0.99].
    float confidence{0.99f};
    // Hard cap on iterations regardless of the adaptive formula. The
    // Chum-style adaptive update may bring the effective iteration count
    // far below this cap.
    std::size_t maxIterations{1000000};
    // When true, samples are assumed to be pre-sorted in *decreasing*
    // quality order and PROSAC sampling is used: small "best" subsets are
    // explored first, the search window grows progressively (Chum & Matas
    // 2005, "Tournament Growth").
    bool samplesAreSorted{false};
    // Seed for the internal RNG. The default value of 0 makes runs
    // deterministic (useful for tests). Set to a different value, or to
    // a time-based seed, to randomize.
    std::uint64_t randomSeed{0};
};

template <typename Model>
struct ransacResult {
    bool success{false};
    Model model{};
    // Indices into the original samples vector identifying the inliers
    // of the returned model. Storing indices (not copies) keeps the
    // result small and lets the caller decide whether to materialize
    // them.
    std::vector<std::size_t> inlierIndices{};
    // Number of iterations actually executed (≤ maxIterations).
    std::size_t iterations{0};
};

namespace detail {

// Draw aCount distinct indices in [0, aUpperBoundExclusive) into aoIndices.
// Quadratic in aCount but aCount is the model size m (typically 4 or 8),
// so this is faster than a Fisher-Yates shuffle on a large pool.
template <typename Rng>
inline void drawDistinctIndices(
    Rng& aoRng,
    std::size_t aUpperBoundExclusive,
    std::size_t aCount,
    std::size_t aReservedLastIndex,
    bool aHasReserved,
    std::vector<std::size_t>& aoIndices) {
    aoIndices.clear();
    // Note: we use an explicit index-based loop (no range-for over aoIndices)
    // because the standard <algorithm> contains() helper is C++20.
    if (aHasReserved) {
        // PROSAC mode: the last sample is forced to aReservedLastIndex,
        // the first (aCount - 1) are drawn from [0, aUpperBoundExclusive)
        // excluding aReservedLastIndex.
        std::uniform_int_distribution<std::size_t> dist(0, aUpperBoundExclusive - 1);
        while (aoIndices.size() + 1 < aCount) {
            const std::size_t candidate = dist(aoRng);
            if (candidate == aReservedLastIndex) {
                continue;
            }
            bool duplicate = false;
            for (std::size_t k = 0; k < aoIndices.size(); ++k) {
                if (aoIndices[k] == candidate) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) {
                aoIndices.push_back(candidate);
            }
        }
        aoIndices.push_back(aReservedLastIndex);
    } else {
        // Standard RANSAC: aCount uniform-random samples in
        // [0, aUpperBoundExclusive).
        std::uniform_int_distribution<std::size_t> dist(0, aUpperBoundExclusive - 1);
        while (aoIndices.size() < aCount) {
            const std::size_t candidate = dist(aoRng);
            bool duplicate = false;
            for (std::size_t k = 0; k < aoIndices.size(); ++k) {
                if (aoIndices[k] == candidate) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) {
                aoIndices.push_back(candidate);
            }
        }
    }
}

}  // namespace detail

// Run RANSAC (or PROSAC if aConfig.samplesAreSorted is true).
//
//   aSamples         : the full input set, possibly containing outliers
//   aConfig          : algorithm parameters
//   aEstimator       : bool (const std::vector<Sample>& minSamples,
//                            Model& aoOutModel)
//                      Writes the candidate model into aoOutModel and
//                      returns true on success, false if the subset is
//                      degenerate (output is then ignored).
//                      [out parameter style chosen over std::optional to
//                      keep the header C++11-compatible.]
//   aInlierTester    : bool (const Model& candidate, const Sample& s)
//                      Returns true if s agrees with the candidate model.
//
// Returns a ransacResult with success = true and a populated model whenever
// at least one estimate produced strictly more inliers than the minimum
// model size.
template <typename Model, typename Sample, typename Estimator, typename InlierTester>
ransacResult<Model> ransac(
    const std::vector<Sample>& aSamples,
    const ransacConfig& aConfig,
    Estimator aEstimator,
    InlierTester aInlierTester) {
    ransacResult<Model> output;

    const std::size_t sampleCount = aSamples.size();
    const std::size_t modelSize = aConfig.minModelSamples;
    if (modelSize < 1 || sampleCount < 2 * modelSize) {
        return output;
    }

    std::mt19937_64 rng(aConfig.randomSeed);

    // A useful model must have at least modelSize inliers (it always has
    // its own minimum sample set as inliers, by construction). We start
    // strictly above to require improvement over the trivial baseline.
    std::size_t bestInlierCount = modelSize;

    const float logFailureThreshold = std::log(1.0f - aConfig.confidence);
    std::size_t requiredIterations = aConfig.maxIterations;

    // PROSAC growth state: window starts at modelSize and grows over time.
    std::size_t prosacWindowSize = modelSize;
    std::size_t prosacRoundLimit = 1;
    double prosacTime = 1.0;

    std::vector<std::size_t> drawnIndices;
    drawnIndices.reserve(modelSize);
    std::vector<Sample> minimalSampleSet;
    minimalSampleSet.reserve(modelSize);
    std::vector<std::size_t> currentInlierIndices;
    currentInlierIndices.reserve(sampleCount);

    std::size_t iter = 0;
    const std::size_t hardCap = aConfig.maxIterations;
    while (iter < requiredIterations && iter < hardCap) {
        // ---- 1) Sample a minimal set of indices ----
        if (aConfig.samplesAreSorted && prosacWindowSize < sampleCount) {
            // PROSAC tournament growth: optionally enlarge the window
            // before drawing.
            if (iter > prosacRoundLimit) {
                ++prosacWindowSize;
                if (prosacWindowSize > sampleCount) {
                    prosacWindowSize = sampleCount;
                }
                if (prosacWindowSize > modelSize) {
                    const double w = static_cast<double>(prosacWindowSize);
                    const double m = static_cast<double>(modelSize);
                    const double nextTime = prosacTime * w / (w - m);
                    const double delta = std::ceil(nextTime - prosacTime);
                    prosacRoundLimit *= (1 + static_cast<std::size_t>(delta));
                    prosacTime = nextTime;
                }
            }
            detail::drawDistinctIndices(rng, prosacWindowSize, modelSize,
                                        prosacWindowSize - 1, true, drawnIndices);
        } else {
            detail::drawDistinctIndices(rng, sampleCount, modelSize, 0, false, drawnIndices);
        }

        // ---- 2) Estimate a candidate model ----
        minimalSampleSet.clear();
        for (std::size_t k = 0; k < drawnIndices.size(); ++k) {
            minimalSampleSet.push_back(aSamples[drawnIndices[k]]);
        }
        Model candidate{};
        if (!aEstimator(minimalSampleSet, candidate)) {
            ++iter;
            continue;
        }

        // ---- 3) Score the candidate by counting inliers ----
        currentInlierIndices.clear();
        for (std::size_t i = 0; i < sampleCount; ++i) {
            if (aInlierTester(candidate, aSamples[i])) {
                currentInlierIndices.push_back(i);
            }
        }

        // ---- 4) Update best-so-far + adaptive iteration count ----
        if (currentInlierIndices.size() > bestInlierCount) {
            bestInlierCount = currentInlierIndices.size();
            output.success = true;
            output.model = candidate;
            output.inlierIndices = currentInlierIndices;

            const float inlierRatio = static_cast<float>(bestInlierCount) / static_cast<float>(sampleCount);
            if (inlierRatio >= 1.0f) {
                requiredIterations = iter + 1;  // perfect model, stop now
            } else if (inlierRatio > 0.0f) {
                const float pPowM = std::pow(inlierRatio, static_cast<float>(modelSize));
                if (pPowM < 1.0f) {
                    const float logDenominator = std::log(1.0f - pPowM);
                    if (logDenominator < 0.0f) {
                        const float k = logFailureThreshold / logDenominator;
                        if (k > 0.0f) {
                            const std::size_t newRequired = 1 + static_cast<std::size_t>(k);
                            if (newRequired < requiredIterations) {
                                requiredIterations = newRequired;
                            }
                        }
                    }
                }
            }
        }

        ++iter;
    }

    output.iterations = iter;
    return output;
}

}  // namespace ez

#ifdef _MSC_VER
#pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
