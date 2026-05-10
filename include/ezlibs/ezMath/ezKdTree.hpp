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

// ezKdTree is part of the ezLibs project : https://github.com/aiekick/ezLibs.git
//
// Generic k-d tree for k-nearest-neighbor search in low-dimensional float
// vector spaces (typical use: matching local image descriptors).
//
// Online insertion: leaves grow up to k_leafCapacity points, then split on
// the axis with the largest empirical variance (cut at the midpoint of the
// observed range on that axis).
//
// Approximate Best-Bin-First search (Beis & Lowe 1997): DFS that always
// descends the closer subtree first and prunes the farther subtree when its
// hyperplane distance exceeds the current best squared distance, with a
// per-query budget on the number of leaves visited (aMaxTry).
//
// References:
//   - Bentley, "Multidimensional binary search trees used for associative
//     searching", Communications of the ACM, 1975.
//   - Friedman, Bentley & Finkel, "An algorithm for finding best matches in
//     logarithmic expected time", ACM TOMS, 1977.
//   - Beis & Lowe, "Shape indexing using approximate nearest-neighbour
//     search in high-dimensional spaces", CVPR 1997.

#include <algorithm>
#include <cstddef>
#include <memory>
#include <utility>
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
namespace math {

template <typename U>
class kdTree {
public:
    // Maximum number of (point, value) pairs stored in a leaf before it splits.
    static constexpr std::size_t k_leafCapacity = 64;

    // Result of a k-NN query: associated value + squared distance to the
    // query point. Squared (not sqrt) to keep comparisons exact and faster.
    struct hit {
        U value;
        float squaredDistance;
    };

private:
    struct entry {
        std::vector<float> point;
        U value;
    };

    struct node {
        // Leaf data (only meaningful when isLeaf == true).
        std::vector<entry> entries;
        // Internal node data (only meaningful when isLeaf == false).
        std::size_t pivotAxis{0};
        float pivotValue{0.0f};
        std::unique_ptr<node> left{};
        std::unique_ptr<node> right{};
        bool isLeaf{true};
    };

    std::size_t m_dimension{0};
    std::size_t m_size{0};
    std::size_t m_depth{0};
    std::unique_ptr<node> m_root{};

public:
    explicit kdTree(std::size_t aDimension)
        : m_dimension(aDimension)
        // std::make_unique is C++14; ezlibs targets C++11 strict, hence the
        // explicit std::unique_ptr<node>(new node()) form.
        , m_root(std::unique_ptr<node>(new node())) {
    }

    std::size_t size() const {
        return m_size;
    }
    std::size_t depth() const {
        return m_depth;
    }
    std::size_t dimension() const {
        return m_dimension;
    }
    bool empty() const {
        return m_size == 0;
    }

    // Insert (aPoint, aValue) into the tree. Silently rejected if the point
    // size does not match the configured dimension.
    void put(std::vector<float> aPoint, U aValue) {
        if (aPoint.size() != m_dimension) {
            return;
        }
        insertRecursive(*m_root, std::move(aPoint), std::move(aValue), 0);
        ++m_size;
    }

    // Find up to aMaxElements nearest neighbors of aQuery, with squared
    // distance not exceeding aMaxSquaredDistance. The search is approximate:
    // visits at most aMaxTry leaves (Best-Bin-First budget).
    //
    // Output (aoResults) is sorted by ascending squared distance.
    // Existing content is cleared.
    void nearest(
        const std::vector<float>& aQuery,
        float aMaxSquaredDistance,
        std::size_t aMaxTry,
        std::size_t aMaxElements,
        std::vector<hit>& aoResults) const {
        aoResults.clear();
        if (aQuery.size() != m_dimension || aMaxElements == 0 || m_size == 0) {
            return;
        }
        std::size_t triedLeaves = 0;
        searchRecursive(*m_root, aQuery, aMaxSquaredDistance, aMaxTry, aMaxElements, aoResults, triedLeaves);
        // Heap order is reverse: largest distance at the front. Sort ascending.
        std::sort(aoResults.begin(), aoResults.end(), [](const hit& a, const hit& b) {
            return a.squaredDistance < b.squaredDistance;
        });
    }

private:
    void insertRecursive(node& aoNode, std::vector<float> aPoint, U aValue, std::size_t aDepth) {
        if (aoNode.isLeaf) {
            aoNode.entries.push_back(entry{std::move(aPoint), std::move(aValue)});
            if (aDepth + 1 > m_depth) {
                m_depth = aDepth + 1;
            }
            if (aoNode.entries.size() > k_leafCapacity) {
                splitLeaf(aoNode, aDepth);
            }
            return;
        }
        if (aPoint[aoNode.pivotAxis] < aoNode.pivotValue) {
            insertRecursive(*aoNode.left, std::move(aPoint), std::move(aValue), aDepth + 1);
        } else {
            insertRecursive(*aoNode.right, std::move(aPoint), std::move(aValue), aDepth + 1);
        }
    }

