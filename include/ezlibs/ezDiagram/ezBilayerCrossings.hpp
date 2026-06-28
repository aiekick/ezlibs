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
#include <cstdint>
#include <utility>    // std::pair
#include <algorithm>  // std::stable_sort

namespace ez {
namespace diagram {

// Count the EXACT number of edge crossings between two adjacent layers of a layered (Sugiyama)
// drawing, given the fixed vertex order of each layer. This is the geometry-free oracle that drives
// the within-layer crossing minimization (it never looks at coordinates).
//
// Each edge is a pair { northOrderIndex, southOrderIndex } : the order-index of its endpoint in the
// upper (north) layer and in the lower (south) layer. Two edges cross iff one starts left and ends
// right of the other ; the total count equals the number of inversions in the sequence of south
// positions read in north order, computed with an accumulator (Fenwick / BIT) tree.
//
// aSouthSize is the number of vertices in the south layer (the tree extent) ; every edge's south
// index must lie in [0, aSouthSize). Edges sharing an endpoint never count as a crossing.
//
// Reference : W. Barth, M. Junger, P. Mutzel, "Simple and Efficient Bilayer Cross Counting",
// Graph Drawing 2002. Complexity O(E log S).
inline uint64_t countBilayerCrossings(const std::vector<std::pair<int32_t, int32_t> >& aEdges, int32_t aSouthSize) {
    if (aEdges.size() < 2U || aSouthSize <= 0) {
        return 0U;
    }

    // 1) order the edges by (north, then south) position. stable_sort with an explicit-typed
    //    comparator : NO generic lambda, so ezlibs stays C++11-strict.
    std::vector<std::pair<int32_t, int32_t> > sortedEdges = aEdges;
    std::stable_sort(
        sortedEdges.begin(),
        sortedEdges.end(),
        [](const std::pair<int32_t, int32_t>& aLeft, const std::pair<int32_t, int32_t>& aRight) -> bool {
            if (aLeft.first != aRight.first) {
                return aLeft.first < aRight.first;
            }
            return aLeft.second < aRight.second;
        });

    // 2) accumulator tree over the south positions : leaves count = next power of two >= aSouthSize.
    int32_t leafCount = 1;
    while (leafCount < aSouthSize) {
        leafCount <<= 1;
    }
    const int32_t firstLeaf = leafCount - 1;  // index of the leftmost leaf in the 0-based heap layout
    std::vector<uint64_t> tree(static_cast<size_t>(2 * leafCount - 1), 0U);

    // 3) sweep the south positions in north order. Each newly placed edge crosses every already
    //    placed edge whose south position is strictly greater : walking up from its leaf, whenever
    //    we come from a left child we add its right sibling's subtree count.
    uint64_t crossings = 0U;
    for (size_t edgeIndex = 0U; edgeIndex < sortedEdges.size(); ++edgeIndex) {
        int32_t nodeIndex = sortedEdges[edgeIndex].second + firstLeaf;
        ++tree[static_cast<size_t>(nodeIndex)];
        while (nodeIndex > 0) {
            if ((nodeIndex & 1) != 0) {  // left child : its right sibling holds the strictly greater positions
                crossings += tree[static_cast<size_t>(nodeIndex + 1)];
            }
            nodeIndex = (nodeIndex - 1) / 2;  // move to the parent
            ++tree[static_cast<size_t>(nodeIndex)];
        }
    }
    return crossings;
}

}  // namespace diagram
}  // namespace ez
