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

// ez::vision::epipolar utilities are part of the ezLibs project : https://github.com/aiekick/ezLibs.git
//
// Two-view geometry utilities for Structure-from-Motion: decomposition of an
// essential matrix into the four (R, t) candidates, linear DLT triangulation
// of a 3D point from two camera projections, and cheirality-based selection
// of the unique physically valid (R, t) pose.
//
// All algorithms are clean-room implementations from the public references
// below. They make no assumption on calibration: the caller is responsible
// for normalizing pixel coordinates with K^-1 prior to using the essential
// matrix decomposition or the cheirality check (otherwise the depth signs
// have no metric meaning).
//
// References:
//   - Hartley & Zisserman, "Multiple View Geometry in Computer Vision"
//     2nd ed., 2003. Chapter 9 (essential matrix decomposition, Result 9.19)
//     and chapter 12 (linear DLT triangulation, section 12.2).
//   - Longuet-Higgins, "A computer algorithm for reconstructing a scene from
//     two projections", Nature 293, 1981 (the original 8-point algorithm).
//
// Convention:
//   - Camera 1 is the canonical view: P1 = [I | 0].
//   - Camera 2 is at pose (R, t), so P2 = [R | t]. This means a 3D point X
//     expressed in camera 1's coordinate frame is mapped to camera 2's
//     coordinate frame by X2 = R * X + t. The camera *center* of camera 2
//     in camera 1's frame is C2 = -R^T * t.
//   - "Depth" of a 3D point in a camera frame = its Z component in that
//     frame. A point is "in front of" the camera iff its depth is > 0.

#include "../ezMath/ezMatN.hpp"
#include "../ezMath/ezVecN.hpp"
#include "../ezMath/ezSvd.hpp"

