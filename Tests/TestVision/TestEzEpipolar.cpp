#include <ezlibs/ezVision/ezEpipolar.hpp>
#include <ezlibs/ezMath/ezMath.hpp>
#include <ezlibs/ezCTest.hpp>

#include <array>
#include <cmath>
#include <cstddef>
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
// Test helpers
////////////////////////////////////////////////////////////////////////////

namespace {

// Element-wise matrix equality with tolerance.
template <typename T>
bool matrixIsClose(const ez::math::matN<T>& aLeft, const ez::math::matN<T>& aRight, T aTolerance) {
    if (aLeft.rows() != aRight.rows() || aLeft.columns() != aRight.columns()) {
        return false;
    }
    for (std::size_t rowIndex = 0; rowIndex < aLeft.rows(); ++rowIndex) {
        for (std::size_t columnIndex = 0; columnIndex < aLeft.columns(); ++columnIndex) {
            if (!ez::math::isEqual(aLeft(rowIndex, columnIndex), aRight(rowIndex, columnIndex), aTolerance)) {
                return false;
            }
        }
    }
    return true;
}

// Element-wise vector equality with tolerance.
template <typename T>
bool vectorIsClose(const ez::math::vecN<T>& aLeft, const ez::math::vecN<T>& aRight, T aTolerance) {
    if (aLeft.size() != aRight.size()) {
        return false;
    }
    for (std::size_t componentIndex = 0; componentIndex < aLeft.size(); ++componentIndex) {
        if (!ez::math::isEqual(aLeft[componentIndex], aRight[componentIndex], aTolerance)) {
            return false;
        }
    }
    return true;
}

// Build a 3x3 rotation matrix as identity. Useful baseline.
template <typename T>
ez::math::matN<T> makeIdentity3() {
    return ez::math::matN<T>::Identity(3);
}

// Build a 3x3 rotation around the Y axis by aAngleRadians.
template <typename T>
ez::math::matN<T> makeRotationY(T aAngleRadians) {
    ez::math::matN<T> rotation(3, 3);
    const T cosA = std::cos(aAngleRadians);
    const T sinA = std::sin(aAngleRadians);
    rotation(0, 0) =  cosA;  rotation(0, 1) = T(0); rotation(0, 2) = sinA;
    rotation(1, 0) = T(0);   rotation(1, 1) = T(1); rotation(1, 2) = T(0);
    rotation(2, 0) = -sinA;  rotation(2, 1) = T(0); rotation(2, 2) = cosA;
    return rotation;
}

// Project a 3D world point through a (R, t) camera pose into normalized
// image coordinates. World frame is taken as camera 1's frame, so the first
// view is just (X / Z, Y / Z); the second is computed from (R, t).
template <typename T>
ez::vision::correspondence<T> projectThroughPair(
    const ez::math::matN<T>& aR, const ez::math::vecN<T>& aT,
    T aX, T aY, T aZ) {
    ez::vision::correspondence<T> result;
    result.x1 = aX / aZ;
    result.y1 = aY / aZ;
    const T xCam2 = aR(0, 0) * aX + aR(0, 1) * aY + aR(0, 2) * aZ + aT[0];
    const T yCam2 = aR(1, 0) * aX + aR(1, 1) * aY + aR(1, 2) * aZ + aT[1];
    const T zCam2 = aR(2, 0) * aX + aR(2, 1) * aY + aR(2, 2) * aZ + aT[2];
    result.x2 = xCam2 / zCam2;
    result.y2 = yCam2 / zCam2;
    return result;
}

// Build the essential matrix corresponding to a (R, t) pair: E = [t]x * R,
// where [t]x is the skew-symmetric cross-product matrix of t.
template <typename T>
ez::math::matN<T> makeEssentialMatrix(const ez::math::matN<T>& aR, const ez::math::vecN<T>& aT) {
    ez::math::matN<T> skew(3, 3);
    skew(0, 0) = T(0);    skew(0, 1) = -aT[2]; skew(0, 2) =  aT[1];
    skew(1, 0) =  aT[2];  skew(1, 1) = T(0);   skew(1, 2) = -aT[0];
    skew(2, 0) = -aT[1];  skew(2, 1) =  aT[0]; skew(2, 2) = T(0);
    return skew * aR;
}

// Determinant of a 3x3 matrix.
template <typename T>
T det3(const ez::math::matN<T>& aMatrix) {
    return aMatrix(0, 0) * (aMatrix(1, 1) * aMatrix(2, 2) - aMatrix(1, 2) * aMatrix(2, 1))
         - aMatrix(0, 1) * (aMatrix(1, 0) * aMatrix(2, 2) - aMatrix(1, 2) * aMatrix(2, 0))
         + aMatrix(0, 2) * (aMatrix(1, 0) * aMatrix(2, 1) - aMatrix(1, 1) * aMatrix(2, 0));
}

}  // namespace

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

