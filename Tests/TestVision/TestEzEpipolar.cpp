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
    for (size_t rowIndex = 0; rowIndex < aLeft.rows(); ++rowIndex) {
        for (size_t columnIndex = 0; columnIndex < aLeft.columns(); ++columnIndex) {
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
    for (size_t componentIndex = 0; componentIndex < aLeft.size(); ++componentIndex) {
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
ez::viz::correspondence<T> projectThroughPair(
    const ez::math::matN<T>& aR, const ez::math::vecN<T>& aT,
    T aX, T aY, T aZ) {
    ez::viz::correspondence<T> result;
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
    std::array<ez::viz::pose<T>, 4> candidates;
    CTEST_ASSERT(!ez::viz::decomposeEssentialMatrix(wrongShape, candidates));
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

    std::array<ez::viz::pose<T>, 4> candidates;
    CTEST_ASSERT(ez::viz::decomposeEssentialMatrix(essential, candidates));

    ez::math::matN<T> identity = makeIdentity3<T>();
    ez::math::vecN<T> expectedTranslationPositive(3);
    expectedTranslationPositive[0] = T(1);
    expectedTranslationPositive[1] = T(0);
    expectedTranslationPositive[2] = T(0);

    // Among the four candidates, at least one must be (R = I, t = (1, 0, 0)).
    bool foundExpected = false;
    const T tolerance = static_cast<T>(1e-5);
    for (size_t i = 0; i < 4; ++i) {
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

    std::array<ez::viz::pose<T>, 4> candidates;
    CTEST_ASSERT(ez::viz::decomposeEssentialMatrix(essential, candidates));

    const T tolerance = static_cast<T>(1e-5);
    for (size_t i = 0; i < 4; ++i) {
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
    CTEST_ASSERT(!ez::viz::triangulatePointDLT(p1, p2WrongShape, T(0), T(0), T(0), T(0), point3D));
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
    CTEST_ASSERT(ez::viz::triangulatePointDLT(p1, p2, T(0), T(0), static_cast<T>(-0.2), T(0), point3D));
    CTEST_ASSERT(point3D.size() == 3);

    const T tolerance = static_cast<T>(1e-4);
    CTEST_ASSERT(ez::math::isEqual(point3D[0], T(0), tolerance));
    CTEST_ASSERT(ez::math::isEqual(point3D[1], T(0), tolerance));
    CTEST_ASSERT(ez::math::isEqual(point3D[2], T(5), tolerance));
    return true;
}

template <typename T>
bool TestEzEpipolar_CheiralityHandlesEmptySamples() {
    std::array<ez::viz::pose<T>, 4> candidates;
    // Build any plausible candidate so we hit the empty-samples guard, not
    // the all-degenerate guard.
    for (size_t i = 0; i < 4; ++i) {
        candidates[i].rotation = makeIdentity3<T>();
        candidates[i].translation = ez::math::vecN<T>(3);
    }
    std::vector<ez::viz::correspondence<T>> empty;
    ez::viz::pose<T> selected;
    size_t inFront = 0;
    CTEST_ASSERT(!ez::viz::selectPoseByCheirality(candidates, empty, selected, inFront));
    return true;
}

template <typename T>
bool TestEzEpipolar_CheiralityWithScoresReportsAllCounts() {
    // Same synthetic scene as CheiralitySelectsCorrectPose: 5 points all
    // strictly in front of both cameras (R = I, t = (1, 0, 0)). For this
    // clean geometry exactly ONE of the four candidates must reach the full
    // sample count; the others must be strictly below it. That property is
    // exactly what makes the new score-returning variant useful as a
    // diagnostic on real datasets (a pair where two candidates tie is the
    // fingerprint of a flipped-camera ambiguity).
    ez::math::matN<T> truthRotation = makeIdentity3<T>();
    ez::math::vecN<T> truthTranslation(3);
    truthTranslation[0] = T(1);
    truthTranslation[1] = T(0);
    truthTranslation[2] = T(0);

    std::vector<ez::viz::correspondence<T>> samples;
    samples.push_back(projectThroughPair(truthRotation, truthTranslation, T(0),  T(0),  T(5)));
    samples.push_back(projectThroughPair(truthRotation, truthTranslation, T(1),  T(0),  T(5)));
    samples.push_back(projectThroughPair(truthRotation, truthTranslation, T(0),  T(1),  T(6)));
    samples.push_back(projectThroughPair(truthRotation, truthTranslation, T(-1), T(0),  T(5)));
    samples.push_back(projectThroughPair(truthRotation, truthTranslation, T(0),  T(-1), T(7)));

    ez::math::matN<T> essential = makeEssentialMatrix(truthRotation, truthTranslation);
    std::array<ez::viz::pose<T>, 4> candidates;
    CTEST_ASSERT(ez::viz::decomposeEssentialMatrix(essential, candidates));

    ez::viz::pose<T> selected;
    size_t selectedIndex = 0;
    std::array<size_t, 4> counts = {{0, 0, 0, 0}};
    CTEST_ASSERT(ez::viz::selectPoseByCheiralityWithScores(candidates, samples, selected, selectedIndex, counts));

    CTEST_ASSERT(selectedIndex < 4);
    CTEST_ASSERT(counts[selectedIndex] == samples.size());

    size_t winnersWithMax = 0;
    for (size_t candIdx = 0; candIdx < 4; ++candIdx) {
        if (counts[candIdx] == samples.size()) {
            ++winnersWithMax;
        }
    }
    CTEST_ASSERT(winnersWithMax == 1);

    // The selected pose returned by the score variant must match the one
    // the historical wrapper would return.
    ez::viz::pose<T> wrapperSelected;
    size_t wrapperInFront = 0;
    CTEST_ASSERT(ez::viz::selectPoseByCheirality(candidates, samples, wrapperSelected, wrapperInFront));
    CTEST_ASSERT(wrapperInFront == counts[selectedIndex]);

    const T tolerance = static_cast<T>(1e-5);
    CTEST_ASSERT(matrixIsClose(wrapperSelected.rotation, selected.rotation, tolerance));
    CTEST_ASSERT(vectorIsClose(wrapperSelected.translation, selected.translation, tolerance));
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

    std::vector<ez::viz::correspondence<T>> samples;
    samples.push_back(projectThroughPair(truthRotation, truthTranslation, T(0),  T(0),  T(5)));
    samples.push_back(projectThroughPair(truthRotation, truthTranslation, T(1),  T(0),  T(5)));
    samples.push_back(projectThroughPair(truthRotation, truthTranslation, T(0),  T(1),  T(6)));
    samples.push_back(projectThroughPair(truthRotation, truthTranslation, T(-1), T(0),  T(5)));
    samples.push_back(projectThroughPair(truthRotation, truthTranslation, T(0),  T(-1), T(7)));

    // Build the true essential matrix and decompose it.
    ez::math::matN<T> essential = makeEssentialMatrix(truthRotation, truthTranslation);
    std::array<ez::viz::pose<T>, 4> candidates;
    CTEST_ASSERT(ez::viz::decomposeEssentialMatrix(essential, candidates));

    // Select via cheirality.
    ez::viz::pose<T> selected;
    size_t inFront = 0;
    CTEST_ASSERT(ez::viz::selectPoseByCheirality(candidates, samples, selected, inFront));

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

template <typename T>
bool TestEzEpipolar_MedianParallaxOnKnownScene() {
    // Scene: camera 1 at origin, camera 2 at world position (-1, 0, 0)
    // with R = I (same orientation), so t = (1, 0, 0). Points at depth
    // z = 5 in front of both cameras.
    //
    // For a 3D point at (0, 0, 5):
    //   ray1 = (0, 0, 5), norm = 5
    //   ray2 = (0-(-1), 0, 5) = (1, 0, 5), norm = sqrt(26)
    //   cos(parallax) = 5 / (5 * sqrt(26)) = 1 / sqrt(26)
    //   parallax = acos(0.196) ≈ 11.3 deg
    //
    // For (1, 0, 5):
    //   ray1 = (1, 0, 5), norm = sqrt(26)
    //   ray2 = (2, 0, 5), norm = sqrt(29)
    //   cos = (2 + 25) / (sqrt(26) * sqrt(29)) ≈ 0.9824
    //   parallax ≈ 10.8 deg
    //
    // The 5 sample scene from CheiralitySelectsCorrectPose has parallax
    // values clustered around 10-12 deg; the median must fall in that
    // window.
    ez::math::matN<T> truthRotation = makeIdentity3<T>();
    ez::math::vecN<T> truthTranslation(3);
    truthTranslation[0] = T(1);
    truthTranslation[1] = T(0);
    truthTranslation[2] = T(0);

    std::vector<ez::viz::correspondence<T>> samples;
    samples.push_back(projectThroughPair(truthRotation, truthTranslation, T(0),  T(0),  T(5)));
    samples.push_back(projectThroughPair(truthRotation, truthTranslation, T(1),  T(0),  T(5)));
    samples.push_back(projectThroughPair(truthRotation, truthTranslation, T(0),  T(1),  T(6)));
    samples.push_back(projectThroughPair(truthRotation, truthTranslation, T(-1), T(0),  T(5)));
    samples.push_back(projectThroughPair(truthRotation, truthTranslation, T(0),  T(-1), T(7)));

    ez::viz::pose<T> truthPose;
    truthPose.rotation = truthRotation;
    truthPose.translation = truthTranslation;

    const T parallaxDeg = ez::viz::computeMedianParallaxDegrees(truthPose, samples);
    // Expected median is in the 5-15 deg range for this geometry.
    CTEST_ASSERT(parallaxDeg > static_cast<T>(5));
    CTEST_ASSERT(parallaxDeg < static_cast<T>(15));
    return true;
}

template <typename T>
bool TestEzEpipolar_MedianParallaxEmptyReturnsZero() {
    ez::viz::pose<T> p;
    p.rotation = makeIdentity3<T>();
    p.translation = ez::math::vecN<T>(3);
    p.translation[0] = T(1);
    std::vector<ez::viz::correspondence<T>> empty;
    const T parallaxDeg = ez::viz::computeMedianParallaxDegrees(p, empty);
    CTEST_ASSERT(ez::math::isEqual(parallaxDeg, T(0), static_cast<T>(1e-6)));
    return true;
}

template <typename T>
bool TestEzEpipolar_HomographyFitRejectsTooFewSamples() {
    std::vector<ez::viz::correspondence<T>> samples;
    samples.resize(3);  // 3 < 4
    T medianError = T(-1);
    CTEST_ASSERT(!ez::viz::fitHomographyAndMedianError(samples, medianError));
    return true;
}

template <typename T>
bool TestEzEpipolar_HomographyFitRecoversKnownPlanarH() {
    // Build a known homography (identity + small perturbation), generate
    // 8 exact correspondences via it, then fit and verify the median
    // reprojection error is essentially zero.
    //
    // H = [[1.1,  0.0, 0.02],
    //      [0.0,  1.2, 0.03],
    //      [0.0,  0.0, 1.0 ]]
    ez::math::matN<T> truthH(3, 3);
    truthH(0, 0) = static_cast<T>(1.1); truthH(0, 1) = T(0);            truthH(0, 2) = static_cast<T>(0.02);
    truthH(1, 0) = T(0);                truthH(1, 1) = static_cast<T>(1.2); truthH(1, 2) = static_cast<T>(0.03);
    truthH(2, 0) = T(0);                truthH(2, 1) = T(0);            truthH(2, 2) = T(1);

    std::vector<ez::viz::correspondence<T>> samples;
    const T sourcePoints[8][2] = {
        {T(-1), T(-1)}, {T(1), T(-1)}, {T(1), T(1)}, {T(-1), T(1)},
        {T(0),  T(-1)}, {T(1), T(0)},  {T(0), T(1)}, {T(-1), T(0)}
    };
    for (size_t i = 0; i < 8; ++i) {
        const T x1 = sourcePoints[i][0];
        const T y1 = sourcePoints[i][1];
        const T xp = truthH(0, 0) * x1 + truthH(0, 1) * y1 + truthH(0, 2);
        const T yp = truthH(1, 0) * x1 + truthH(1, 1) * y1 + truthH(1, 2);
        const T wp = truthH(2, 0) * x1 + truthH(2, 1) * y1 + truthH(2, 2);
        ez::viz::correspondence<T> sample;
        sample.x1 = x1;
        sample.y1 = y1;
        sample.x2 = xp / wp;
        sample.y2 = yp / wp;
        samples.push_back(sample);
    }

    T medianError = T(-1);
    CTEST_ASSERT(ez::viz::fitHomographyAndMedianError(samples, medianError));
    // Exact data → median error should be at floating-point noise level.
    CTEST_ASSERT(medianError < static_cast<T>(1e-4));
    return true;
}

template <typename T>
bool TestEzEpipolar_EpipolarErrorOnExactScene() {
    // Same scene as CheiralitySelectsCorrectPose: 5 in-front points with
    // R = I, t = (1, 0, 0). The matches projected exactly through this
    // (R, t) must give a median epipolar error close to zero under the
    // matching E = [t]x * R.
    ez::math::matN<T> truthRotation = makeIdentity3<T>();
    ez::math::vecN<T> truthTranslation(3);
    truthTranslation[0] = T(1);
    truthTranslation[1] = T(0);
    truthTranslation[2] = T(0);

    std::vector<ez::viz::correspondence<T>> samples;
    samples.push_back(projectThroughPair(truthRotation, truthTranslation, T(0),  T(0),  T(5)));
    samples.push_back(projectThroughPair(truthRotation, truthTranslation, T(1),  T(0),  T(5)));
    samples.push_back(projectThroughPair(truthRotation, truthTranslation, T(0),  T(1),  T(6)));
    samples.push_back(projectThroughPair(truthRotation, truthTranslation, T(-1), T(0),  T(5)));
    samples.push_back(projectThroughPair(truthRotation, truthTranslation, T(0),  T(-1), T(7)));

    ez::math::matN<T> essential = makeEssentialMatrix(truthRotation, truthTranslation);
    const T medianError = ez::viz::computeMedianEpipolarError(essential, samples);
    CTEST_ASSERT(medianError < static_cast<T>(1e-4));
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
    else IfTestExist(TestEzEpipolar_CheiralityWithScoresReportsAllCounts<float>);
    else IfTestExist(TestEzEpipolar_CheiralityWithScoresReportsAllCounts<double>);
    else IfTestExist(TestEzEpipolar_CheiralitySelectsCorrectPose<float>);
    else IfTestExist(TestEzEpipolar_CheiralitySelectsCorrectPose<double>);
    else IfTestExist(TestEzEpipolar_MedianParallaxOnKnownScene<float>);
    else IfTestExist(TestEzEpipolar_MedianParallaxOnKnownScene<double>);
    else IfTestExist(TestEzEpipolar_MedianParallaxEmptyReturnsZero<float>);
    else IfTestExist(TestEzEpipolar_MedianParallaxEmptyReturnsZero<double>);
    else IfTestExist(TestEzEpipolar_HomographyFitRejectsTooFewSamples<float>);
    else IfTestExist(TestEzEpipolar_HomographyFitRejectsTooFewSamples<double>);
    else IfTestExist(TestEzEpipolar_HomographyFitRecoversKnownPlanarH<float>);
    else IfTestExist(TestEzEpipolar_HomographyFitRecoversKnownPlanarH<double>);
    else IfTestExist(TestEzEpipolar_EpipolarErrorOnExactScene<float>);
    else IfTestExist(TestEzEpipolar_EpipolarErrorOnExactScene<double>);
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
