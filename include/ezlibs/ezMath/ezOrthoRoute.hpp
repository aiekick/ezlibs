#pragma once

/*
MIT License

Copyright (c) 2014-2024 Stephane Cuillerdier (aka aiekick)

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

// ezOrthoRoute is part of the ezLibs project : https://github.com/aiekick/ezLibs.git
// Orthogonal, obstacle-avoiding routing over a box field. The route runs in the CHANNELS between
// the boxes : the routing lattice is the orthogonal visibility graph built from each box edge
// offset by `clearance` (plus the two endpoints), so routes hug the node borders at a fixed
// distance instead of cutting across them. Shortest orthogonal path = Dijkstra with a turn
// penalty. Relies on the ezMath umbrella for vec2<T>/AABB<T> and on ezSdf.hpp (sdfToBoxes +
// segmentHitsBoxes) ; it is included by ezMath.hpp AFTER ezSdf.hpp.

#include <vector>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <queue>
#include <utility>
#include <algorithm>
#include <functional>

namespace ez {
namespace math {

template <typename T>
struct OrthoRouteConfig {
    T clearance{static_cast<T>(8)};     // distance kept from every box edge (the hug distance / channel half-width)
    T turnPenalty{static_cast<T>(4)};   // small cost per 90 degrees bend : breaks staircase ties but lets routes hug
    int32_t maxLines{256};              // safety cap on lattice lines per axis ; over it, fall back to a straight link
};

namespace detail {

template <typename T>
inline T routeAbsScalar(T aValue) {
    return (aValue < static_cast<T>(0)) ? -aValue : aValue;
}

// sort + drop near-duplicates (within aMergeEps) in place.
template <typename T>
inline void routeSortUnique(std::vector<T>& aoValues, T aMergeEps) {
    std::sort(aoValues.begin(), aoValues.end());
    std::vector<T> merged;
    merged.reserve(aoValues.size());
    for (size_t valueIndex = 0U; valueIndex < aoValues.size(); ++valueIndex) {
        if (merged.empty() || (aoValues[valueIndex] - merged[merged.size() - 1U]) > aMergeEps) {
            merged.push_back(aoValues[valueIndex]);
        }
    }
    aoValues.swap(merged);
}

// index of the lattice line nearest to aTarget (linear scan ; lattices are small).
template <typename T>
inline int32_t routeNearestLine(const std::vector<T>& aLines, T aTarget) {
    int32_t best = 0;
    T bestDist = routeAbsScalar(aLines[0] - aTarget);
    for (size_t lineIndex = 1U; lineIndex < aLines.size(); ++lineIndex) {
        const T dist = routeAbsScalar(aLines[lineIndex] - aTarget);
        if (dist < bestDist) {
            bestDist = dist;
            best = static_cast<int32_t>(lineIndex);
        }
    }
    return best;
}

// append aPoint to aoPath, dropping near-duplicates and collinear vertices.
template <typename T>
inline void routePushSimplified(std::vector<vec2<T>>& aoPath, const vec2<T>& aPoint, T aEps) {
    const size_t count = aoPath.size();
    if (count >= 1U) {
        const vec2<T>& last = aoPath[count - 1U];
        if (routeAbsScalar(last.x - aPoint.x) < aEps && routeAbsScalar(last.y - aPoint.y) < aEps) {
            return;  // duplicate
        }
    }
    if (count >= 2U) {
        const vec2<T>& prev = aoPath[count - 2U];
        const vec2<T>& last = aoPath[count - 1U];
        const bool sameX = routeAbsScalar(prev.x - last.x) < aEps && routeAbsScalar(last.x - aPoint.x) < aEps;
        const bool sameY = routeAbsScalar(prev.y - last.y) < aEps && routeAbsScalar(last.y - aPoint.y) < aEps;
        if (sameX || sameY) {
            aoPath[count - 1U] = aPoint;  // collinear : extend the run instead of adding a vertex
            return;
        }
    }
    aoPath.push_back(aPoint);
}

// Orthogonal L between two points (horizontal leg first). Used as the routing fallback so a failed
// route is still axis-aligned (vertical/horizontal only), never a diagonal.
template <typename T>
inline std::vector<vec2<T>> routeElbow(const vec2<T>& aStart, const vec2<T>& aEnd) {
    std::vector<vec2<T>> elbow;
    elbow.push_back(aStart);
    if (routeAbsScalar(aEnd.x - aStart.x) > static_cast<T>(1e-6) && routeAbsScalar(aEnd.y - aStart.y) > static_cast<T>(1e-6)) {
        elbow.push_back(vec2<T>(aEnd.x, aStart.y));
    }
    elbow.push_back(aEnd);
    return elbow;
}

}  // namespace detail

// Route an orthogonal polyline from aStart to aEnd through the channels between the boxes.
// The result starts at aStart, ends at aEnd, runs along lines offset by config.clearance from the
// box edges, and stays clear of every box. Falls back to the straight [aStart, aEnd] when no path
// exists or the lattice would be too large.
template <typename T>
inline std::vector<vec2<T>> orthoRoute(const std::vector<AABB<T>>& aBoxes, const vec2<T>& aStart, const vec2<T>& aEnd, const OrthoRouteConfig<T>& aConfig) {
    std::vector<vec2<T>> result;
    if (detail::routeAbsScalar(aStart.x - aEnd.x) < static_cast<T>(1e-6) && detail::routeAbsScalar(aStart.y - aEnd.y) < static_cast<T>(1e-6)) {
        result.push_back(aStart);
        return result;
    }
    T clearance = aConfig.clearance;
    if (clearance < static_cast<T>(0)) {
        clearance = static_cast<T>(0);
    }
    const T eps = (clearance > static_cast<T>(0)) ? clearance * static_cast<T>(0.05) : static_cast<T>(0.5);
    const T safe = clearance - eps;  // effective min distance (lets a channel sitting exactly at clearance pass)

    // 1) candidate lattice lines : the two endpoints + every box edge offset by clearance.
    std::vector<T> xs;
    std::vector<T> ys;
    xs.push_back(aStart.x);
    xs.push_back(aEnd.x);
    ys.push_back(aStart.y);
    ys.push_back(aEnd.y);
    for (size_t boxIndex = 0U; boxIndex < aBoxes.size(); ++boxIndex) {
        const AABB<T>& box = aBoxes[boxIndex];
        xs.push_back(box.lowerBound.x - clearance);
        xs.push_back(box.upperBound.x + clearance);
        ys.push_back(box.lowerBound.y - clearance);
        ys.push_back(box.upperBound.y + clearance);
    }
    detail::routeSortUnique(xs, eps);
    detail::routeSortUnique(ys, eps);
    const int32_t width = static_cast<int32_t>(xs.size());
    const int32_t height = static_cast<int32_t>(ys.size());
    if (width < 1 || height < 1 || width > aConfig.maxLines || height > aConfig.maxLines) {
        return detail::routeElbow(aStart, aEnd);
    }

    // boxes grown by `safe`, used to reject a lattice segment that would enter a box's clearance band.
    std::vector<AABB<T>> grown;
    grown.reserve(aBoxes.size());
    for (size_t boxIndex = 0U; boxIndex < aBoxes.size(); ++boxIndex) {
        const AABB<T>& box = aBoxes[boxIndex];
        grown.push_back(AABB<T>(vec2<T>(box.lowerBound.x - safe, box.lowerBound.y - safe), vec2<T>(box.upperBound.x + safe, box.upperBound.y + safe)));
    }

    // 2) node validity : a lattice point is usable if it sits at least `safe` from every box.
    const int32_t cellCount = width * height;
    std::vector<char> valid(static_cast<size_t>(cellCount), 0);
    for (int32_t iy = 0; iy < height; ++iy) {
        for (int32_t ix = 0; ix < width; ++ix) {
            if (sdfToBoxes(vec2<T>(xs[ix], ys[iy]), aBoxes) >= safe) {
                valid[static_cast<size_t>(iy * width + ix)] = 1;
            }
        }
    }
    const int32_t startIx = detail::routeNearestLine(xs, aStart.x);
    const int32_t startIy = detail::routeNearestLine(ys, aStart.y);
    const int32_t endIx = detail::routeNearestLine(xs, aEnd.x);
    const int32_t endIy = detail::routeNearestLine(ys, aEnd.y);
    valid[static_cast<size_t>(startIy * width + startIx)] = 1;  // endpoints are slots : always usable
    valid[static_cast<size_t>(endIy * width + endIx)] = 1;

    // 3) precompute the channel edges (right / down) between adjacent valid lattice points.
    std::vector<char> edgeRight(static_cast<size_t>(cellCount), 0);
    std::vector<char> edgeDown(static_cast<size_t>(cellCount), 0);
    for (int32_t iy = 0; iy < height; ++iy) {
        for (int32_t ix = 0; ix < width; ++ix) {
            const int32_t node = iy * width + ix;
            if (valid[static_cast<size_t>(node)] == 0) {
                continue;
            }
            if (ix + 1 < width && valid[static_cast<size_t>(node + 1)] != 0) {
                if (!segmentHitsBoxes(vec2<T>(xs[ix], ys[iy]), vec2<T>(xs[ix + 1], ys[iy]), grown)) {
                    edgeRight[static_cast<size_t>(node)] = 1;
                }
            }
            if (iy + 1 < height && valid[static_cast<size_t>(node + width)] != 0) {
                if (!segmentHitsBoxes(vec2<T>(xs[ix], ys[iy]), vec2<T>(xs[ix], ys[iy + 1]), grown)) {
                    edgeDown[static_cast<size_t>(node)] = 1;
                }
            }
        }
    }

    // 4) Dijkstra. State = node * 3 + arrivalAxis (0 horizontal, 1 vertical, 2 none) so a change of
    //    axis can be charged config.turnPenalty.
    const int32_t kNone = 2;
    const T kInf = std::numeric_limits<T>::max() / static_cast<T>(4);
    std::vector<T> dist(static_cast<size_t>(cellCount) * 3U, kInf);
    std::vector<int32_t> cameFrom(static_cast<size_t>(cellCount) * 3U, -1);
    std::vector<char> closed(static_cast<size_t>(cellCount) * 3U, 0);
    const int32_t startNode = startIy * width + startIx;
    const int32_t endNode = endIy * width + endIx;
    typedef std::pair<T, int32_t> QueueNode;  // (distance, state)
    std::priority_queue<QueueNode, std::vector<QueueNode>, std::greater<QueueNode> > open;
    const int32_t startState = startNode * 3 + kNone;
    dist[static_cast<size_t>(startState)] = static_cast<T>(0);
    open.push(QueueNode(static_cast<T>(0), startState));
    int32_t goalState = -1;
    while (!open.empty()) {
        const QueueNode topNode = open.top();
        open.pop();
        const int32_t state = topNode.second;
        if (closed[static_cast<size_t>(state)] != 0) {
            continue;
        }
        closed[static_cast<size_t>(state)] = 1;
        const int32_t node = state / 3;
        if (node == endNode) {
            goalState = state;
            break;
        }
        const int32_t arrivalAxis = state % 3;
        const int32_t ix = node % width;
        const int32_t iy = node / width;
        // 4 neighbours : right / left (axis 0), down / up (axis 1), gated by the precomputed edges.
        for (int32_t dir = 0; dir < 4; ++dir) {
            int32_t nNode = -1;
            int32_t axis = 0;
            if (dir == 0 && ix + 1 < width && edgeRight[static_cast<size_t>(node)] != 0) {
                nNode = node + 1;
                axis = 0;
            } else if (dir == 1 && ix - 1 >= 0 && edgeRight[static_cast<size_t>(node - 1)] != 0) {
                nNode = node - 1;
                axis = 0;
            } else if (dir == 2 && iy + 1 < height && edgeDown[static_cast<size_t>(node)] != 0) {
                nNode = node + width;
                axis = 1;
            } else if (dir == 3 && iy - 1 >= 0 && edgeDown[static_cast<size_t>(node - width)] != 0) {
                nNode = node - width;
                axis = 1;
            }
            if (nNode < 0) {
                continue;
            }
            const int32_t nix = nNode % width;
            const int32_t niy = nNode / width;
            const T moveCost = detail::routeAbsScalar(xs[nix] - xs[ix]) + detail::routeAbsScalar(ys[niy] - ys[iy]);
            const T turnCost = (arrivalAxis != kNone && axis != arrivalAxis) ? aConfig.turnPenalty : static_cast<T>(0);
            const int32_t nState = nNode * 3 + axis;
            const T tentative = dist[static_cast<size_t>(state)] + moveCost + turnCost;
            if (tentative < dist[static_cast<size_t>(nState)]) {
                dist[static_cast<size_t>(nState)] = tentative;
                cameFrom[static_cast<size_t>(nState)] = state;
                open.push(QueueNode(tentative, nState));
            }
        }
    }
    if (goalState < 0) {
        return detail::routeElbow(aStart, aEnd);
    }

    // 5) reconstruct (goal -> start), reverse, stitch the exact endpoints, simplify.
    std::vector<vec2<T>> lattice;
    for (int32_t state = goalState; state >= 0; state = cameFrom[static_cast<size_t>(state)]) {
        const int32_t node = state / 3;
        lattice.push_back(vec2<T>(xs[node % width], ys[node / width]));
    }
    std::reverse(lattice.begin(), lattice.end());
    detail::routePushSimplified(result, aStart, eps);
    for (size_t pointIndex = 0U; pointIndex < lattice.size(); ++pointIndex) {
        detail::routePushSimplified(result, lattice[pointIndex], eps);
    }
    detail::routePushSimplified(result, aEnd, eps);
    if (result.size() < 2U) {
        return detail::routeElbow(aStart, aEnd);
    }
    return result;
}

namespace detail {

// one route segment lying on a lattice line, tagged for lane ordering.
template <typename T>
struct RouteLineUse {
    T line{static_cast<T>(0)};
    T orderKey{static_cast<T>(0)};
    size_t routeIndex{0};
    bool operator<(const RouteLineUse<T>& aOther) const {
        if (line < aOther.line) { return true; }
        if (aOther.line < line) { return false; }
        if (orderKey < aOther.orderKey) { return true; }
        if (aOther.orderKey < orderKey) { return false; }
        return routeIndex < aOther.routeIndex;
    }
};

// offset stored for a route on a given line, looked up by the line coordinate.
template <typename T>
inline T routeLookupOffset(const std::vector<std::pair<T, T>>& aOffsets, T aLine, T aEps) {
    for (size_t offsetIndex = 0U; offsetIndex < aOffsets.size(); ++offsetIndex) {
        if (routeAbsScalar(aOffsets[offsetIndex].first - aLine) < aEps) {
            return aOffsets[offsetIndex].second;
        }
    }
    return static_cast<T>(0);
}

// group the uses by coincident line, order each group by orderKey, emit a centered perpendicular
// offset per (route, line). Lines used by a single route are left at offset 0 (nothing to separate).
template <typename T>
inline void routeAssignLanes(std::vector<RouteLineUse<T>>& aoUses, T aLaneGap, T aEps, std::vector<std::vector<std::pair<T, T>>>& aoPerRouteOffsets) {
    std::sort(aoUses.begin(), aoUses.end());
    size_t groupStart = 0U;
    while (groupStart < aoUses.size()) {
        size_t groupEnd = groupStart;
        while (groupEnd < aoUses.size() && (aoUses[groupEnd].line - aoUses[groupStart].line) < aEps) {
            ++groupEnd;
        }
        std::vector<size_t> routesInOrder;
        for (size_t useIndex = groupStart; useIndex < groupEnd; ++useIndex) {
            bool already = false;
            for (size_t orderIndex = 0U; orderIndex < routesInOrder.size(); ++orderIndex) {
                if (routesInOrder[orderIndex] == aoUses[useIndex].routeIndex) {
                    already = true;
                    break;
                }
            }
            if (!already) {
                routesInOrder.push_back(aoUses[useIndex].routeIndex);
            }
        }
        const size_t laneCount = routesInOrder.size();
        if (laneCount > 1U) {
            for (size_t lane = 0U; lane < laneCount; ++lane) {
                const T offset = (static_cast<T>(lane) - static_cast<T>(laneCount - 1U) / static_cast<T>(2)) * aLaneGap;
                aoPerRouteOffsets[routesInOrder[lane]].push_back(std::make_pair(aoUses[groupStart].line, offset));
            }
        }
        groupStart = groupEnd;
    }
}

}  // namespace detail

// Separate links that share a channel into parallel lanes. Every group of route segments lying on
// the same lattice line (same x for a vertical run, same y for a horizontal run) is spread by
// aLaneGap, centered and ordered by source-slot Y. The per-line offset keeps corners connected and
// segments orthogonal ; the first and last point of each route (the slots) are left untouched.
template <typename T>
inline void bundleRoutes(std::vector<std::vector<vec2<T>>>& aoRoutes, T aLaneGap) {
    if (aLaneGap <= static_cast<T>(0)) {
        return;
    }
    const T eps = aLaneGap * static_cast<T>(0.25);
    // 1) simplify each route so its segments strictly alternate horizontal/vertical (merge collinear).
    for (size_t routeIndex = 0U; routeIndex < aoRoutes.size(); ++routeIndex) {
        std::vector<vec2<T>> simplified;
        for (size_t pointIndex = 0U; pointIndex < aoRoutes[routeIndex].size(); ++pointIndex) {
            detail::routePushSimplified(simplified, aoRoutes[routeIndex][pointIndex], eps);
        }
        aoRoutes[routeIndex].swap(simplified);
    }
    // 2) collect the interior trunk segments (skip the first and last segment : the slot legs, kept fixed).
    std::vector<detail::RouteLineUse<T>> verticalUses;
    std::vector<detail::RouteLineUse<T>> horizontalUses;
    for (size_t routeIndex = 0U; routeIndex < aoRoutes.size(); ++routeIndex) {
        const std::vector<vec2<T>>& route = aoRoutes[routeIndex];
        if (route.size() < 4U) {
            continue;
        }
        const T orderKey = route[0].y;  // stable lane order : the source-slot Y
        const size_t segCount = route.size() - 1U;
        for (size_t seg = 1U; seg + 1U < segCount; ++seg) {
            const T dx = detail::routeAbsScalar(route[seg + 1U].x - route[seg].x);
            const T dy = detail::routeAbsScalar(route[seg + 1U].y - route[seg].y);
            detail::RouteLineUse<T> use;
            use.orderKey = orderKey;
            use.routeIndex = routeIndex;
            if (dy < dx) {  // horizontal segment
                use.line = route[seg].y;
                horizontalUses.push_back(use);
            } else {  // vertical segment
                use.line = route[seg].x;
                verticalUses.push_back(use);
            }
        }
    }
    std::vector<std::vector<std::pair<T, T>>> vertOffset(aoRoutes.size());
    std::vector<std::vector<std::pair<T, T>>> horizOffset(aoRoutes.size());
    detail::routeAssignLanes(verticalUses, aLaneGap, eps, vertOffset);
    detail::routeAssignLanes(horizontalUses, aLaneGap, eps, horizOffset);
    // 3) rebuild each route from its (offset) segment lines : every interior vertex is the intersection
    //    of its two perpendicular segments -> the result is ALWAYS orthogonal, no diagonal possible.
    for (size_t routeIndex = 0U; routeIndex < aoRoutes.size(); ++routeIndex) {
        std::vector<vec2<T>>& route = aoRoutes[routeIndex];
        if (route.size() < 4U) {
            continue;
        }
        const size_t pointCount = route.size();
        const size_t segCount = pointCount - 1U;
        std::vector<T> segLine(segCount);
        std::vector<char> segHorizontal(segCount);
        for (size_t seg = 0U; seg < segCount; ++seg) {
            const T dx = detail::routeAbsScalar(route[seg + 1U].x - route[seg].x);
            const T dy = detail::routeAbsScalar(route[seg + 1U].y - route[seg].y);
            const bool horizontal = (dy < dx);
            segHorizontal[seg] = horizontal ? static_cast<char>(1) : static_cast<char>(0);
            T line = horizontal ? route[seg].y : route[seg].x;
            if (seg >= 1U && seg + 1U < segCount) {  // interior trunk segment : apply its lane offset
                line += horizontal ? detail::routeLookupOffset(horizOffset[routeIndex], route[seg].y, eps)
                                   : detail::routeLookupOffset(vertOffset[routeIndex], route[seg].x, eps);
            }
            segLine[seg] = line;
        }
        std::vector<vec2<T>> rebuilt(pointCount);
        rebuilt[0] = route[0];
        rebuilt[pointCount - 1U] = route[pointCount - 1U];
        for (size_t pointIndex = 1U; pointIndex + 1U < pointCount; ++pointIndex) {
            const size_t segPrev = pointIndex - 1U;
            const size_t segNext = pointIndex;
            if (segHorizontal[segPrev] == segHorizontal[segNext]) {
                rebuilt[pointIndex] = route[pointIndex];  // non-alternating safety : keep the original vertex
            } else if (segHorizontal[segPrev] != 0) {     // prev horizontal, next vertical
                rebuilt[pointIndex] = vec2<T>(segLine[segNext], segLine[segPrev]);
            } else {                                       // prev vertical, next horizontal
                rebuilt[pointIndex] = vec2<T>(segLine[segPrev], segLine[segNext]);
            }
        }
        route.swap(rebuilt);
    }
}

}  // namespace math
}  // namespace ez
