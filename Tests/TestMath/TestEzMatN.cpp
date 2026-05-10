#include <ezlibs/ezMath/ezMatN.hpp>
#include <ezlibs/ezMath/ezMath.hpp>
#include <ezlibs/ezCTest.hpp>

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
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

template <typename T>
bool TestEzMatN_DefaultConstructor() {
    ez::math::matN<T> matrix;
    CTEST_ASSERT(matrix.rows() == 0);
    CTEST_ASSERT(matrix.columns() == 0);
    CTEST_ASSERT(matrix.size() == 0);
    CTEST_ASSERT(matrix.empty());
    return true;
}

template <typename T>
bool TestEzMatN_DimensionConstructor() {
    ez::math::matN<T> matrix(3, 4);
    CTEST_ASSERT(matrix.rows() == 3);
    CTEST_ASSERT(matrix.columns() == 4);
    CTEST_ASSERT(matrix.size() == 12);
    CTEST_ASSERT(!matrix.empty());
    for (std::size_t rowIndex = 0; rowIndex < 3; ++rowIndex) {
        for (std::size_t columnIndex = 0; columnIndex < 4; ++columnIndex) {
            CTEST_ASSERT(ez::math::isEqual(matrix(rowIndex, columnIndex), static_cast<T>(0)));
        }
    }
    return true;
}

template <typename T>
bool TestEzMatN_DimensionConstructorWithFillValue() {
    ez::math::matN<T> matrix(2, 3, static_cast<T>(7));
    CTEST_ASSERT(matrix.rows() == 2);
    CTEST_ASSERT(matrix.columns() == 3);
    for (std::size_t rowIndex = 0; rowIndex < 2; ++rowIndex) {
        for (std::size_t columnIndex = 0; columnIndex < 3; ++columnIndex) {
            CTEST_ASSERT(ez::math::isEqual(matrix(rowIndex, columnIndex), static_cast<T>(7)));
        }
    }
    return true;
}

template <typename T>
bool TestEzMatN_Identity() {
    ez::math::matN<T> matrix = ez::math::matN<T>::Identity(3);
    CTEST_ASSERT(matrix.rows() == 3);
    CTEST_ASSERT(matrix.columns() == 3);
    for (std::size_t rowIndex = 0; rowIndex < 3; ++rowIndex) {
        for (std::size_t columnIndex = 0; columnIndex < 3; ++columnIndex) {
            T expected = (rowIndex == columnIndex) ? static_cast<T>(1) : static_cast<T>(0);
            CTEST_ASSERT(ez::math::isEqual(matrix(rowIndex, columnIndex), expected));
        }
    }
    return true;
}

template <typename T>
bool TestEzMatN_Zero() {
    ez::math::matN<T> matrix = ez::math::matN<T>::Zero(2, 4);
    CTEST_ASSERT(matrix.rows() == 2);
    CTEST_ASSERT(matrix.columns() == 4);
    for (std::size_t rowIndex = 0; rowIndex < 2; ++rowIndex) {
        for (std::size_t columnIndex = 0; columnIndex < 4; ++columnIndex) {
            CTEST_ASSERT(ez::math::isEqual(matrix(rowIndex, columnIndex), static_cast<T>(0)));
        }
    }
    return true;
}

template <typename T>
bool TestEzMatN_AccessAndAssign() {
    ez::math::matN<T> matrix(2, 3);
    matrix(0, 0) = static_cast<T>(1);
    matrix(0, 1) = static_cast<T>(2);
    matrix(0, 2) = static_cast<T>(3);
    matrix(1, 0) = static_cast<T>(4);
    matrix(1, 1) = static_cast<T>(5);
    matrix(1, 2) = static_cast<T>(6);
    CTEST_ASSERT(ez::math::isEqual(matrix(0, 0), static_cast<T>(1)));
    CTEST_ASSERT(ez::math::isEqual(matrix(0, 1), static_cast<T>(2)));
    CTEST_ASSERT(ez::math::isEqual(matrix(0, 2), static_cast<T>(3)));
    CTEST_ASSERT(ez::math::isEqual(matrix(1, 0), static_cast<T>(4)));
    CTEST_ASSERT(ez::math::isEqual(matrix(1, 1), static_cast<T>(5)));
    CTEST_ASSERT(ez::math::isEqual(matrix(1, 2), static_cast<T>(6)));
    return true;
}