template <typename T>
bool TestEzEpipolar_DecomposeRejectsNonSquare() {
    ez::math::matN<T> wrongShape(3, 4);
    std::array<ez::vision::pose<T>, 4> candidates;
    CTEST_ASSERT(!ez::vision::decomposeEssentialMatrix(wrongShape, candidates));
    return true;
}

template <typename T>
bool TestEzEpipolar_DecomposePureXTranslation() {
    // Reference setup: R = I, t = (1, 0, 0). Then E = [t]x * I:
    //   E = [[ 0,  0,  0],
    //        [ 0,  0, -1],
    //        [ 0,  1,  0]]
    ez::math::matN<T> essential(3, 3);
    essential(0, 0) = T(0); essential(0, 1) = T(0);  essential(0, 2) = T(0);
    essential(1, 0) = T(0); essential(1, 1) = T(0);  essential(1, 2) = T(-1);
    essential(2, 0) = T(0); essential(2, 1) = T(1);  essential(2, 2) = T(0);

    std::array<ez::vision::pose<T>, 4> candidates;
    CTEST_ASSERT(ez::vision::decomposeEssentialMatrix(essential, candidates));

    ez::math::matN<T> identity = makeIdentity3<T>();
    ez::math::vecN<T> expectedTranslationPositive(3);
    expectedTranslationPositive[0] = T(1);
    expectedTranslationPositive[1] = T(0);
    expectedTranslationPositive[2] = T(0);

    // Among the four candidates, at least one must be (R = I, t = (1, 0, 0)).
    bool foundExpected = false;
    const T tolerance = static_cast<T>(1e-5);
    for (std::size_t i = 0; i < 4; ++i) {
        if (matrixIsClose(candidates[i].rotation, identity, tolerance)
         && vectorIsClose(candidates[i].translation, expectedTranslationPositive, tolerance)) {
            foundExpected = true;
            break;
        }
    }
    CTEST_ASSERT(foundExpected);
    return true;
}

template <typename T>
bool TestEzEpipolar_DecomposeProducesProperRotations() {
    // Use the same pure-X-translation E.
    ez::math::matN<T> essential(3, 3);
    essential(0, 0) = T(0); essential(0, 1) = T(0);  essential(0, 2) = T(0);
    essential(1, 0) = T(0); essential(1, 1) = T(0);  essential(1, 2) = T(-1);
    essential(2, 0) = T(0); essential(2, 1) = T(1);  essential(2, 2) = T(0);

    std::array<ez::vision::pose<T>, 4> candidates;
    CTEST_ASSERT(ez::vision::decomposeEssentialMatrix(essential, candidates));

    const T tolerance = static_cast<T>(1e-5);
    for (std::size_t i = 0; i < 4; ++i) {
        const ez::math::matN<T>& rotation = candidates[i].rotation;
        // Check det(R) = +1.
        const T determinant = det3(rotation);
        CTEST_ASSERT(ez::math::isEqual(determinant, T(1), tolerance));
        // Check R^T * R = I.
        ez::math::matN<T> product = rotation.transpose() * rotation;
        CTEST_ASSERT(matrixIsClose(product, makeIdentity3<T>(), tolerance));
    }
    return true;
}

template <typename T>
bool TestEzEpipolar_TriangulateRejectsBadDimensions() {
    ez::math::matN<T> p1(3, 4);
    ez::math::matN<T> p2WrongShape(3, 3);  // wrong: needs to be 3x4
    ez::math::vecN<T> point3D;
    CTEST_ASSERT(!ez::vision::triangulatePointDLT(p1, p2WrongShape, T(0), T(0), T(0), T(0), point3D));
    return true;
}

template <typename T>
bool TestEzEpipolar_TriangulateAxisAlignedCameras() {
    // Camera 1 at world origin (P1 = [I | 0]).
    // Camera 2 at world position (1, 0, 0) with R = I, so t = -R * c = (-1, 0, 0)
    // (P2 = [I | (-1, 0, 0)]).
    // 3D world point at (0, 0, 5):
    //   cam1 image = (0/5, 0/5) = (0, 0)
    //   cam2 image = ((0-1)/5, 0/5) = (-0.2, 0)
    ez::math::matN<T> p1(3, 4);
    p1(0, 0) = T(1); p1(0, 1) = T(0); p1(0, 2) = T(0); p1(0, 3) = T(0);
    p1(1, 0) = T(0); p1(1, 1) = T(1); p1(1, 2) = T(0); p1(1, 3) = T(0);
    p1(2, 0) = T(0); p1(2, 1) = T(0); p1(2, 2) = T(1); p1(2, 3) = T(0);

    ez::math::matN<T> p2(3, 4);
    p2(0, 0) = T(1); p2(0, 1) = T(0); p2(0, 2) = T(0); p2(0, 3) = T(-1);
    p2(1, 0) = T(0); p2(1, 1) = T(1); p2(1, 2) = T(0); p2(1, 3) = T(0);
    p2(2, 0) = T(0); p2(2, 1) = T(0); p2(2, 2) = T(1); p2(2, 3) = T(0);

    ez::math::vecN<T> point3D;
    CTEST_ASSERT(ez::vision::triangulatePointDLT(p1, p2, T(0), T(0), static_cast<T>(-0.2), T(0), point3D));
    CTEST_ASSERT(point3D.size() == 3);

    const T tolerance = static_cast<T>(1e-4);
    CTEST_ASSERT(ez::math::isEqual(point3D[0], T(0), tolerance));
    CTEST_ASSERT(ez::math::isEqual(point3D[1], T(0), tolerance));
    CTEST_ASSERT(ez::math::isEqual(point3D[2], T(5), tolerance));
    return true;
}

