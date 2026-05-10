#include <ezlibs/ezMath/ezMath.hpp>
#include <ezlibs/ezCTest.hpp>
#include <cmath>
#include <limits>

// Desactivation des warnings de conversion
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244)  // Conversion from 'double' to 'float', possible loss of data
#pragma warning(disable : 4305)  // Truncation from 'double' to 'float'
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#endif

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

template <typename T>
bool TestEzVec2_Offset() {
    ez::math::vec2<T> v(1, 2);
    ez::math::vec2<T> result = v.Offset(1, 2);
    CTEST_ASSERT(ez::math::isEqual(result.x, static_cast<T>(2)));
    CTEST_ASSERT(ez::math::isEqual(result.y, static_cast<T>(4)));
    return true;
}

template <typename T>
bool TestEzVec2_Negate() {
    ez::math::vec2<T> v(1, 2);
    ez::math::vec2<T> result = -v;
    CTEST_ASSERT(ez::math::isEqual(result.x, static_cast<T>(-1)));
    CTEST_ASSERT(ez::math::isEqual(result.y, static_cast<T>(-2)));
    return true;
}

template <typename T>
bool TestEzVec2_Length() {
    ez::math::vec2<T> v(3, 4);
    CTEST_ASSERT(ez::math::isEqual(v.length(), static_cast<T>(5)));
    return true;
}

template <typename T>
bool TestEzVec2_Normalize() {
    ez::math::vec2<T> v(3, 4);
    v.normalize();
    CTEST_ASSERT(ez::math::isEqual(v.length(), static_cast<T>(1)));
    return true;
}

template <typename T>
bool TestEzVec2_Sum() {
    ez::math::vec2<T> v(1, 2);
    CTEST_ASSERT(ez::math::isEqual(v.sum(), static_cast<T>(3)));
    return true;
}

template <typename T>
bool TestEzVec2_OperatorAdd() {
    ez::math::vec2<T> v1(1, 2);
    ez::math::vec2<T> v2(3, 4);
    T scalar = 1;

    ez::math::vec2<T> result1 = v1 + scalar;
    CTEST_ASSERT(ez::math::isEqual(result1.x, static_cast<T>(2)));
    CTEST_ASSERT(ez::math::isEqual(result1.y, static_cast<T>(3)));

    ez::math::vec2<T> result2 = scalar + v1;
    CTEST_ASSERT(ez::math::isEqual(result2.x, static_cast<T>(2)));
    CTEST_ASSERT(ez::math::isEqual(result2.y, static_cast<T>(3)));

    ez::math::vec2<T> result3 = v1 + v2;
    CTEST_ASSERT(ez::math::isEqual(result3.x, static_cast<T>(4)));
    CTEST_ASSERT(ez::math::isEqual(result3.y, static_cast<T>(6)));

    return true;
}

template <typename T>
bool TestEzVec2_OperatorSubtract() {
    ez::math::vec2<T> v1(3, 4);
    ez::math::vec2<T> v2(1, 2);
    T scalar = 1;

    ez::math::vec2<T> result1 = v1 - scalar;
    CTEST_ASSERT(ez::math::isEqual(result1.x, static_cast<T>(2)));
    CTEST_ASSERT(ez::math::isEqual(result1.y, static_cast<T>(3)));

    ez::math::vec2<T> result2 = scalar - v1;
    CTEST_ASSERT(ez::math::isEqual(result2.x, static_cast<T>(-2)));
    CTEST_ASSERT(ez::math::isEqual(result2.y, static_cast<T>(-3)));

    ez::math::vec2<T> result3 = v1 - v2;
    CTEST_ASSERT(ez::math::isEqual(result3.x, static_cast<T>(2)));
    CTEST_ASSERT(ez::math::isEqual(result3.y, static_cast<T>(2)));

    return true;
}