template <typename T>
bool TestEzMatN_Transpose() {
    // 2x3 matrix:
    //   [1 2 3]
    //   [4 5 6]
    ez::math::matN<T> matrix(2, 3);
    matrix(0, 0) = static_cast<T>(1);
    matrix(0, 1) = static_cast<T>(2);
    matrix(0, 2) = static_cast<T>(3);
    matrix(1, 0) = static_cast<T>(4);
    matrix(1, 1) = static_cast<T>(5);
    matrix(1, 2) = static_cast<T>(6);

    ez::math::matN<T> transposed = matrix.transpose();
    CTEST_ASSERT(transposed.rows() == 3);
    CTEST_ASSERT(transposed.columns() == 2);
    CTEST_ASSERT(ez::math::isEqual(transposed(0, 0), static_cast<T>(1)));
    CTEST_ASSERT(ez::math::isEqual(transposed(1, 0), static_cast<T>(2)));
    CTEST_ASSERT(ez::math::isEqual(transposed(2, 0), static_cast<T>(3)));
    CTEST_ASSERT(ez::math::isEqual(transposed(0, 1), static_cast<T>(4)));
    CTEST_ASSERT(ez::math::isEqual(transposed(1, 1), static_cast<T>(5)));
    CTEST_ASSERT(ez::math::isEqual(transposed(2, 1), static_cast<T>(6)));
    return true;
}

template <typename T>
bool TestEzMatN_Multiplication() {
    // (2x3) * (3x2) = (2x2)
    //   [1 2 3]   [ 7  8]   [ 58  64]
    //   [4 5 6] * [ 9 10] = [139 154]
    //             [11 12]
    ez::math::matN<T> left(2, 3);
    left(0, 0) = static_cast<T>(1); left(0, 1) = static_cast<T>(2); left(0, 2) = static_cast<T>(3);
    left(1, 0) = static_cast<T>(4); left(1, 1) = static_cast<T>(5); left(1, 2) = static_cast<T>(6);

    ez::math::matN<T> right(3, 2);
    right(0, 0) = static_cast<T>(7);  right(0, 1) = static_cast<T>(8);
    right(1, 0) = static_cast<T>(9);  right(1, 1) = static_cast<T>(10);
    right(2, 0) = static_cast<T>(11); right(2, 1) = static_cast<T>(12);

    ez::math::matN<T> product = left * right;
    CTEST_ASSERT(product.rows() == 2);
    CTEST_ASSERT(product.columns() == 2);
    CTEST_ASSERT(ez::math::isEqual(product(0, 0), static_cast<T>(58)));
    CTEST_ASSERT(ez::math::isEqual(product(0, 1), static_cast<T>(64)));
    CTEST_ASSERT(ez::math::isEqual(product(1, 0), static_cast<T>(139)));
    CTEST_ASSERT(ez::math::isEqual(product(1, 1), static_cast<T>(154)));
    return true;
}

template <typename T>
bool TestEzMatN_MultiplicationDimensionMismatch() {
    // (2x3) * (4x2) → cannot multiply (3 != 4), expect empty result
    ez::math::matN<T> left(2, 3, static_cast<T>(1));
    ez::math::matN<T> right(4, 2, static_cast<T>(1));
    ez::math::matN<T> product = left * right;
    CTEST_ASSERT(product.empty());
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzMatN(const std::string& vTest) {
    IfTestExist(TestEzMatN_DefaultConstructor<float>);
    else IfTestExist(TestEzMatN_DefaultConstructor<double>);
    else IfTestExist(TestEzMatN_DimensionConstructor<float>);
    else IfTestExist(TestEzMatN_DimensionConstructor<double>);
    else IfTestExist(TestEzMatN_DimensionConstructorWithFillValue<float>);
    else IfTestExist(TestEzMatN_DimensionConstructorWithFillValue<double>);
    else IfTestExist(TestEzMatN_Identity<float>);
    else IfTestExist(TestEzMatN_Identity<double>);
    else IfTestExist(TestEzMatN_Zero<float>);
    else IfTestExist(TestEzMatN_Zero<double>);
    else IfTestExist(TestEzMatN_AccessAndAssign<float>);
    else IfTestExist(TestEzMatN_AccessAndAssign<double>);
    else IfTestExist(TestEzMatN_Transpose<float>);
    else IfTestExist(TestEzMatN_Transpose<double>);
    else IfTestExist(TestEzMatN_Multiplication<float>);
    else IfTestExist(TestEzMatN_Multiplication<double>);
    else IfTestExist(TestEzMatN_MultiplicationDimensionMismatch<float>);
    else IfTestExist(TestEzMatN_MultiplicationDimensionMismatch<double>);
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
