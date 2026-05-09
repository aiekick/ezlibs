#include <ezlibs/ezVecN.hpp>
#include <ezlibs/ezMath.hpp>
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
bool TestEzVecN_DefaultConstructor() {
    ez::vecN<T> vector;
    CTEST_ASSERT(vector.size() == 0);
    CTEST_ASSERT(vector.empty());
    return true;
}

template <typename T>
bool TestEzVecN_SizeConstructor() {
    ez::vecN<T> vector(4);
    CTEST_ASSERT(vector.size() == 4);
    CTEST_ASSERT(!vector.empty());
    for (std::size_t componentIndex = 0; componentIndex < 4; ++componentIndex) {
        CTEST_ASSERT(ez::isEqual(vector[componentIndex], static_cast<T>(0)));
    }
    return true;
}

template <typename T>
bool TestEzVecN_SizeConstructorWithFillValue() {
    ez::vecN<T> vector(3, static_cast<T>(5));
    CTEST_ASSERT(vector.size() == 3);
    for (std::size_t componentIndex = 0; componentIndex < 3; ++componentIndex) {
        CTEST_ASSERT(ez::isEqual(vector[componentIndex], static_cast<T>(5)));
    }
    return true;
}

template <typename T>
bool TestEzVecN_InitializerListConstructor() {
    ez::vecN<T> vector{static_cast<T>(1), static_cast<T>(2), static_cast<T>(3)};
    CTEST_ASSERT(vector.size() == 3);
    CTEST_ASSERT(ez::isEqual(vector[0], static_cast<T>(1)));
    CTEST_ASSERT(ez::isEqual(vector[1], static_cast<T>(2)));
    CTEST_ASSERT(ez::isEqual(vector[2], static_cast<T>(3)));
    return true;
}

template <typename T>
bool TestEzVecN_Zero() {
    ez::vecN<T> vector = ez::vecN<T>::Zero(5);
    CTEST_ASSERT(vector.size() == 5);
    for (std::size_t componentIndex = 0; componentIndex < 5; ++componentIndex) {
        CTEST_ASSERT(ez::isEqual(vector[componentIndex], static_cast<T>(0)));
    }
    return true;
}

template <typename T>
bool TestEzVecN_AccessAndAssign() {
    ez::vecN<T> vector(3);
    vector[0] = static_cast<T>(10);
    vector[1] = static_cast<T>(20);
    vector[2] = static_cast<T>(30);
    CTEST_ASSERT(ez::isEqual(vector[0], static_cast<T>(10)));
    CTEST_ASSERT(ez::isEqual(vector[1], static_cast<T>(20)));
    CTEST_ASSERT(ez::isEqual(vector[2], static_cast<T>(30)));
    return true;
}

template <typename T>
bool TestEzVecN_LengthSquared() {
    ez::vecN<T> vector{static_cast<T>(3), static_cast<T>(4)};
    CTEST_ASSERT(ez::isEqual(vector.lengthSquared(), static_cast<T>(25)));
    return true;
}

template <typename T>
bool TestEzVecN_Length() {
    ez::vecN<T> vector{static_cast<T>(3), static_cast<T>(4)};
    CTEST_ASSERT(ez::isEqual(vector.length(), static_cast<T>(5)));
    return true;
}

template <typename T>
bool TestEzVecN_DotProduct() {
    ez::vecN<T> left{static_cast<T>(1), static_cast<T>(2), static_cast<T>(3)};
    ez::vecN<T> right{static_cast<T>(4), static_cast<T>(5), static_cast<T>(6)};
    // 1*4 + 2*5 + 3*6 = 32
    CTEST_ASSERT(ez::isEqual(left.dot(right), static_cast<T>(32)));
    return true;
}

template <typename T>
bool TestEzVecN_DotProductDimensionMismatch() {
    ez::vecN<T> left(3, static_cast<T>(1));
    ez::vecN<T> right(4, static_cast<T>(1));
    // Mismatched sizes return 0
    CTEST_ASSERT(ez::isEqual(left.dot(right), static_cast<T>(0)));
    return true;
}

template <typename T>
bool TestEzVecN_Normalize() {
    ez::vecN<T> vector{static_cast<T>(3), static_cast<T>(4)};
    bool normalized = vector.normalize();
    CTEST_ASSERT(normalized);
    CTEST_ASSERT(ez::isEqual(vector.length(), static_cast<T>(1), static_cast<T>(1e-6)));
    return true;
}

template <typename T>
bool TestEzVecN_NormalizeNullVector() {
    ez::vecN<T> vector{static_cast<T>(0), static_cast<T>(0), static_cast<T>(0)};
    bool normalized = vector.normalize();
    CTEST_ASSERT(!normalized);
    // Vector should be left untouched
    CTEST_ASSERT(ez::isEqual(vector[0], static_cast<T>(0)));
    CTEST_ASSERT(ez::isEqual(vector[1], static_cast<T>(0)));
    CTEST_ASSERT(ez::isEqual(vector[2], static_cast<T>(0)));
    return true;
}

template <typename T>
bool TestEzVecN_Normalized() {
    ez::vecN<T> source{static_cast<T>(3), static_cast<T>(4)};
    ez::vecN<T> result = source.normalized();
    // Result has unit length
    CTEST_ASSERT(ez::isEqual(result.length(), static_cast<T>(1), static_cast<T>(1e-6)));
    // Source is untouched
    CTEST_ASSERT(ez::isEqual(source[0], static_cast<T>(3)));
    CTEST_ASSERT(ez::isEqual(source[1], static_cast<T>(4)));
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzVecN(const std::string& vTest) {
    IfTestExist(TestEzVecN_DefaultConstructor<float>);
    else IfTestExist(TestEzVecN_DefaultConstructor<double>);
    else IfTestExist(TestEzVecN_SizeConstructor<float>);
    else IfTestExist(TestEzVecN_SizeConstructor<double>);
    else IfTestExist(TestEzVecN_SizeConstructorWithFillValue<float>);
    else IfTestExist(TestEzVecN_SizeConstructorWithFillValue<double>);
    else IfTestExist(TestEzVecN_InitializerListConstructor<float>);
    else IfTestExist(TestEzVecN_InitializerListConstructor<double>);
    else IfTestExist(TestEzVecN_Zero<float>);
    else IfTestExist(TestEzVecN_Zero<double>);
    else IfTestExist(TestEzVecN_AccessAndAssign<float>);
    else IfTestExist(TestEzVecN_AccessAndAssign<double>);
    else IfTestExist(TestEzVecN_LengthSquared<float>);
    else IfTestExist(TestEzVecN_LengthSquared<double>);
    else IfTestExist(TestEzVecN_Length<float>);
    else IfTestExist(TestEzVecN_Length<double>);
    else IfTestExist(TestEzVecN_DotProduct<float>);
    else IfTestExist(TestEzVecN_DotProduct<double>);
    else IfTestExist(TestEzVecN_DotProductDimensionMismatch<float>);
    else IfTestExist(TestEzVecN_DotProductDimensionMismatch<double>);
    else IfTestExist(TestEzVecN_Normalize<float>);
    else IfTestExist(TestEzVecN_Normalize<double>);
    else IfTestExist(TestEzVecN_NormalizeNullVector<float>);
    else IfTestExist(TestEzVecN_NormalizeNullVector<double>);
    else IfTestExist(TestEzVecN_Normalized<float>);
    else IfTestExist(TestEzVecN_Normalized<double>);
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