template <typename T>
bool TestEzVec2_OperatorMultiply() {
    ez::math::vec2<T> v1(2, 3);
    ez::math::vec2<T> v2(4, 5);
    T scalar = 2;

    ez::math::vec2<T> result1 = v1 * scalar;
    CTEST_ASSERT(ez::math::isEqual(result1.x, static_cast<T>(4)));
    CTEST_ASSERT(ez::math::isEqual(result1.y, static_cast<T>(6)));

    ez::math::vec2<T> result2 = scalar * v1;
    CTEST_ASSERT(ez::math::isEqual(result2.x, static_cast<T>(4)));
    CTEST_ASSERT(ez::math::isEqual(result2.y, static_cast<T>(6)));

    ez::math::vec2<T> result3 = v1 * v2;
    CTEST_ASSERT(ez::math::isEqual(result3.x, static_cast<T>(8)));
    CTEST_ASSERT(ez::math::isEqual(result3.y, static_cast<T>(15)));

    v1 *= v2;
    CTEST_ASSERT(ez::math::isEqual(v1.x, static_cast<T>(8)));
    CTEST_ASSERT(ez::math::isEqual(v1.y, static_cast<T>(15)));

    return true;
}

template <typename T>
bool TestEzVec2_OperatorDivide() {
    ez::math::vec2<T> v1(4, 6);
    ez::math::vec2<T> v2(2, 3);
    T scalar = 2;

    ez::math::vec2<T> result1 = v1 / scalar;
    CTEST_ASSERT(ez::math::isEqual(result1.x, static_cast<T>(2)));
    CTEST_ASSERT(ez::math::isEqual(result1.y, static_cast<T>(3)));

    ez::math::vec2<T> result3 = v1 / v2;
    CTEST_ASSERT(ez::math::isEqual(result3.x, static_cast<T>(2)));
    CTEST_ASSERT(ez::math::isEqual(result3.y, static_cast<T>(2)));

    return true;
}

template <>
bool TestEzVec2_OperatorDivide<float>() {
    ez::math::vec2<float> v1(4.0f, 6.0f);
    ez::math::vec2<float> v2(2.0f, 3.0f);
    float scalar = 2.0f;

    ez::math::vec2<float> result2 = scalar / v1;
    CTEST_ASSERT(ez::math::isEqual(result2.x, 0.5f));
    CTEST_ASSERT(ez::math::isEqual(result2.y, 1.0f / 3.0f));

    return true;
}

template <>
bool TestEzVec2_OperatorDivide<double>() {
    ez::math::vec2<double> v1(4.0, 6.0);
    ez::math::vec2<double> v2(2.0, 3.0);
    double scalar = 2.0;

    ez::math::vec2<double> result2 = scalar / v1;
    CTEST_ASSERT(ez::math::isEqual(result2.x, 0.5));
    CTEST_ASSERT(ez::math::isEqual(result2.y, 1.0 / 3.0));

    return true;
}

template <typename T>
bool TestEzVec2_EmptyAND() {
    ez::math::vec2<T> v1(0, 0);
    ez::math::vec2<T> v2(1, 0);
    ez::math::vec2<T> v3(0, 1);
    ez::math::vec2<T> v4(1, 1);

    CTEST_ASSERT(v1.emptyAND());
    CTEST_ASSERT(!v2.emptyAND());
    CTEST_ASSERT(!v3.emptyAND());
    CTEST_ASSERT(!v4.emptyAND());

    return true;
}

template <typename T>
bool TestEzVec2_EmptyOR() {
    ez::math::vec2<T> v1(0, 0);
    ez::math::vec2<T> v2(1, 0);
    ez::math::vec2<T> v3(0, 1);
    ez::math::vec2<T> v4(1, 1);

    CTEST_ASSERT(v1.emptyOR());
    CTEST_ASSERT(v2.emptyOR());
    CTEST_ASSERT(v3.emptyOR());
    CTEST_ASSERT(!v4.emptyOR());

    return true;
}

template <typename T>
bool TestEzVec2_Min() {
    ez::math::vec2<T> v1(1, 2);
    CTEST_ASSERT(ez::math::isEqual(v1.min(), static_cast<T>(1)));
    ez::math::vec2<T> v2(2, 1);
    CTEST_ASSERT(ez::math::isEqual(v2.min(), static_cast<T>(1)));
    return true;
}

template <typename T>
bool TestEzVec2_Max() {
    ez::math::vec2<T> v1(1, 2);
    CTEST_ASSERT(ez::math::isEqual(v1.max(), static_cast<T>(2)));
    ez::math::vec2<T> v2(2, 1);
    CTEST_ASSERT(ez::math::isEqual(v2.max(), static_cast<T>(2)));
    return true;
}