    // Promote a leaf to an internal node by choosing the axis with the
    // largest empirical variance and cutting at the midpoint of its range.
    void splitLeaf(node& aoNode, std::size_t aDepth) {
        const std::size_t entryCount = aoNode.entries.size();

        // Pick the axis with maximum variance via running sums:
        //   var(j) = (Σx²)/n − (Σx/n)² = (n·Σx² − (Σx)²) / n²
        std::size_t bestAxis = 0;
        float bestVariance = -1.0f;
        for (std::size_t axis = 0; axis < m_dimension; ++axis) {
            float sum = 0.0f;
            float sumOfSquares = 0.0f;
            for (const auto& current : aoNode.entries) {
                const float v = current.point[axis];
                sum += v;
                sumOfSquares += v * v;
            }
            const float mean = sum / static_cast<float>(entryCount);
            const float variance = sumOfSquares / static_cast<float>(entryCount) - mean * mean;
            if (variance > bestVariance) {
                bestVariance = variance;
                bestAxis = axis;
            }
        }

        // Cut at the midpoint of the observed range on the chosen axis.
        float minValue = aoNode.entries[0].point[bestAxis];
        float maxValue = minValue;
        for (const auto& current : aoNode.entries) {
            const float v = current.point[bestAxis];
            if (v < minValue) {
                minValue = v;
            }
            if (v > maxValue) {
                maxValue = v;
            }
        }
        const float pivot = (minValue + maxValue) * 0.5f;

        // std::make_unique not available under C++11 (added in C++14).
        std::unique_ptr<node> leftChild(new node());
        std::unique_ptr<node> rightChild(new node());
        for (auto& current : aoNode.entries) {
            if (current.point[bestAxis] < pivot) {
                leftChild->entries.push_back(std::move(current));
            } else {
                rightChild->entries.push_back(std::move(current));
            }
        }

        // Pathological case: all points landed on the same side. Fall back
        // to keeping the leaf as-is to avoid an infinite recursion.
        if (leftChild->entries.empty() || rightChild->entries.empty()) {
            return;
        }

        aoNode.isLeaf = false;
        aoNode.pivotAxis = bestAxis;
        aoNode.pivotValue = pivot;
        aoNode.entries.clear();
        aoNode.entries.shrink_to_fit();
        aoNode.left = std::move(leftChild);
        aoNode.right = std::move(rightChild);

        const std::size_t leftCount = aoNode.left->entries.size();
        const std::size_t rightCount = aoNode.right->entries.size();
        if (leftCount > k_leafCapacity) {
            splitLeaf(*aoNode.left, aDepth + 1);
        }
        if (rightCount > k_leafCapacity) {
            splitLeaf(*aoNode.right, aDepth + 1);
        }

        if (aDepth + 2 > m_depth) {
            m_depth = aDepth + 2;
        }
    }

    void searchRecursive(
        const node& aNode,
        const std::vector<float>& aQuery,
        float aMaxSquaredDistance,
        std::size_t aMaxTry,
        std::size_t aMaxElements,
        std::vector<hit>& aoResults,
        std::size_t& aoTriedLeaves) const {
        if (aoTriedLeaves >= aMaxTry) {
            return;
        }
        if (aNode.isLeaf) {
            ++aoTriedLeaves;
            for (const auto& current : aNode.entries) {
                const float squaredDistance = squaredDistanceL2(aQuery, current.point);
                if (squaredDistance <= aMaxSquaredDistance) {
                    insertHit(aoResults, hit{current.value, squaredDistance}, aMaxElements);
                }
            }
            return;
        }
        const float diff = aQuery[aNode.pivotAxis] - aNode.pivotValue;
        const node& nearChild = (diff < 0.0f) ? *aNode.left : *aNode.right;
        const node& farChild  = (diff < 0.0f) ? *aNode.right : *aNode.left;

        searchRecursive(nearChild, aQuery, aMaxSquaredDistance, aMaxTry, aMaxElements, aoResults, aoTriedLeaves);

        // Best-Bin-First pruning: only descend the far side if its
        // hyperplane distance is below the current k-th best squared
        // distance (or below the user-set ceiling if we have fewer hits).
        const float currentBound = (aoResults.size() < aMaxElements)
                                       ? aMaxSquaredDistance
                                       : aoResults.front().squaredDistance;
        if (diff * diff < currentBound) {
            searchRecursive(farChild, aQuery, aMaxSquaredDistance, aMaxTry, aMaxElements, aoResults, aoTriedLeaves);
        }
    }

    static float squaredDistanceL2(const std::vector<float>& aLeft, const std::vector<float>& aRight) {
        float total = 0.0f;
        const std::size_t n = aLeft.size();
        for (std::size_t i = 0; i < n; ++i) {
            const float delta = aLeft[i] - aRight[i];
            total += delta * delta;
        }
        return total;
    }

    // Maintain aoResults as a max-heap of size <= aMaxElements. The largest
    // squared distance sits at the front and is the first to be evicted.
    static void insertHit(std::vector<hit>& aoResults, hit aCandidate, std::size_t aMaxElements) {
        const auto compare = [](const hit& a, const hit& b) {
            return a.squaredDistance < b.squaredDistance;
        };
        if (aoResults.size() < aMaxElements) {
            aoResults.push_back(aCandidate);
            std::push_heap(aoResults.begin(), aoResults.end(), compare);
            return;
        }
        if (aCandidate.squaredDistance < aoResults.front().squaredDistance) {
            std::pop_heap(aoResults.begin(), aoResults.end(), compare);
            aoResults.back() = aCandidate;
            std::push_heap(aoResults.begin(), aoResults.end(), compare);
        }
    }
};

}  // namespace math
}  // namespace ez

#ifdef _MSC_VER
#pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
