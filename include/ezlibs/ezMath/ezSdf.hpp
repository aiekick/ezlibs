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

// ezSdf is part of the ezLibs project : https://github.com/aiekick/ezLibs.git
// Signed distance fields over axis-aligned boxes. Relies on the ezMath umbrella
// for vec2<T> / AABB<T> (it is included by ezMath.hpp after them).

#include <vector>
#include <cstddef>
#include <limits>

namespace ez {
namespace math {

// Signed distance from a point to an axis-aligned box.
// Convention : < 0 inside, 0 on the border, > 0 outside (euclidean distance to the nearest edge).
template <typename T>
inline T sdfToBox(const vec2<T>& aPoint, const AABB<T>& aBox) {
    const vec2<T> center = aBox.GetCenter();
    const vec2<T> halfExtent = aBox.GetExtents();
    // fold the offset into the first quadrant (abs), then subtract the half extents.
    vec2<T> dist = aPoint - center;
    dist.x = (dist.x < static_cast<T>(0)) ? -dist.x : dist.x;
    dist.y = (dist.y < static_cast<T>(0)) ? -dist.y : dist.y;
    dist = dist - halfExtent;
    // outside contribution : euclidean length of the positive part (0 on an axis where we are inside).
    const vec2<T> positivePart = maxi(dist, vec2<T>(static_cast<T>(0), static_cast<T>(0)));
    const T outside = length(positivePart);
    // inside contribution : negative penetration depth, 0 when the point is outside.
    const T deepest = (dist.x > dist.y) ? dist.x : dist.y;
    const T inside = (deepest < static_cast<T>(0)) ? deepest : static_cast<T>(0);
    return outside + inside;
}

// Signed distance from a point to the UNION of boxes (min over all boxes).
// Exact in free space ; conservative approximation inside overlapping boxes.
// Empty set : returns the largest representable value (no obstacle anywhere).
template <typename T>
inline T sdfToBoxes(const vec2<T>& aPoint, const std::vector<AABB<T>>& aBoxes) {
    if (aBoxes.empty()) {
        return std::numeric_limits<T>::max();
    }
    T best = sdfToBox(aPoint, aBoxes[0]);
    for (size_t boxIndex = 1U; boxIndex < aBoxes.size(); ++boxIndex) {
        const T dist = sdfToBox(aPoint, aBoxes[boxIndex]);
        if (dist < best) {
            best = dist;
        }
    }
    return best;
}

namespace detail {
// Three-way sign : +1 / -1 / 0 (0 exactly at 0, so the gradient vanishes at a box center).
template <typename T>
inline T axisSign(T aValue) {
    if (aValue > static_cast<T>(0)) {
        return static_cast<T>(1);
    }
    if (aValue < static_cast<T>(0)) {
        return static_cast<T>(-1);
    }
    return static_cast<T>(0);
}
}  // namespace detail

// Gradient of the box SDF at a point : unit direction of INCREASING distance, i.e. pointing
// AWAY from the box (the escape direction). |grad| = 1 everywhere except at the exact center,
// where the point is equidistant to all faces and the gradient is (0,0) (undefined).
template <typename T>
inline vec2<T> sdfGradToBox(const vec2<T>& aPoint, const AABB<T>& aBox) {
    const vec2<T> center = aBox.GetCenter();
    const vec2<T> halfExtent = aBox.GetExtents();
    const vec2<T> offset = aPoint - center;
    const vec2<T> sideSign(detail::axisSign(offset.x), detail::axisSign(offset.y));
    const vec2<T> dist(
        ((offset.x < static_cast<T>(0)) ? -offset.x : offset.x) - halfExtent.x,
        ((offset.y < static_cast<T>(0)) ? -offset.y : offset.y) - halfExtent.y);
    if (dist.x > static_cast<T>(0) || dist.y > static_cast<T>(0)) {
        // outside : away from the nearest face (one axis) or corner (both axes), re-signed per quadrant.
        const vec2<T> positivePart = maxi(dist, vec2<T>(static_cast<T>(0), static_cast<T>(0)));
        const T len = length(positivePart);  // > 0 here : at least one positive component
        return vec2<T>(positivePart.x / len * sideSign.x, positivePart.y / len * sideSign.y);
    }
    // inside : push out along the nearest face (the axis with the least-negative penetration).
    if (dist.x > dist.y) {
        return vec2<T>(sideSign.x, static_cast<T>(0));
    }
    return vec2<T>(static_cast<T>(0), sideSign.y);
}

// Gradient of the union SDF : the gradient of the nearest box (the argmin of sdfToBox).
// The gradient of a min is the gradient of the winning term. Empty set : (0,0) (no obstacle).
template <typename T>
inline vec2<T> sdfGradToBoxes(const vec2<T>& aPoint, const std::vector<AABB<T>>& aBoxes) {
    if (aBoxes.empty()) {
        return vec2<T>(static_cast<T>(0), static_cast<T>(0));
    }
    size_t nearestIndex = 0U;
    T best = sdfToBox(aPoint, aBoxes[0]);
    for (size_t boxIndex = 1U; boxIndex < aBoxes.size(); ++boxIndex) {
        const T dist = sdfToBox(aPoint, aBoxes[boxIndex]);
        if (dist < best) {
            best = dist;
            nearestIndex = boxIndex;
        }
    }
    return sdfGradToBox(aPoint, aBoxes[nearestIndex]);
}

// Does the segment [aStart, aEnd] overlap the box ? (slab method ; endpoints inside count as a hit).
// Used to CLASSIFY a link : a straight link whose body crosses a box should be rerouted, a clear
// one can stay a spline. The caller is responsible for excluding the boxes of the link endpoints.
template <typename T>
inline bool segmentHitsBox(const vec2<T>& aStart, const vec2<T>& aEnd, const AABB<T>& aBox) {
    const vec2<T> dir = aEnd - aStart;
    T tMin = static_cast<T>(0);
    T tMax = static_cast<T>(1);
    // x slab
    if (((dir.x < static_cast<T>(0)) ? -dir.x : dir.x) < static_cast<T>(1e-8)) {
        if (aStart.x < aBox.lowerBound.x || aStart.x > aBox.upperBound.x) {
            return false;  // parallel to the x slab and outside it
        }
    } else {
        T tEnter = (aBox.lowerBound.x - aStart.x) / dir.x;
        T tLeave = (aBox.upperBound.x - aStart.x) / dir.x;
        if (tEnter > tLeave) {
            const T swap = tEnter;
            tEnter = tLeave;
            tLeave = swap;
        }
        if (tEnter > tMin) {
            tMin = tEnter;
        }
        if (tLeave < tMax) {
            tMax = tLeave;
        }
        if (tMin > tMax) {
            return false;
        }
    }
    // y slab
    if (((dir.y < static_cast<T>(0)) ? -dir.y : dir.y) < static_cast<T>(1e-8)) {
        if (aStart.y < aBox.lowerBound.y || aStart.y > aBox.upperBound.y) {
            return false;  // parallel to the y slab and outside it
        }
    } else {
        T tEnter = (aBox.lowerBound.y - aStart.y) / dir.y;
        T tLeave = (aBox.upperBound.y - aStart.y) / dir.y;
        if (tEnter > tLeave) {
            const T swap = tEnter;
            tEnter = tLeave;
            tLeave = swap;
        }
        if (tEnter > tMin) {
            tMin = tEnter;
        }
        if (tLeave < tMax) {
            tMax = tLeave;
        }
        if (tMin > tMax) {
            return false;
        }
    }
    return true;
}

// True if the segment overlaps ANY box of the set.
template <typename T>
inline bool segmentHitsBoxes(const vec2<T>& aStart, const vec2<T>& aEnd, const std::vector<AABB<T>>& aBoxes) {
    for (size_t boxIndex = 0U; boxIndex < aBoxes.size(); ++boxIndex) {
        if (segmentHitsBox(aStart, aEnd, aBoxes[boxIndex])) {
            return true;
        }
    }
    return false;
}

}  // namespace math
}  // namespace ez