template <typename T>
bool TestEzVec2_GetNormalized() {
    ez::math::vec2<T> v1(3, 4);
    ez::math::vec2<T> n1 = v1.GetNormalized();
    CTEST_ASSERT(ez::math::isEqual(n1.length(), static_cast<T>(1), static_cast<T>(0.00001)));
    ez::math::vec2<T> v2(0.000001, 0.000005);
    ez::math::vec2<T> n2 = v2.GetNormalized();
    CTEST_ASSERT(ez::math::isEqual(n2.length(), static_cast<T>(0), static_cast<T>(0.00001)));
    return true;
}

template <typename T>
bool TestEzVec2_ComparisonOperators() {
    ez::math::vec2<T> v1(1, 2);
    ez::math::vec2<T> v2(3, 4);
    ez::math::vec2<T> v3(1, 2);
    T scalar = 2;

    CTEST_ASSERT(v1 < v2);
    CTEST_ASSERT(v2 > v1);
    CTEST_ASSERT(v1 < scalar);
    CTEST_ASSERT(v2 > scalar);

    CTEST_ASSERT(v1 > v2);
    CTEST_ASSERT(v2 < v1);
    CTEST_ASSERT(v1 < scalar);
    CTEST_ASSERT(v2 > scalar);

    CTEST_ASSERT(v1 <= v2);
    CTEST_ASSERT(v1 <= v3);
    CTEST_ASSERT(v2 >= v1);
    CTEST_ASSERT(v1 <= scalar);
    CTEST_ASSERT(v1 <= scalar);

    CTEST_ASSERT(v1 <= v2);
    CTEST_ASSERT(v2 >= v1);
    CTEST_ASSERT(v2 >= scalar);

    CTEST_ASSERT(v1 == v3);
    CTEST_ASSERT(v1 != v2);

    CTEST_ASSERT(v1 != v2);
    CTEST_ASSERT(!v1 == v3);

    return true;
}

template <typename T>
bool TestEzVec2_Floor() {
    ez::math::vec2<T> v(1.5, 2.7);
    ez::math::vec2<T> floorResult = ez::math::floor<T>(v);
    CTEST_ASSERT(ez::math::isEqual(floorResult.x, static_cast<T>(1)));
    CTEST_ASSERT(ez::math::isEqual(floorResult.y, static_cast<T>(2)));
    return true;
}

template <typename T>
bool TestEzVec2_Fract() {
    ez::math::vec2<T> v(1.5, 2.7);
    ez::math::vec2<T> fractResult = ez::math::fract<T>(v);
    CTEST_ASSERT(ez::math::isEqual(fractResult.x, static_cast<T>(0.5), static_cast<T>(0.00001)));
    CTEST_ASSERT(ez::math::isEqual(fractResult.y, static_cast<T>(0.7), static_cast<T>(0.00001)));
    return true;
}

template <typename T>
bool TestEzVec2_Ceil() {
    ez::math::vec2<T> v(1.5, 2.7);
    ez::math::vec2<T> ceilResult = ez::math::ceil<T>(v);
    CTEST_ASSERT(ez::math::isEqual(ceilResult.x, static_cast<T>(2)));
    CTEST_ASSERT(ez::math::isEqual(ceilResult.y, static_cast<T>(3)));
    return true;
}

template <typename T>
bool TestEzVec2_Mini() {
    ez::math::vec2<T> v1(1, 3);
    ez::math::vec2<T> v2(2, 4);
    ez::math::vec2<T> miniResult = ez::math::mini<T>(v1, v2);
    CTEST_ASSERT(ez::math::isEqual(miniResult.x, static_cast<T>(1)));
    CTEST_ASSERT(ez::math::isEqual(miniResult.y, static_cast<T>(3)));
    return true;
}

template <typename T>
bool TestEzVec2_Maxi() {
    ez::math::vec2<T> v1(1, 3);
    ez::math::vec2<T> v2(2, 4);
    ez::math::vec2<T> maxiResult = ez::math::maxi<T>(v1, v2);
    CTEST_ASSERT(ez::math::isEqual(maxiResult.x, static_cast<T>(2)));
    CTEST_ASSERT(ez::math::isEqual(maxiResult.y, static_cast<T>(4)));
    return true;
}