template <typename T>
bool TestEzEpipolar_CheiralityHandlesEmptySamples() {
    std::array<ez::vision::pose<T>, 4> candidates;
    // Build any plausible candidate so we hit the empty-samples guard, not
    // the all-degenerate guard.
    for (std::size_t i = 0; i < 4; ++i) {
        candidates[i].rotation = makeIdentity3<T>();
        candidates[i].translation = ez::math::vecN<T>(3);
    }
    std::vector<ez::vision::correspondence<T>> empty;
    ez::vision::pose<T> selected;
    std::size_t inFront = 0;
    CTEST_ASSERT(!ez::vision::selectPoseByCheirality(candidates, empty, selected, inFront));
    return true;
}

template <typename T>
bool TestEzEpipolar_CheiralitySelectsCorrectPose() {
    // Synthetic scene:
    //   - Camera 1 at world origin (R = I, t = 0).
    //   - Camera 2 at world position (-1, 0, 0), looking the same way as
    //     camera 1: R = I, so t_for_camera2 = -R * c = (1, 0, 0).
    //   - 5 points at z = 5..7, well in front of both cameras.
    ez::math::matN<T> truthRotation = makeIdentity3<T>();
    ez::math::vecN<T> truthTranslation(3);
    truthTranslation[0] = T(1);
    truthTranslation[1] = T(0);
    truthTranslation[2] = T(0);

    std::vector<ez::vision::correspondence<T>> samples;
    samples.push_back(projectThroughPair(truthRotation, truthTranslation, T(0),  T(0),  T(5)));
    samples.push_back(projectThroughPair(truthRotation, truthTranslation, T(1),  T(0),  T(5)));
    samples.push_back(projectThroughPair(truthRotation, truthTranslation, T(0),  T(1),  T(6)));
    samples.push_back(projectThroughPair(truthRotation, truthTranslation, T(-1), T(0),  T(5)));
    samples.push_back(projectThroughPair(truthRotation, truthTranslation, T(0),  T(-1), T(7)));

    // Build the true essential matrix and decompose it.
    ez::math::matN<T> essential = makeEssentialMatrix(truthRotation, truthTranslation);
    std::array<ez::vision::pose<T>, 4> candidates;
    CTEST_ASSERT(ez::vision::decomposeEssentialMatrix(essential, candidates));

    // Select via cheirality.
    ez::vision::pose<T> selected;
    std::size_t inFront = 0;
    CTEST_ASSERT(ez::vision::selectPoseByCheirality(candidates, samples, selected, inFront));

    // The winner must have all 5 samples in front of both cameras.
    CTEST_ASSERT(inFront == samples.size());

    // The winner's R must equal identity, and t must equal (1, 0, 0) up to
    // the inherent unit scale (which is preserved here because we built E
    // with ||t|| = 1).
    const T tolerance = static_cast<T>(1e-4);
    CTEST_ASSERT(matrixIsClose(selected.rotation, truthRotation, tolerance));
    CTEST_ASSERT(vectorIsClose(selected.translation, truthTranslation, tolerance));
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzEpipolar(const std::string& vTest) {
    IfTestExist(TestEzEpipolar_DecomposeRejectsNonSquare<float>);
    else IfTestExist(TestEzEpipolar_DecomposeRejectsNonSquare<double>);
    else IfTestExist(TestEzEpipolar_DecomposePureXTranslation<float>);
    else IfTestExist(TestEzEpipolar_DecomposePureXTranslation<double>);
    else IfTestExist(TestEzEpipolar_DecomposeProducesProperRotations<float>);
    else IfTestExist(TestEzEpipolar_DecomposeProducesProperRotations<double>);
    else IfTestExist(TestEzEpipolar_TriangulateRejectsBadDimensions<float>);
    else IfTestExist(TestEzEpipolar_TriangulateRejectsBadDimensions<double>);
    else IfTestExist(TestEzEpipolar_TriangulateAxisAlignedCameras<float>);
    else IfTestExist(TestEzEpipolar_TriangulateAxisAlignedCameras<double>);
    else IfTestExist(TestEzEpipolar_CheiralityHandlesEmptySamples<float>);
    else IfTestExist(TestEzEpipolar_CheiralityHandlesEmptySamples<double>);
    else IfTestExist(TestEzEpipolar_CheiralitySelectsCorrectPose<float>);
    else IfTestExist(TestEzEpipolar_CheiralitySelectsCorrectPose<double>);
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