#include <array>
#include <vector>
#include <cstddef>
#include <cmath>
#include <limits>
#include <type_traits>

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
namespace vision {

// Camera pose: the 3x3 rotation matrix R and the 3-vector translation t
// expressed in the first camera's frame. A 3D point X (in camera-1 frame)
// is mapped to camera-2 frame via X2 = R * X + t.
template <typename T>
struct pose {
    math::matN<T> rotation{};      // 3x3, must satisfy R^T * R = I and det(R) = +1
    math::vecN<T> translation{};   // 3 elements
};

// One normalized point correspondence between two views. "Normalized" means
// K^-1 has been applied to the pixel coordinates so each (x, y) lies on the
// canonical image plane at z = 1. Required for the cheirality check to be
// meaningful.
template <typename T>
struct correspondence {
    T x1{0};
    T y1{0};
    T x2{0};
    T y2{0};
};

namespace detail {

// Determinant of a 3x3 matrix stored in math::matN (column-major access via
// operator()).
template <typename T>
inline T det3x3(const math::matN<T>& aMatrix) {
    return aMatrix(0, 0) * (aMatrix(1, 1) * aMatrix(2, 2) - aMatrix(1, 2) * aMatrix(2, 1))
         - aMatrix(0, 1) * (aMatrix(1, 0) * aMatrix(2, 2) - aMatrix(1, 2) * aMatrix(2, 0))
         + aMatrix(0, 2) * (aMatrix(1, 0) * aMatrix(2, 1) - aMatrix(1, 1) * aMatrix(2, 0));
}

// In-place negation of a single column.
template <typename T>
inline void negateColumn(math::matN<T>& aoMatrix, std::size_t aColumnIndex) {
    for (std::size_t rowIndex = 0; rowIndex < aoMatrix.rows(); ++rowIndex) {
        aoMatrix(rowIndex, aColumnIndex) = -aoMatrix(rowIndex, aColumnIndex);
    }
}

}  // namespace detail

// Decompose an essential matrix into the four (R, t) candidates predicted by
// Hartley & Zisserman, Multiple View Geometry, Result 9.19.
//
//     E = U * diag(s, s, 0) * V^T
//     R1 = U * W   * V^T
//     R2 = U * W^T * V^T   with W = [[0,-1,0],[1,0,0],[0,0,1]]
//     t  = U[:,2]   (third column of U), defined up to sign
//
// To guarantee that both R1 and R2 are proper rotations (det = +1), the
// third column of U or V is negated whenever det(U) or det(V) is negative.
// These flips do not change E because the corresponding singular value is
// zero.
//
// The four returned candidates are:
//   [0] = (R1, +t)
//   [1] = (R1, -t)
//   [2] = (R2, +t)
//   [3] = (R2, -t)
//
// Returns false on:
//   - aE not 3x3
//   - SVD failure
template <typename T>
bool decomposeEssentialMatrix(const math::matN<T>& aE, std::array<pose<T>, 4>& aoCandidates) {
    static_assert(std::is_floating_point<T>::value, "decomposeEssentialMatrix requires a floating-point T");

    if (aE.rows() != 3 || aE.columns() != 3) {
        return false;
    }

    math::svd::solver<T> svdSolver;
    math::svd::result<T> svdResult = svdSolver.compute(aE);
    if (!svdResult.success) {
        return false;
    }

    math::matN<T> uMatrix = svdResult.u;
    math::matN<T> vMatrix = svdResult.v;

    // Force U and V into SO(3) by negating the third column when det < 0.
    // Harmless because the corresponding singular value is 0, so this leaves
    // U * diag(s, s, 0) * V^T = E unchanged.
    if (detail::det3x3(uMatrix) < T(0)) {
        detail::negateColumn(uMatrix, 2);
    }
    if (detail::det3x3(vMatrix) < T(0)) {
        detail::negateColumn(vMatrix, 2);
    }

    // W matrix from H&Z 9.13.
    math::matN<T> wMatrix(3, 3);
    wMatrix(0, 0) = T(0);  wMatrix(0, 1) = T(-1); wMatrix(0, 2) = T(0);
    wMatrix(1, 0) = T(1);  wMatrix(1, 1) = T(0);  wMatrix(1, 2) = T(0);
    wMatrix(2, 0) = T(0);  wMatrix(2, 1) = T(0);  wMatrix(2, 2) = T(1);

    math::matN<T> wTransposed = wMatrix.transpose();
    math::matN<T> vTransposed = vMatrix.transpose();

    math::matN<T> rotationFirst  = uMatrix * wMatrix      * vTransposed;
    math::matN<T> rotationSecond = uMatrix * wTransposed  * vTransposed;

    // Translation = third column of U.
    math::vecN<T> translationPositive(3);
    translationPositive[0] = uMatrix(0, 2);
    translationPositive[1] = uMatrix(1, 2);
    translationPositive[2] = uMatrix(2, 2);

    math::vecN<T> translationNegative(3);
    translationNegative[0] = -translationPositive[0];
    translationNegative[1] = -translationPositive[1];
    translationNegative[2] = -translationPositive[2];

    aoCandidates[0].rotation    = rotationFirst;
    aoCandidates[0].translation = translationPositive;
    aoCandidates[1].rotation    = rotationFirst;
    aoCandidates[1].translation = translationNegative;
    aoCandidates[2].rotation    = rotationSecond;
    aoCandidates[2].translation = translationPositive;
    aoCandidates[3].rotation    = rotationSecond;
    aoCandidates[3].translation = translationNegative;

    return true;
}

// Triangulate a 3D point from two 3x4 camera matrices and one image-plane
// correspondence, using the linear DLT formulation of Hartley & Zisserman,
// section 12.2.
//
//     A = [ x1 * P1.row(2) - P1.row(0) ]
//         [ y1 * P1.row(2) - P1.row(1) ]
//         [ x2 * P2.row(2) - P2.row(0) ]
//         [ y2 * P2.row(2) - P2.row(1) ]
//
// The unique 3D point X (homogeneous, 4-vector) is the right singular vector
// of A with smallest singular value. The output is the dehomogenized X[:3].
//
// The (x1, y1, x2, y2) coordinates must be expressed in the same frame as
// the rows of P1 and P2 (typically: pixel coordinates with P = K[R|t], or
// normalized coordinates with P = [R|t]). The function does not enforce a
// choice — be consistent.
//
// Returns false on:
//   - P matrices not 3x4
//   - SVD failure
//   - X[3] near zero (point at infinity, ambiguous)
template <typename T>
bool triangulatePointDLT(
    const math::matN<T>& aP1,
    const math::matN<T>& aP2,
    T aX1, T aY1,
    T aX2, T aY2,
    math::vecN<T>& aoPoint3D) {
    static_assert(std::is_floating_point<T>::value, "triangulatePointDLT requires a floating-point T");

    if (aP1.rows() != 3 || aP1.columns() != 4) {
        return false;
    }
    if (aP2.rows() != 3 || aP2.columns() != 4) {
        return false;
    }

    math::matN<T> aMatrix(4, 4);
    for (std::size_t columnIndex = 0; columnIndex < 4; ++columnIndex) {
        aMatrix(0, columnIndex) = aX1 * aP1(2, columnIndex) - aP1(0, columnIndex);
        aMatrix(1, columnIndex) = aY1 * aP1(2, columnIndex) - aP1(1, columnIndex);
        aMatrix(2, columnIndex) = aX2 * aP2(2, columnIndex) - aP2(0, columnIndex);
        aMatrix(3, columnIndex) = aY2 * aP2(2, columnIndex) - aP2(1, columnIndex);
    }

    math::svd::solver<T> svdSolver;
    math::svd::result<T> svdResult = svdSolver.compute(aMatrix);
    if (!svdResult.success) {
        return false;
    }

    // Right singular vector with smallest singular value = last column of V
    // (singular values are sorted descending).
    const std::size_t lastColumn = svdResult.v.columns() - 1;
    const T xHomogeneous = svdResult.v(0, lastColumn);
    const T yHomogeneous = svdResult.v(1, lastColumn);
    const T zHomogeneous = svdResult.v(2, lastColumn);
    const T wHomogeneous = svdResult.v(3, lastColumn);

    const T epsilon = std::numeric_limits<T>::epsilon() * T(100);
    if (std::abs(wHomogeneous) < epsilon) {
        return false;  // point at infinity
    }

    aoPoint3D = math::vecN<T>(3);
    aoPoint3D[0] = xHomogeneous / wHomogeneous;
    aoPoint3D[1] = yHomogeneous / wHomogeneous;
    aoPoint3D[2] = zHomogeneous / wHomogeneous;
    return true;
}

// Cheirality selector: for each (R, t) candidate, build the canonical
// projection matrix pair P1 = [I | 0] and P2 = [R | t], triangulate every
// input correspondence, and count how many of the resulting 3D points lie
// strictly *in front of both* cameras (positive depth in both views).
//
// The candidate with the largest in-front count wins. Ties are broken by
// lowest candidate index.
//
// Inputs MUST be normalized correspondences (K^-1 already applied) for the
// cheirality count to have geometric meaning.
//
// Returns false on:
//   - aSamples empty
//   - all candidates yielding zero in-front points (degenerate scene)
template <typename T>
bool selectPoseByCheirality(
    const std::array<pose<T>, 4>& aCandidates,
    const std::vector<correspondence<T>>& aSamples,
    pose<T>& aoSelected,
    std::size_t& aoInFrontCount) {
    static_assert(std::is_floating_point<T>::value, "selectPoseByCheirality requires a floating-point T");

    if (aSamples.empty()) {
        return false;
    }

    // P1 = [I | 0] is constant across candidates.
    math::matN<T> projectionFirst(3, 4);
    projectionFirst(0, 0) = T(1); projectionFirst(0, 1) = T(0); projectionFirst(0, 2) = T(0); projectionFirst(0, 3) = T(0);
    projectionFirst(1, 0) = T(0); projectionFirst(1, 1) = T(1); projectionFirst(1, 2) = T(0); projectionFirst(1, 3) = T(0);
    projectionFirst(2, 0) = T(0); projectionFirst(2, 1) = T(0); projectionFirst(2, 2) = T(1); projectionFirst(2, 3) = T(0);

    std::size_t bestCount = 0;
    std::size_t bestIndex = 0;
    bool anyValid = false;

    for (std::size_t candidateIndex = 0; candidateIndex < 4; ++candidateIndex) {
        const pose<T>& candidate = aCandidates[candidateIndex];
        if (candidate.rotation.rows() != 3 || candidate.rotation.columns() != 3) {
            continue;
        }
        if (candidate.translation.size() != 3) {
            continue;
        }

        // P2 = [R | t]
        math::matN<T> projectionSecond(3, 4);
        for (std::size_t rowIndex = 0; rowIndex < 3; ++rowIndex) {
            for (std::size_t columnIndex = 0; columnIndex < 3; ++columnIndex) {
                projectionSecond(rowIndex, columnIndex) = candidate.rotation(rowIndex, columnIndex);
            }
            projectionSecond(rowIndex, 3) = candidate.translation[rowIndex];
        }

        std::size_t inFrontForCandidate = 0;
        for (std::size_t sampleIndex = 0; sampleIndex < aSamples.size(); ++sampleIndex) {
            const correspondence<T>& sample = aSamples[sampleIndex];
            math::vecN<T> point3D;
            if (!triangulatePointDLT(projectionFirst, projectionSecond,
                                     sample.x1, sample.y1, sample.x2, sample.y2,
                                     point3D)) {
                continue;
            }
            // Depth in camera 1 = Z component (because P1 = [I | 0] places
            // the world origin at camera 1).
            const T depthFirst = point3D[2];
            // Depth in camera 2 = third row of (R * X + t).
            const T depthSecond = candidate.rotation(2, 0) * point3D[0]
                                + candidate.rotation(2, 1) * point3D[1]
                                + candidate.rotation(2, 2) * point3D[2]
                                + candidate.translation[2];
            if (depthFirst > T(0) && depthSecond > T(0)) {
                ++inFrontForCandidate;
            }
        }

        anyValid = true;
        if (inFrontForCandidate > bestCount) {
            bestCount = inFrontForCandidate;
            bestIndex = candidateIndex;
        }
    }

    if (!anyValid || bestCount == 0) {
        return false;
    }

    aoSelected = aCandidates[bestIndex];
    aoInFrontCount = bestCount;
    return true;
}

}  // namespace vision
}  // namespace ez

#ifdef _MSC_VER
#pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
