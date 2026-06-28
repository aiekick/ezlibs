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

// ezDiagram is part of the ezLibs project : https://github.com/aiekick/ezLibs.git
// C++11 strict (the authoritative gate is Clang/Linux ; MSVC is lax).

#include <vector>
#include <cstddef>

namespace ez {
namespace diagram {

// Solve the 1-D separation problem on an ORDERED chain : find positions p[0..n-1] minimizing
//     sum_i  weight_i * (p_i - desired_i)^2
// subject to the chain separation constraints
//     p_{i+1} - p_i >= minGap_i        (i in 0 .. n-2)
// in the GIVEN, fixed order. The optimum is exact and computed in O(n) by weighted isotonic
// regression (pool-adjacent-violators) on the gap-removed coordinates : because the constraints form
// a total order, no general active-set VPSC solve is needed. This is the placement step that makes
// "positions serve the (crossing-minimal) order" : the order is enforced by the constraints, the
// desired positions (e.g. neighbour barycenters, or a pinned coordinate) only pull within them.
//
// aDesired / aWeight have size n. aMinGap has size n-1 (or may be shorter / empty : missing gaps are
// treated as 0). Non-positive weights are floored to a tiny positive value so a 0-weight variable
// still gets a finite position. The result is written to aoPositions (resized to n).
inline void solveSeparation1D(  //
    const std::vector<float>& aDesired,
    const std::vector<float>& aWeight,
    const std::vector<float>& aMinGap,
    std::vector<float>& aoPositions) {
    const size_t count = aDesired.size();
    aoPositions.assign(count, 0.0f);
    if (count == 0U) {
        return;
    }

    // cumulative gap offset : offset[i] = sum of the gaps before index i. Removing it turns the
    // separation constraints into a plain monotonicity (non-decreasing) constraint.
    std::vector<double> offset(count, 0.0);
    for (size_t index = 1U; index < count; ++index) {
        const double gap = ((index - 1U) < aMinGap.size()) ? static_cast<double>(aMinGap[index - 1U]) : 0.0;
        offset[index] = offset[index - 1U] + gap;
    }

    // weighted isotonic regression on e_i = desired_i - offset_i : a stack of merged blocks, each
    // merging while the previous block mean would exceed the current one (a monotonicity violation).
    struct Block {
        double weightedSum{0.0};  // sum of weight_k * e_k over the block
        double weight{0.0};       // sum of weight_k over the block
        size_t span{0U};          // number of entries merged into the block
    };
    std::vector<Block> blocks;
    blocks.reserve(count);
    for (size_t index = 0U; index < count; ++index) {
        double weight = (aWeight.size() > index) ? static_cast<double>(aWeight[index]) : 1.0;
        if (weight < 1e-9) {
            weight = 1e-9;
        }
        const double value = static_cast<double>(aDesired[index]) - offset[index];
        Block block;
        block.weightedSum = weight * value;
        block.weight = weight;
        block.span = 1U;
        while (!blocks.empty()) {
            const Block& previous = blocks.back();
            const double previousMean = previous.weightedSum / previous.weight;
            const double currentMean = block.weightedSum / block.weight;
            if (previousMean <= currentMean) {
                break;  // monotone already : keep the previous block
            }
            block.weightedSum += previous.weightedSum;
            block.weight += previous.weight;
            block.span += previous.span;
            blocks.pop_back();
        }
        blocks.push_back(block);
    }

    // expand each block back to its entries, re-adding the gap offset : p_i = blockValue + offset_i.
    size_t writeIndex = 0U;
    for (size_t blockIndex = 0U; blockIndex < blocks.size(); ++blockIndex) {
        const double blockValue = blocks[blockIndex].weightedSum / blocks[blockIndex].weight;
        for (size_t inner = 0U; inner < blocks[blockIndex].span; ++inner) {
            aoPositions[writeIndex] = static_cast<float>(blockValue + offset[writeIndex]);
            ++writeIndex;
        }
    }
}

}  // namespace diagram
}  // namespace ez