template <typename T>
bool TestEzVec2_Dot() {
    ez::math::vec2<T> v1(1, 3);
    ez::math::vec2<T> v2(2, 4);
    T dotResult = ez::math::dot<T>(v1, v2);
    CTEST_ASSERT(ez::math::isEqual(dotResult, static_cast<T>(14)));
    return true;
}

template <typename T>
bool TestEzVec2_Det() {
    ez::math::vec2<T> v1(1, 3);
    ez::math::vec2<T> v2(2, 4);
    T detResult = ez::math::det<T>(v1, v2);
    CTEST_ASSERT(ez::math::isEqual(detResult, static_cast<T>(-2)));
    return true;
}

template <typename T>
bool TestEzVec2_Reflect() {
    ez::math::vec2<T> v1(1, 3);
    ez::math::vec2<T> v2(2, 4);
    ez::math::vec2<T> reflectResult = ez::math::reflect<T>(v1, v2);
    CTEST_ASSERT(ez::math::isEqual(reflectResult.x, static_cast<T>(-55.0)));
    CTEST_ASSERT(ez::math::isEqual(reflectResult.y, static_cast<T>(-109.0)));
    return true;
}

template <typename T>
bool TestEzVec2_Sign() {
    ez::math::vec2<T> v(5, 3);
    ez::math::vec2<T> signResult = ez::math::sign<T>(v);
    CTEST_ASSERT(ez::math::isEqual(signResult.x, static_cast<T>(1)));
    CTEST_ASSERT(ez::math::isEqual(signResult.y, static_cast<T>(1)));

    v = ez::math::vec2<T>(0, 0);
    signResult = ez::math::sign(v);
    CTEST_ASSERT(ez::math::isEqual(signResult.x, static_cast<T>(0)));
    CTEST_ASSERT(ez::math::isEqual(signResult.y, static_cast<T>(0)));

    return true;
}

template <typename T>
bool TestEzVec2_Sin() {
    ez::math::vec2<T> v(1.5, 2.7);
    ez::math::vec2<T> sinResult = ez::math::sin<T>(v);
    CTEST_ASSERT(ez::math::isEqual(sinResult.x, std::sin(static_cast<T>(1.5))));
    CTEST_ASSERT(ez::math::isEqual(sinResult.y, std::sin(static_cast<T>(2.7))));
    return true;
}

template <typename T>
bool TestEzVec2_Cos() {
    ez::math::vec2<T> v(1.5, 2.7);
    ez::math::vec2<T> cosResult = ez::math::cos<T>(v);
    CTEST_ASSERT(ez::math::isEqual(cosResult.x, std::cos(static_cast<T>(1.5))));
    CTEST_ASSERT(ez::math::isEqual(cosResult.y, std::cos(static_cast<T>(2.7))));
    return true;
}

template <typename T>
bool TestEzVec2_Tan() {
    ez::math::vec2<T> v(1.5, 2.7);
    ez::math::vec2<T> tanResult = ez::math::tan<T>(v);
    CTEST_ASSERT(ez::math::isEqual(tanResult.x, std::tan(static_cast<T>(1.5))));
    CTEST_ASSERT(ez::math::isEqual(tanResult.y, std::tan(static_cast<T>(2.7))));
    return true;
}

template <typename T>
bool TestEzVec2_Atan() {
    ez::math::vec2<T> v(1.5, 2.7);
    ez::math::vec2<T> atanResult = ez::math::atan<T>(v);
    CTEST_ASSERT(ez::math::isEqual(atanResult.x, std::atan(static_cast<T>(1.5))));
    CTEST_ASSERT(ez::math::isEqual(atanResult.y, std::atan(static_cast<T>(2.7))));
    return true;
}

