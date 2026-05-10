#include <ezlibs/ezMath/ezEigenSym.hpp>
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

// Build the (n x n) diagonal matrix from an n-vector.
template <typename T>
ez::math::matN<T> makeDiagonal(const ez::math::vecN<T>& aValues) {
    std::size_t n = aValues.size();
    ez::math::matN<T> diagonal(n, n);
    for (std::size_t i = 0; i < n; ++i) {
        diagonal(i, i) = aValues[i];
    }
    return diagonal;
}

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

// Reconstruct A = V * diag(d) * V^T.
template <typename T>
ez::math::matN<T> reconstruct(const ez::math::eigenSym::result<T>& aResult) {
    return aResult.v * makeDiagonal(aResult.d) * aResult.v.transpose();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

template <typename T>
bool TestEzEigenSym_EmptyMatrix() {
    ez::math::matN<T> empty;
    ez::math::eigenSym::solver<T> eigSolver;
    ez::math::eigenSym::result<T> output = eigSolver.compute(empty);
    CTEST_ASSERT(!output.success);
    return true;
}

template <typename T>
bool TestEzEigenSym_NonSquareMatrix() {
    ez::math::matN<T> rectangular(3, 4);
    ez::math::eigenSym::solver<T> eigSolver;
    ez::math::eigenSym::result<T> output = eigSolver.compute(rectangular);
    CTEST_ASSERT(!output.success);
    return true;
}

template <typename T>
bool TestEzEigenSym_Identity3x3() {
    ez::math::matN<T> identity = ez::math::matN<T>::Identity(3);
    ez::math::eigenSym::solver<T> eigSolver;
    ez::math::eigenSym::result<T> output = eigSolver.compute(identity);
    CTEST_ASSERT(output.success);
    CTEST_ASSERT(output.d.size() == 3);
    CTEST_ASSERT(ez::math::isEqual(output.d[0], static_cast<T>(1), static_cast<T>(1e-6)));
    CTEST_ASSERT(ez::math::isEqual(output.d[1], static_cast<T>(1), static_cast<T>(1e-6)));
    CTEST_ASSERT(ez::math::isEqual(output.d[2], static_cast<T>(1), static_cast<T>(1e-6)));
    return true;
}

template <typename T>
bool TestEzEigenSym_Diagonal() {
    // diag(3, 1, 2) -> eigenvalues sorted descending: [3, 2, 1]
    ez::math::matN<T> diagonal(3, 3);
    diagonal(0, 0) = static_cast<T>(3);
    diagonal(1, 1) = static_cast<T>(1);
    diagonal(2, 2) = static_cast<T>(2);
    ez::math::eigenSym::solver<T> eigSolver;
    ez::math::eigenSym::result<T> output = eigSolver.compute(diagonal);
    CTEST_ASSERT(output.success);
    CTEST_ASSERT(ez::math::isEqual(output.d[0], static_cast<T>(3), static_cast<T>(1e-6)));
    CTEST_ASSERT(ez::math::isEqual(output.d[1], static_cast<T>(2), static_cast<T>(1e-6)));
    CTEST_ASSERT(ez::math::isEqual(output.d[2], static_cast<T>(1), static_cast<T>(1e-6)));
    return true;
}

template <typename T>
bool TestEzEigenSym_DiagonalWithNegative() {
    // diag(-2, 5, 1) -> sorted descending: [5, 1, -2]
    ez::math::matN<T> diagonal(3, 3);
    diagonal(0, 0) = static_cast<T>(-2);
    diagonal(1, 1) = static_cast<T>(5);
    diagonal(2, 2) = static_cast<T>(1);
    ez::math::eigenSym::solver<T> eigSolver;
    ez::math::eigenSym::result<T> output = eigSolver.compute(diagonal);
    CTEST_ASSERT(output.success);
    CTEST_ASSERT(ez::math::isEqual(output.d[0], static_cast<T>(5), static_cast<T>(1e-6)));
    CTEST_ASSERT(ez::math::isEqual(output.d[1], static_cast<T>(1), static_cast<T>(1e-6)));
    CTEST_ASSERT(ez::math::isEqual(output.d[2], static_cast<T>(-2), static_cast<T>(1e-6)));
    return true;
}

template <typename T>
bool TestEzEigenSym_Known2x2() {
    // [[2, 1], [1, 2]] has eigenvalues 3 and 1.
    ez::math::matN<T> matrix(2, 2);
    matrix(0, 0) = static_cast<T>(2); matrix(0, 1) = static_cast<T>(1);
    matrix(1, 0) = static_cast<T>(1); matrix(1, 1) = static_cast<T>(2);
    ez::math::eigenSym::solver<T> eigSolver;
    ez::math::eigenSym::result<T> output = eigSolver.compute(matrix);
    CTEST_ASSERT(output.success);
    CTEST_ASSERT(ez::math::isEqual(output.d[0], static_cast<T>(3), static_cast<T>(1e-6)));
    CTEST_ASSERT(ez::math::isEqual(output.d[1], static_cast<T>(1), static_cast<T>(1e-6)));
    return true;
}

template <typename T>
bool TestEzEigenSym_ReconstructionSymmetric3x3() {
    // Arbitrary symmetric 3x3 matrix.
    //   [4  1 -2]
    //   [1  2  3]
    //   [-2 3  5]
    ez::math::matN<T> matrix(3, 3);
    matrix(0, 0) = static_cast<T>(4);  matrix(0, 1) = static_cast<T>(1);  matrix(0, 2) = static_cast<T>(-2);
    matrix(1, 0) = static_cast<T>(1);  matrix(1, 1) = static_cast<T>(2);  matrix(1, 2) = static_cast<T>(3);
    matrix(2, 0) = static_cast<T>(-2); matrix(2, 1) = static_cast<T>(3);  matrix(2, 2) = static_cast<T>(5);

    ez::math::eigenSym::solver<T> eigSolver;
    ez::math::eigenSym::result<T> output = eigSolver.compute(matrix);
    CTEST_ASSERT(output.success);
    CTEST_ASSERT(output.d.size() == 3);

    ez::math::matN<T> reconstructed = reconstruct(output);
    CTEST_ASSERT(matrixIsEqual(matrix, reconstructed, static_cast<T>(1e-5)));
    return true;
}

template <typename T>
bool TestEzEigenSym_EigenvectorsAreOrthogonal() {
    // V * V^T should equal the identity matrix.
    ez::math::matN<T> matrix(3, 3);
    matrix(0, 0) = static_cast<T>(4);  matrix(0, 1) = static_cast<T>(1);  matrix(0, 2) = static_cast<T>(-2);
    matrix(1, 0) = static_cast<T>(1);  matrix(1, 1) = static_cast<T>(2);  matrix(1, 2) = static_cast<T>(3);
    matrix(2, 0) = static_cast<T>(-2); matrix(2, 1) = static_cast<T>(3);  matrix(2, 2) = static_cast<T>(5);

    ez::math::eigenSym::solver<T> eigSolver;
    ez::math::eigenSym::result<T> output = eigSolver.compute(matrix);
    CTEST_ASSERT(output.success);

    ez::math::matN<T> product = output.v * output.v.transpose();
    ez::math::matN<T> identity = ez::math::matN<T>::Identity(3);
    CTEST_ASSERT(matrixIsEqual(product, identity, static_cast<T>(1e-5)));
    return true;
}

template <typename T>
bool TestEzEigenSym_EigenvaluesAreSortedDescending() {
    ez::math::matN<T> matrix(3, 3);
    matrix(0, 0) = static_cast<T>(4);  matrix(0, 1) = static_cast<T>(1);  matrix(0, 2) = static_cast<T>(-2);
    matrix(1, 0) = static_cast<T>(1);  matrix(1, 1) = static_cast<T>(2);  matrix(1, 2) = static_cast<T>(3);
    matrix(2, 0) = static_cast<T>(-2); matrix(2, 1) = static_cast<T>(3);  matrix(2, 2) = static_cast<T>(5);

    ez::math::eigenSym::solver<T> eigSolver;
    ez::math::eigenSym::result<T> output = eigSolver.compute(matrix);
    CTEST_ASSERT(output.success);
    CTEST_ASSERT(output.d[0] >= output.d[1]);
    CTEST_ASSERT(output.d[1] >= output.d[2]);
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzEigenSym(const std::string& vTest) {
    IfTestExist(TestEzEigenSym_EmptyMatrix<float>);
    else IfTestExist(TestEzEigenSym_EmptyMatrix<double>);
    else IfTestExist(TestEzEigenSym_NonSquareMatrix<float>);
    else IfTestExist(TestEzEigenSym_NonSquareMatrix<double>);
    else IfTestExist(TestEzEigenSym_Identity3x3<float>);
    else IfTestExist(TestEzEigenSym_Identity3x3<double>);
    else IfTestExist(TestEzEigenSym_Diagonal<float>);
    else IfTestExist(TestEzEigenSym_Diagonal<double>);
    else IfTestExist(TestEzEigenSym_DiagonalWithNegative<float>);
    else IfTestExist(TestEzEigenSym_DiagonalWithNegative<double>);
    else IfTestExist(TestEzEigenSym_Known2x2<float>);
    else IfTestExist(TestEzEigenSym_Known2x2<double>);
    else IfTestExist(TestEzEigenSym_ReconstructionSymmetric3x3<float>);
    else IfTestExist(TestEzEigenSym_ReconstructionSymmetric3x3<double>);
    else IfTestExist(TestEzEigenSym_EigenvectorsAreOrthogonal<float>);
    else IfTestExist(TestEzEigenSym_EigenvectorsAreOrthogonal<double>);
    else IfTestExist(TestEzEigenSym_EigenvaluesAreSortedDescending<float>);
    else IfTestExist(TestEzEigenSym_EigenvaluesAreSortedDescending<double>);
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
