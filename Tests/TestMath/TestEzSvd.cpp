#include <ezlibs/ezMath/ezSvd.hpp>
#include <ezlibs/ezMath/ezMath.hpp>
#include <ezlibs/ezCTest.hpp>

#include <cstddef>

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

// Build the (n x n) diagonal matrix of singular values.
template <typename T>
ez::math::matN<T> makeSigma(const ez::math::vecN<T>& aSingularValues) {
    std::size_t n = aSingularValues.size();
    ez::math::matN<T> sigma(n, n);
    for (std::size_t i = 0; i < n; ++i) {
        sigma(i, i) = aSingularValues[i];
    }
    return sigma;
}

// Element-wise matrix equality with tolerance.
template <typename T>
bool matrixIsEqual(const ez::math::matN<T>& aLeft, const ez::math::matN<T>& aRight, T aTolerance) {
    if (aLeft.rows() != aRight.rows() || aLeft.columns() != aRight.columns()) {
        return false;
    }
    for (std::size_t i = 0; i < aLeft.rows(); ++i) {
        for (std::size_t j = 0; j < aLeft.columns(); ++j) {
            if (!ez::math::isEqual(aLeft(i, j), aRight(i, j), aTolerance)) {
                return false;
            }
        }
    }
    return true;
}

// Reconstruct A = U * diag(s) * V^T.
template <typename T>
ez::math::matN<T> reconstruct(const ez::math::svd::result<T>& aResult) {
    return aResult.u * makeSigma(aResult.s) * aResult.v.transpose();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

template <typename T>
bool TestEzSvd_EmptyMatrix() {
    ez::math::matN<T> empty;
    ez::math::svd::solver<T> svdSolver;
    ez::math::svd::result<T> output = svdSolver.compute(empty);
    CTEST_ASSERT(!output.success);
    return true;
}

template <typename T>
bool TestEzSvd_Identity3x3() {
    ez::math::matN<T> identity = ez::math::matN<T>::Identity(3);
    ez::math::svd::solver<T> svdSolver;
    ez::math::svd::result<T> output = svdSolver.compute(identity);
    CTEST_ASSERT(output.success);
    CTEST_ASSERT(output.s.size() == 3);
    CTEST_ASSERT(ez::math::isEqual(output.s[0], static_cast<T>(1), static_cast<T>(1e-6)));
    CTEST_ASSERT(ez::math::isEqual(output.s[1], static_cast<T>(1), static_cast<T>(1e-6)));
    CTEST_ASSERT(ez::math::isEqual(output.s[2], static_cast<T>(1), static_cast<T>(1e-6)));
    return true;
}

template <typename T>
bool TestEzSvd_Diagonal() {
    // diag(3, 1, 2) -> singular values must come back sorted as [3, 2, 1]
    ez::math::matN<T> diagonal(3, 3);
    diagonal(0, 0) = static_cast<T>(3);
    diagonal(1, 1) = static_cast<T>(1);
    diagonal(2, 2) = static_cast<T>(2);
    ez::math::svd::solver<T> svdSolver;
    ez::math::svd::result<T> output = svdSolver.compute(diagonal);
    CTEST_ASSERT(output.success);
    CTEST_ASSERT(output.s.size() == 3);
    CTEST_ASSERT(ez::math::isEqual(output.s[0], static_cast<T>(3), static_cast<T>(1e-6)));
    CTEST_ASSERT(ez::math::isEqual(output.s[1], static_cast<T>(2), static_cast<T>(1e-6)));
    CTEST_ASSERT(ez::math::isEqual(output.s[2], static_cast<T>(1), static_cast<T>(1e-6)));
    return true;
}

template <typename T>
bool TestEzSvd_DiagonalNegative() {
    // diag(2, -3, 1) -> singular values are absolute values, sorted: [3, 2, 1]
    ez::math::matN<T> diagonal(3, 3);
    diagonal(0, 0) = static_cast<T>(2);
    diagonal(1, 1) = static_cast<T>(-3);
    diagonal(2, 2) = static_cast<T>(1);
    ez::math::svd::solver<T> svdSolver;
    ez::math::svd::result<T> output = svdSolver.compute(diagonal);
    CTEST_ASSERT(output.success);
    CTEST_ASSERT(ez::math::isEqual(output.s[0], static_cast<T>(3), static_cast<T>(1e-6)));
    CTEST_ASSERT(ez::math::isEqual(output.s[1], static_cast<T>(2), static_cast<T>(1e-6)));
    CTEST_ASSERT(ez::math::isEqual(output.s[2], static_cast<T>(1), static_cast<T>(1e-6)));
    return true;
}

template <typename T>
bool TestEzSvd_ReconstructionSquare() {
    // Arbitrary 3x3 well-conditioned matrix.
    ez::math::matN<T> matrix(3, 3);
    matrix(0, 0) = static_cast<T>(1); matrix(0, 1) = static_cast<T>(2); matrix(0, 2) = static_cast<T>(3);
    matrix(1, 0) = static_cast<T>(4); matrix(1, 1) = static_cast<T>(5); matrix(1, 2) = static_cast<T>(6);
    matrix(2, 0) = static_cast<T>(7); matrix(2, 1) = static_cast<T>(8); matrix(2, 2) = static_cast<T>(10);

    ez::math::svd::solver<T> svdSolver;
    ez::math::svd::result<T> output = svdSolver.compute(matrix);
    CTEST_ASSERT(output.success);
    CTEST_ASSERT(output.s.size() == 3);

    ez::math::matN<T> reconstructed = reconstruct(output);
    CTEST_ASSERT(matrixIsEqual(matrix, reconstructed, static_cast<T>(1e-5)));
    return true;
}

template <typename T>
bool TestEzSvd_ReconstructionTallRectangular() {
    // 4 rows x 3 cols (m > n).
    ez::math::matN<T> matrix(4, 3);
    matrix(0, 0) = static_cast<T>(1); matrix(0, 1) = static_cast<T>(0); matrix(0, 2) = static_cast<T>(2);
    matrix(1, 0) = static_cast<T>(3); matrix(1, 1) = static_cast<T>(1); matrix(1, 2) = static_cast<T>(0);
    matrix(2, 0) = static_cast<T>(0); matrix(2, 1) = static_cast<T>(2); matrix(2, 2) = static_cast<T>(4);
    matrix(3, 0) = static_cast<T>(2); matrix(3, 1) = static_cast<T>(1); matrix(3, 2) = static_cast<T>(1);

    ez::math::svd::solver<T> svdSolver;
    ez::math::svd::result<T> output = svdSolver.compute(matrix);
    CTEST_ASSERT(output.success);
    CTEST_ASSERT(output.u.rows() == 4);
    CTEST_ASSERT(output.u.columns() == 3);
    CTEST_ASSERT(output.s.size() == 3);
    CTEST_ASSERT(output.v.rows() == 3);
    CTEST_ASSERT(output.v.columns() == 3);

    ez::math::matN<T> reconstructed = reconstruct(output);
    CTEST_ASSERT(matrixIsEqual(matrix, reconstructed, static_cast<T>(1e-5)));
    return true;
}

template <typename T>
bool TestEzSvd_ReconstructionWideRectangular() {
    // 2 rows x 4 cols (m < n) -> exercises the internal transpose path.
    ez::math::matN<T> matrix(2, 4);
    matrix(0, 0) = static_cast<T>(1); matrix(0, 1) = static_cast<T>(2); matrix(0, 2) = static_cast<T>(3); matrix(0, 3) = static_cast<T>(4);
    matrix(1, 0) = static_cast<T>(5); matrix(1, 1) = static_cast<T>(6); matrix(1, 2) = static_cast<T>(7); matrix(1, 3) = static_cast<T>(8);

    ez::math::svd::solver<T> svdSolver;
    ez::math::svd::result<T> output = svdSolver.compute(matrix);
    CTEST_ASSERT(output.success);
    // After internal transpose + swap: U is m x n, V is n x n in the original
    // orientation, where m=2 and n=4 here.
    CTEST_ASSERT(output.u.rows() == 2);
    CTEST_ASSERT(output.u.columns() == 2);
    CTEST_ASSERT(output.s.size() == 2);
    CTEST_ASSERT(output.v.rows() == 4);
    CTEST_ASSERT(output.v.columns() == 2);

    ez::math::matN<T> reconstructed = reconstruct(output);
    CTEST_ASSERT(matrixIsEqual(matrix, reconstructed, static_cast<T>(1e-5)));
    return true;
}

template <typename T>
bool TestEzSvd_SingularValuesAreSortedDescending() {
    // 3x3 matrix with three distinct singular values.
    ez::math::matN<T> matrix(3, 3);
    matrix(0, 0) = static_cast<T>(4); matrix(0, 1) = static_cast<T>(0); matrix(0, 2) = static_cast<T>(0);
    matrix(1, 0) = static_cast<T>(0); matrix(1, 1) = static_cast<T>(1); matrix(1, 2) = static_cast<T>(0);
    matrix(2, 0) = static_cast<T>(0); matrix(2, 1) = static_cast<T>(0); matrix(2, 2) = static_cast<T>(2);

    ez::math::svd::solver<T> svdSolver;
    ez::math::svd::result<T> output = svdSolver.compute(matrix);
    CTEST_ASSERT(output.success);
    CTEST_ASSERT(output.s[0] >= output.s[1]);
    CTEST_ASSERT(output.s[1] >= output.s[2]);
    CTEST_ASSERT(output.s[2] >= static_cast<T>(0));
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzSvd(const std::string& vTest) {
    IfTestExist(TestEzSvd_EmptyMatrix<float>);
    else IfTestExist(TestEzSvd_EmptyMatrix<double>);
    else IfTestExist(TestEzSvd_Identity3x3<float>);
    else IfTestExist(TestEzSvd_Identity3x3<double>);
    else IfTestExist(TestEzSvd_Diagonal<float>);
    else IfTestExist(TestEzSvd_Diagonal<double>);
    else IfTestExist(TestEzSvd_DiagonalNegative<float>);
    else IfTestExist(TestEzSvd_DiagonalNegative<double>);
    else IfTestExist(TestEzSvd_ReconstructionSquare<float>);
    else IfTestExist(TestEzSvd_ReconstructionSquare<double>);
    else IfTestExist(TestEzSvd_ReconstructionTallRectangular<float>);
    else IfTestExist(TestEzSvd_ReconstructionTallRectangular<double>);
    else IfTestExist(TestEzSvd_ReconstructionWideRectangular<float>);
    else IfTestExist(TestEzSvd_ReconstructionWideRectangular<double>);
    else IfTestExist(TestEzSvd_SingularValuesAreSortedDescending<float>);
    else IfTestExist(TestEzSvd_SingularValuesAreSortedDescending<double>);
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