template <typename T>
bool TestEzVec2_SquaredDistanceToSegment() {
    const T epsilon = static_cast<T>(1e-5);
    // Point ON segment endpoint A → 0
    {
        ez::math::vec2<T> a(0, 0);
        ez::math::vec2<T> b(10, 0);
        T result = ez::math::squaredDistanceToSegment<T>(a, a, b);
        CTEST_ASSERT(ez::math::isEqual<T>(result, static_cast<T>(0), epsilon));
    }
    // Point ON segment endpoint B → 0
    {
        ez::math::vec2<T> a(0, 0);
        ez::math::vec2<T> b(10, 0);
        T result = ez::math::squaredDistanceToSegment<T>(b, a, b);
        CTEST_ASSERT(ez::math::isEqual<T>(result, static_cast<T>(0), epsilon));
    }
    // Point ON segment middle → 0
    {
        ez::math::vec2<T> a(0, 0);
        ez::math::vec2<T> b(10, 0);
        ez::math::vec2<T> p(5, 0);
        T result = ez::math::squaredDistanceToSegment<T>(p, a, b);
        CTEST_ASSERT(ez::math::isEqual<T>(result, static_cast<T>(0), epsilon));
    }
    // Point perpendicular to mid-segment, perp distance 3 → squared = 9
    {
        ez::math::vec2<T> a(0, 0);
        ez::math::vec2<T> b(10, 0);
        ez::math::vec2<T> p(5, 3);
        T result = ez::math::squaredDistanceToSegment<T>(p, a, b);
        CTEST_ASSERT(ez::math::isEqual<T>(result, static_cast<T>(9), epsilon));
    }
    // Point past A: nearest is A → squared = 25
    {
        ez::math::vec2<T> a(0, 0);
        ez::math::vec2<T> b(10, 0);
        ez::math::vec2<T> p(-5, 0);
        T result = ez::math::squaredDistanceToSegment<T>(p, a, b);
        CTEST_ASSERT(ez::math::isEqual<T>(result, static_cast<T>(25), epsilon));
    }
    // Point past B: nearest is B → squared = 25 + 16 = 41
    {
        ez::math::vec2<T> a(0, 0);
        ez::math::vec2<T> b(10, 0);
        ez::math::vec2<T> p(15, 4);
        T result = ez::math::squaredDistanceToSegment<T>(p, a, b);
        CTEST_ASSERT(ez::math::isEqual<T>(result, static_cast<T>(41), epsilon));
    }
    // Degenerate segment (A == B): plain point-to-point squared distance
    // (5-2)^2 + (7-3)^2 = 9 + 16 = 25
    {
        ez::math::vec2<T> a(2, 3);
        ez::math::vec2<T> p(5, 7);
        T result = ez::math::squaredDistanceToSegment<T>(p, a, a);
        CTEST_ASSERT(ez::math::isEqual<T>(result, static_cast<T>(25), epsilon));
    }
    // Diagonal segment: point at perpendicular distance sqrt(2) → squared = 2
    {
        ez::math::vec2<T> a(0, 0);
        ez::math::vec2<T> b(10, 10);
        ez::math::vec2<T> p(0, 2);  // perp foot at (1, 1), delta = (-1, 1), |delta|^2 = 2
        T result = ez::math::squaredDistanceToSegment<T>(p, a, b);
        CTEST_ASSERT(ez::math::isEqual<T>(result, static_cast<T>(2), epsilon));
    }
    return true;
}

template <typename T>
bool TestEzVec2_DistanceToSegment() {
    const T epsilon = static_cast<T>(1e-5);
    // Perpendicular distance 3 → distance 3
    {
        ez::math::vec2<T> a(0, 0);
        ez::math::vec2<T> b(10, 0);
        ez::math::vec2<T> p(5, 3);
        T result = ez::math::distanceToSegment<T>(p, a, b);
        CTEST_ASSERT(ez::math::isEqual<T>(result, static_cast<T>(3), epsilon));
    }
    // Past A by 5 → distance 5
    {
        ez::math::vec2<T> a(0, 0);
        ez::math::vec2<T> b(10, 0);
        ez::math::vec2<T> p(-5, 0);
        T result = ez::math::distanceToSegment<T>(p, a, b);
        CTEST_ASSERT(ez::math::isEqual<T>(result, static_cast<T>(5), epsilon));
    }
    // 3-4-5 triangle past B → distance 5
    {
        ez::math::vec2<T> a(0, 0);
        ez::math::vec2<T> b(10, 0);
        ez::math::vec2<T> p(13, 4);
        T result = ez::math::distanceToSegment<T>(p, a, b);
        CTEST_ASSERT(ez::math::isEqual<T>(result, static_cast<T>(5), epsilon));
    }
    return true;
}

#define IfTestExist(v)            \
    if (vTest == std::string(#v)) \
    return v()

bool TestEzVec2(const std::string& vTest) {
    IfTestExist(TestEzVec2_Offset<float>);
    else IfTestExist(TestEzVec2_Offset<double>);
    else IfTestExist(TestEzVec2_Offset<int32_t>);
    else IfTestExist(TestEzVec2_Offset<int64_t>);
    else IfTestExist(TestEzVec2_Offset<uint32_t>);
    else IfTestExist(TestEzVec2_Offset<uint64_t>);

    IfTestExist(TestEzVec2_Negate<float>);
    else IfTestExist(TestEzVec2_Negate<double>);
    else IfTestExist(TestEzVec2_Negate<int32_t>);
    else IfTestExist(TestEzVec2_Negate<int64_t>);

    IfTestExist(TestEzVec2_Length<float>);
    else IfTestExist(TestEzVec2_Length<double>);

    IfTestExist(TestEzVec2_Normalize<float>);
    else IfTestExist(TestEzVec2_Normalize<double>);

    IfTestExist(TestEzVec2_Sum<float>);
    else IfTestExist(TestEzVec2_Sum<double>);
    else IfTestExist(TestEzVec2_Sum<int32_t>);
    else IfTestExist(TestEzVec2_Sum<int64_t>);
    else IfTestExist(TestEzVec2_Sum<uint32_t>);
    else IfTestExist(TestEzVec2_Sum<uint64_t>);

    IfTestExist(TestEzVec2_OperatorAdd<float>);
    else IfTestExist(TestEzVec2_OperatorAdd<double>);
    else IfTestExist(TestEzVec2_OperatorAdd<int32_t>);
    else IfTestExist(TestEzVec2_OperatorAdd<int64_t>);
    else IfTestExist(TestEzVec2_OperatorAdd<uint32_t>);
    else IfTestExist(TestEzVec2_OperatorAdd<uint64_t>);

    IfTestExist(TestEzVec2_OperatorSubtract<float>);
    else IfTestExist(TestEzVec2_OperatorSubtract<double>);
    else IfTestExist(TestEzVec2_OperatorSubtract<int32_t>);
    else IfTestExist(TestEzVec2_OperatorSubtract<int64_t>);
    else IfTestExist(TestEzVec2_OperatorSubtract<uint32_t>);
    else IfTestExist(TestEzVec2_OperatorSubtract<uint64_t>);

    IfTestExist(TestEzVec2_OperatorMultiply<float>);
    else IfTestExist(TestEzVec2_OperatorMultiply<double>);
    else IfTestExist(TestEzVec2_OperatorMultiply<int32_t>);
    else IfTestExist(TestEzVec2_OperatorMultiply<int64_t>);
    else IfTestExist(TestEzVec2_OperatorMultiply<uint32_t>);
    else IfTestExist(TestEzVec2_OperatorMultiply<uint64_t>);

    IfTestExist(TestEzVec2_OperatorDivide<float>);
    else IfTestExist(TestEzVec2_OperatorDivide<double>);
    else IfTestExist(TestEzVec2_OperatorDivide<int32_t>);
    else IfTestExist(TestEzVec2_OperatorDivide<int64_t>);
    else IfTestExist(TestEzVec2_OperatorDivide<uint32_t>);
    else IfTestExist(TestEzVec2_OperatorDivide<uint64_t>);

    IfTestExist(TestEzVec2_EmptyAND<float>);
    else IfTestExist(TestEzVec2_EmptyAND<double>);
    else IfTestExist(TestEzVec2_EmptyAND<int32_t>);
    else IfTestExist(TestEzVec2_EmptyAND<int64_t>);
    else IfTestExist(TestEzVec2_EmptyAND<uint32_t>);
    else IfTestExist(TestEzVec2_EmptyAND<uint64_t>);

    IfTestExist(TestEzVec2_EmptyOR<float>);
    else IfTestExist(TestEzVec2_EmptyOR<double>);
    else IfTestExist(TestEzVec2_EmptyOR<int32_t>);
    else IfTestExist(TestEzVec2_EmptyOR<int64_t>);
    else IfTestExist(TestEzVec2_EmptyOR<uint32_t>);
    else IfTestExist(TestEzVec2_EmptyOR<uint64_t>);

    IfTestExist(TestEzVec2_Min<float>);
    else IfTestExist(TestEzVec2_Min<double>);
    else IfTestExist(TestEzVec2_Min<int32_t>);
    else IfTestExist(TestEzVec2_Min<int64_t>);
    else IfTestExist(TestEzVec2_Min<uint32_t>);
    else IfTestExist(TestEzVec2_Min<uint64_t>);

    IfTestExist(TestEzVec2_Max<float>);
    else IfTestExist(TestEzVec2_Max<double>);
    else IfTestExist(TestEzVec2_Max<int32_t>);
    else IfTestExist(TestEzVec2_Max<int64_t>);
    else IfTestExist(TestEzVec2_Max<uint32_t>);
    else IfTestExist(TestEzVec2_Max<uint64_t>);

    IfTestExist(TestEzVec2_GetNormalized<float>);
    else IfTestExist(TestEzVec2_GetNormalized<double>);

    IfTestExist(TestEzVec2_Floor<float>);
    else IfTestExist(TestEzVec2_Floor<double>);

    IfTestExist(TestEzVec2_Fract<float>);
    else IfTestExist(TestEzVec2_Fract<double>);

    IfTestExist(TestEzVec2_Ceil<float>);
    else IfTestExist(TestEzVec2_Ceil<double>);

    IfTestExist(TestEzVec2_Mini<float>);
    else IfTestExist(TestEzVec2_Mini<double>);
    else IfTestExist(TestEzVec2_Mini<int32_t>);
    else IfTestExist(TestEzVec2_Mini<int64_t>);
    else IfTestExist(TestEzVec2_Mini<uint32_t>);
    else IfTestExist(TestEzVec2_Mini<uint64_t>);

    IfTestExist(TestEzVec2_Maxi<float>);
    else IfTestExist(TestEzVec2_Maxi<double>);
    else IfTestExist(TestEzVec2_Maxi<int32_t>);
    else IfTestExist(TestEzVec2_Maxi<int64_t>);
    else IfTestExist(TestEzVec2_Maxi<uint32_t>);
    else IfTestExist(TestEzVec2_Maxi<uint64_t>);

    IfTestExist(TestEzVec2_Dot<float>);
    else IfTestExist(TestEzVec2_Dot<double>);
    else IfTestExist(TestEzVec2_Dot<int32_t>);
    else IfTestExist(TestEzVec2_Dot<int64_t>);
    else IfTestExist(TestEzVec2_Dot<uint32_t>);
    else IfTestExist(TestEzVec2_Dot<uint64_t>);

    IfTestExist(TestEzVec2_Det<float>);
    else IfTestExist(TestEzVec2_Det<double>);
    else IfTestExist(TestEzVec2_Det<int32_t>);
    else IfTestExist(TestEzVec2_Det<int64_t>);
    else IfTestExist(TestEzVec2_Det<uint32_t>);
    else IfTestExist(TestEzVec2_Det<uint64_t>);

    IfTestExist(TestEzVec2_Reflect<float>);
    else IfTestExist(TestEzVec2_Reflect<double>);

    IfTestExist(TestEzVec2_Sign<float>);
    else IfTestExist(TestEzVec2_Sign<double>);
    else IfTestExist(TestEzVec2_Sign<int32_t>);
    else IfTestExist(TestEzVec2_Sign<int64_t>);

    IfTestExist(TestEzVec2_Sin<float>);
    else IfTestExist(TestEzVec2_Sin<double>);

    IfTestExist(TestEzVec2_Cos<float>);
    else IfTestExist(TestEzVec2_Cos<double>);

    IfTestExist(TestEzVec2_Tan<float>);
    else IfTestExist(TestEzVec2_Tan<double>);

    IfTestExist(TestEzVec2_Atan<float>);
    else IfTestExist(TestEzVec2_Atan<double>);

    IfTestExist(TestEzVec2_SquaredDistanceToSegment<float>);
    else IfTestExist(TestEzVec2_SquaredDistanceToSegment<double>);

    IfTestExist(TestEzVec2_DistanceToSegment<float>);
    else IfTestExist(TestEzVec2_DistanceToSegment<double>);

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

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
