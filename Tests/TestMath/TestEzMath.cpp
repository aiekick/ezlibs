#include <TestEzMath.h>
#include <ezlibs/ezMath/ezMath.hpp>
#include <cmath>
#include <limits>

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

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

bool TestEzMath_RoundN() {
    if (ez::math::round_n(1.23456, 2) != "1.23")  // double
        return false;
    if (ez::math::round_n(1.23456f, 2) != "1.23")  // float
        return false;
    if (ez::math::round_n(1.23456, 4) != "1.2346")  // double
        return false;
    if (ez::math::round_n(1.23456f, 4) != "1.2346")  // float
        return false;
    if (ez::math::round_n(1.2, 1) != "1.2")  // double
        return false;
    if (ez::math::round_n(1.2f, 1) != "1.2")  // float
        return false;
    if (ez::math::round_n(1.2, 2) != "1.20")  // double
        return false;
    if (ez::math::round_n(1.2f, 2) != "1.20")  // float
        return false;
    if (ez::math::round_n(0.0000, 2) != "0.00")  // double
        return false;
    if (ez::math::round_n(0.0000f, 2) != "0.00")  // float
        return false;
    if (ez::math::round_n(0.0000, 0) != "0")  // double
        return false;
    if (ez::math::round_n(0.0000f, 0) != "0")  // float
        return false;
    return true;
}

bool TestEzMath_FloatIsValid() {
    if (!ez::math::floatIsValid(1.0f))
        return false;
    if (!ez::math::floatIsValid(0.0f))
        return false;
    if (!ez::math::floatIsValid(-1.0f))
        return false;
    if (ez::math::floatIsValid(std::numeric_limits<float>::infinity()))
        return false;
    if (ez::math::floatIsValid(std::numeric_limits<float>::quiet_NaN()))
        return false;
    return true;
}

template <typename T>
bool TestEzMath_IsDifferent() {
    if (!ez::math::isDifferent<T>(-5, 5))
        return false;
    if (!ez::math::isDifferent<T>(0, 5))
        return false;
    if (ez::math::isDifferent<T>(-0, 0))
        return false;
    if (!ez::math::isDifferent<T>(1, 1 + 1))
        return false;
    if (!ez::math::isDifferent<T>(1, 1 - 1))
        return false;
    return true;
}

template <>
bool TestEzMath_IsDifferent<uint32_t>() {
    if (!ez::math::isDifferent<uint32_t>(0, 5))
        return false;
    if (ez::math::isDifferent<uint32_t>(-0, 0))
        return false;
    if (!ez::math::isDifferent<uint32_t>(1, 1 + 1))
        return false;
    if (!ez::math::isDifferent<uint32_t>(1, 1 - 1))
        return false;
    return true;
}

template <>
bool TestEzMath_IsDifferent<uint64_t>() {
    if (!ez::math::isDifferent<uint64_t>(0, 5))
        return false;
    if (ez::math::isDifferent<uint64_t>(-0, 0))
        return false;
    if (!ez::math::isDifferent<uint64_t>(1, 1 + 1))
        return false;
    if (!ez::math::isDifferent<uint64_t>(1, 1 - 1))
        return false;
    return true;
}

template <>
bool TestEzMath_IsDifferent<float>() {
    if (!ez::math::isDifferent<float>(-5.0f, 5.0f))
        return false;
    if (!ez::math::isDifferent<float>(0.0f, 5.0f))
        return false;
    if (ez::math::isDifferent<float>(-0.0f, 0.0f))
        return false;
    if (!ez::math::isDifferent<float>(1.0f, 1.0f + std::numeric_limits<float>::epsilon() * 2.0f))
        return false;
    if (!ez::math::isDifferent<float>(1.0f, 1.0f - std::numeric_limits<float>::epsilon() * 2.0f))
        return false;
    return true;
}

template <>
bool TestEzMath_IsDifferent<double>() {
    if (!ez::math::isDifferent<double>(-5.0, 5.0))
        return false;
    if (!ez::math::isDifferent<double>(0.0, 5.0))
        return false;
    if (ez::math::isDifferent<double>(-0.0, 0.0))
        return false;
    if (!ez::math::isDifferent<double>(1.0, 1.0 + std::numeric_limits<double>::epsilon() * 2.0))
        return false;
    if (!ez::math::isDifferent<double>(1.0, 1.0 - std::numeric_limits<double>::epsilon() * 2.0))
        return false;
    return true;
}

template <typename T>
bool TestEzMath_IsEqual() {
    if (ez::math::isEqual<T>(0, 5))
        return false;
    if (!ez::math::isEqual<T>(-0, 0))
        return false;
    if (ez::math::isEqual<T>(1, 1 + 1))
        return false;
    if (ez::math::isEqual<T>(1, 1 - 1))
        return false;
    return true;
}

template <int32_t>
bool TestEzMath_IsEqual() {
    if (!ez::math::isEqual<int32_t>(-5, -5))
        return false;
    if (ez::math::isEqual<int32_t>(0, 5))
        return false;
    if (!ez::math::isEqual<int32_t>(-0, 0))
        return false;
    if (ez::math::isEqual<int32_t>(1, 1 + 1))
        return false;
    if (ez::math::isEqual<int32_t>(1, 1 - 1))
        return false;
    return true;
}

template <int64_t>
bool TestEzMath_IsEqual() {
    if (!ez::math::isEqual<int64_t>(-5, -5))
        return false;
    if (ez::math::isEqual<int64_t>(0, 5))
        return false;
    if (!ez::math::isEqual<int64_t>(-0, 0))
        return false;
    if (ez::math::isEqual<int64_t>(1, 1 + 1))
        return false;
    if (ez::math::isEqual<int64_t>(1, 1 - 1))
        return false;
    return true;
}

template <>
bool TestEzMath_IsEqual<float>() {
    if (!ez::math::isEqual<float>(-5.0f, -5.0f))
        return false;
    if (ez::math::isEqual<float>(0.0f, 5.0f))
        return false;
    if (!ez::math::isEqual<float>(-0.0f, 0.0f))
        return false;
    if (ez::math::isEqual<float>(1.0f, 1.0f + std::numeric_limits<float>::epsilon()))
        return false;
    if (ez::math::isEqual<float>(1.0f, 1.0f - std::numeric_limits<float>::epsilon()))
        return false;
    return true;
}

template <>
bool TestEzMath_IsEqual<double>() {
    if (!ez::math::isEqual<double>(-5, -5))
        return false;
    if (ez::math::isEqual<double>(0, 5))
        return false;
    if (!ez::math::isEqual<double>(-0, 0))
        return false;
    if (ez::math::isEqual<double>(1, 1 + std::numeric_limits<double>::epsilon()))
        return false;
    if (ez::math::isEqual<double>(1, 1 - std::numeric_limits<double>::epsilon()))
        return false;
    return true;
}

template <typename T>
bool TestEzMath_Round() {
    if (ez::math::isDifferent(ez::math::round<T>(0.5), static_cast<T>(1.0)))
        return false;
    if (ez::math::isDifferent(ez::math::round<T>(0.0), static_cast<T>(0.0)))
        return false;
    if (ez::math::isDifferent(ez::math::round<T>(1.2), static_cast<T>(1.0)))
        return false;
    return true;
}

template <typename T>
bool TestEzMath_Floor() {
    if (ez::math::isDifferent(ez::math::floor<T>(0.5), static_cast<T>(0.0)))
        return false;
    if (ez::math::isDifferent(ez::math::floor<T>(0.0), static_cast<T>(0.0)))
        return false;
    if (ez::math::isDifferent(ez::math::floor<T>(1.2), static_cast<T>(1.0)))
        return false;
    return true;
}

template <typename T>
bool TestEzMath_Ceil() {
    if (ez::math::isDifferent(ez::math::ceil<T>(0.5), static_cast<T>(1.0)))
        return false;
    if (ez::math::isDifferent(ez::math::ceil<T>(0.0), static_cast<T>(0.0)))
        return false;
    if (ez::math::isDifferent(ez::math::ceil<T>(1.2), static_cast<T>(2.0)))
        return false;
    return true;
}

template <typename T>
bool TestEzMath_Fract() {
    if (ez::math::isDifferent(ez::math::fract<T>(0.5), static_cast<T>(0.5)))
        return false;
    if (ez::math::isDifferent(ez::math::fract<T>(1.2), static_cast<T>(0.2)))
        return false;
    if (ez::math::isDifferent(ez::math::fract<T>(2.0), static_cast<T>(0.0)))
        return false;
    return true;
}

template <typename T>
bool TestEzMath_Cos() {
    if (ez::math::isDifferent(ez::math::cos<T>(0.0), static_cast<T>(1.0)))
        return false;
    if (ez::math::isDifferent(ez::math::cos<T>(M_PI / 2.0), static_cast<T>(0.0)))
        return false;
    return true;
}

template <typename T>
bool TestEzMath_Acos() {
    if (ez::math::isDifferent(ez::math::acos<T>(1.0), static_cast<T>(0.0)))
        return false;
    if (ez::math::isDifferent(ez::math::acos<T>(0.0), static_cast<T>(M_PI / 2.0)))
        return false;
    return true;
}

template <typename T>
bool TestEzMath_Sin() {
    if (ez::math::isDifferent(ez::math::sin<T>(0.0), static_cast<T>(0.0)))
        return false;
    if (ez::math::isDifferent(ez::math::sin<T>(M_PI / 2.0), static_cast<T>(1.0)))
        return false;
    return true;
}

template <typename T>
bool TestEzMath_Asin() {
    if (ez::math::isDifferent(ez::math::asin<T>(0.0), static_cast<T>(0.0)))
        return false;
    if (ez::math::isDifferent(ez::math::asin<T>(1.0), static_cast<T>(M_PI / 2.0)))
        return false;
    return true;
}

template <typename T>
bool TestEzMath_Tan() {
    if (ez::math::isDifferent(ez::math::tan<T>(0.0), static_cast<T>(0.0)))
        return false;
    return true;
}

template <typename T>
bool TestEzMath_Atan() {
    if (ez::math::isDifferent(ez::math::atan<T>(0.0), static_cast<T>(0.0)))
        return false;
    if (ez::math::isDifferent(ez::math::atan<T>(1.0), static_cast<T>(M_PI / 4.0)))
        return false;
    return true;
}

template <typename T>
bool TestEzMath_Mini() {
    if (ez::math::mini<T>(1, 2) != 1)
        return false;
    if (ez::math::mini<T>(2, 1) != 1)
        return false;
    return true;
}

template <typename T>
bool TestEzMath_Maxi() {
    if (ez::math::maxi<T>(1, 2) != 2)
        return false;
    if (ez::math::maxi<T>(2, 1) != 2)
        return false;
    return true;
}

template <typename T>
bool TestEzMath_Clamp() {
    if (ez::math::clamp<T>(-1, 0, 1) != 0)
        return false;
    if (ez::math::clamp<T>(2, 0, 1) != 1)
        return false;
    if (ez::math::isDifferent(ez::math::clamp<T>(0.5, 0, 1), static_cast<T>(0.5)))
        return false;
    return true;
}

template <>
bool TestEzMath_Clamp<int64_t>() {
    if (ez::math::clamp<int64_t>(-1, 0, 1) != 0)
        return false;
    if (ez::math::clamp<int64_t>(2, 0, 1) != 1)
        return false;
    return true;
}

template <>
bool TestEzMath_Clamp<float>() {
    if (ez::math::clamp<float>(-1.0f, 0.0f, 1.0f) != 0.0f)
        return false;
    if (ez::math::clamp<float>(2.0f, 0.0f, 1.0f) != 1.0f)
        return false;
    if (ez::math::isDifferent(ez::math::clamp<float>(0.5f, 0.0f, 1.0f), 0.5f))
        return false;
    return true;
}

template <>
bool TestEzMath_Clamp<double>() {
    if (ez::math::clamp<double>(-1.0, 0.0, 1.0) != 0.0)
        return false;
    if (ez::math::clamp<double>(2.0, 0.0, 1.0) != 1.0)
        return false;
    if (ez::math::isDifferent(ez::math::clamp<double>(0.5, 0.0, 1.0), 0.5))
        return false;
    return true;
}

// Specialisation for int32_t, only positive values
template <>
bool TestEzMath_Clamp<int32_t>() {
    if (ez::math::clamp<int32_t>(-1, 0, 1) != 0)
        return false;
    if (ez::math::clamp<int32_t>(-2, -5, 5) != -2)
        return false;
    if (ez::math::clamp<int32_t>(2, -5, -2) != -2)
        return false;
    return true;
}

// Specialisation for uint32_t, only positive values
template <>
bool TestEzMath_Clamp<uint32_t>() {
    if (ez::math::clamp<uint32_t>(2, 0, 10) != 2)
        return false;
    if (ez::math::clamp<uint32_t>(2, 0, 1) != 1)
        return false;
    if (ez::math::clamp<uint32_t>(2, 5, 10) != 5)
        return false;
    return true;
}

// Specialisation for uint64_t, only positive values
template <>
bool TestEzMath_Clamp<uint64_t>() {
    if (ez::math::clamp<uint64_t>(2, 0, 10) != 2)
        return false;
    if (ez::math::clamp<uint64_t>(2, 0, 1) != 1)
        return false;
    if (ez::math::clamp<uint64_t>(2, 5, 10) != 5)
        return false;
    return true;
}
template <typename T>
bool TestEzMath_Abs() {
    if (ez::math::abs<T>(-1) != 1)
        return false;
    if (ez::math::abs<T>(1) != 1)
        return false;
    return true;
}

template <typename T>
bool TestEzMath_Sign() {
    if (ez::math::sign<T>(-1) != -1)
        return false;
    if (ez::math::sign<T>(1) != 1)
        return false;
    return true;
}

template <typename T>
bool TestEzMath_Step() {
    if (ez::math::step<T>(0.5, 0.4) != 0)
        return false;
    if (ez::math::step<T>(0.5, 0.6) != 1)
        return false;
    return true;
}

template <typename T>
bool TestEzMath_Mod() {
    if (ez::math::isDifferent(ez::math::mod<T>(5.5, 2.0), static_cast<T>(1.5)))
        return false;
    if (ez::math::isDifferent(ez::math::mod<T>(4.0, 2.0), static_cast<T>(0.0)))
        return false;
    return true;
}

template <typename T>
bool TestEzMath_InvMix() {
    if (ez::math::isDifferent(ez::math::invMix<T>(1, 2, 1.5), static_cast<T>(0.5)))
        return false;
    return true;
}

template <typename T>
bool TestEzMath_Lerp() {
    if (ez::math::isDifferent(ez::math::lerp<T>(0, 1, 0.5), static_cast<T>(0.5)))
        return false;
    if (ez::math::isDifferent(ez::math::lerp<T>(1, 2, 0.5), static_cast<T>(1.5)))
        return false;
    return true;
}

template <typename T>
bool TestEzMath_Eerp() {
    if (ez::math::isDifferent(ez::math::eerp<T>(0, 2, 0.5), static_cast<T>(0)))
        return false;
    if (ez::math::isDifferent(ez::math::eerp<T>(1, 2, 0.5), ez::math::sqrt<T>(2.0)))
        return false;
    return true;
}

template <typename T>
bool TestEzMath_Mix() {
    if (ez::math::isDifferent(ez::math::mix<T>(0, 1, 0.5), static_cast<T>(0.5)))
        return false;
    if (ez::math::isDifferent(ez::math::mix<T>(1, 2, 0.5), static_cast<T>(1.5)))
        return false;
    return true;
}

////////////////////////////////////////////////////////////////////////////
//// ENTRY POINT ///////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

#define IfTestExist(v)            \
    if (vTest == std::string(#v)) \
    return v()

bool TestEzMath(const std::string& vTest) {
    IfTestExist(TestEzMath_RoundN);

    else IfTestExist(TestEzMath_FloatIsValid);

    else IfTestExist(TestEzMath_IsDifferent<float>);
    else IfTestExist(TestEzMath_IsDifferent<double>);
    else IfTestExist(TestEzMath_IsDifferent<int32_t>);
    else IfTestExist(TestEzMath_IsDifferent<uint32_t>);
    else IfTestExist(TestEzMath_IsDifferent<int64_t>);
    else IfTestExist(TestEzMath_IsDifferent<uint64_t>);

    else IfTestExist(TestEzMath_IsEqual<float>);
    else IfTestExist(TestEzMath_IsEqual<double>);
    else IfTestExist(TestEzMath_IsEqual<int32_t>);
    else IfTestExist(TestEzMath_IsEqual<uint32_t>);
    else IfTestExist(TestEzMath_IsEqual<int64_t>);
    else IfTestExist(TestEzMath_IsEqual<uint64_t>);

    else IfTestExist(TestEzMath_Round<float>);
    else IfTestExist(TestEzMath_Round<double>);

    else IfTestExist(TestEzMath_Floor<float>);
    else IfTestExist(TestEzMath_Floor<double>);

    else IfTestExist(TestEzMath_Ceil<float>);
    else IfTestExist(TestEzMath_Ceil<double>);

    else IfTestExist(TestEzMath_Fract<float>);
    else IfTestExist(TestEzMath_Fract<double>);

    else IfTestExist(TestEzMath_Cos<float>);
    else IfTestExist(TestEzMath_Cos<double>);

    else IfTestExist(TestEzMath_Acos<float>);
    else IfTestExist(TestEzMath_Acos<double>);

    else IfTestExist(TestEzMath_Sin<float>);
    else IfTestExist(TestEzMath_Sin<double>);

    else IfTestExist(TestEzMath_Asin<float>);
    else IfTestExist(TestEzMath_Asin<double>);

    else IfTestExist(TestEzMath_Tan<float>);
    else IfTestExist(TestEzMath_Tan<double>);

    else IfTestExist(TestEzMath_Atan<float>);
    else IfTestExist(TestEzMath_Atan<double>);

    else IfTestExist(TestEzMath_Mini<float>);
    else IfTestExist(TestEzMath_Mini<double>);
    else IfTestExist(TestEzMath_Mini<int32_t>);
    else IfTestExist(TestEzMath_Mini<uint32_t>);
    else IfTestExist(TestEzMath_Mini<int64_t>);
    else IfTestExist(TestEzMath_Mini<uint64_t>);

    else IfTestExist(TestEzMath_Maxi<float>);
    else IfTestExist(TestEzMath_Maxi<double>);
    else IfTestExist(TestEzMath_Maxi<int32_t>);
    else IfTestExist(TestEzMath_Maxi<uint32_t>);
    else IfTestExist(TestEzMath_Maxi<int64_t>);
    else IfTestExist(TestEzMath_Maxi<uint64_t>);

    else IfTestExist(TestEzMath_Clamp<float>);
    else IfTestExist(TestEzMath_Clamp<double>);
    else IfTestExist(TestEzMath_Clamp<int32_t>);
    else IfTestExist(TestEzMath_Clamp<uint32_t>);
    else IfTestExist(TestEzMath_Clamp<int64_t>);
    else IfTestExist(TestEzMath_Clamp<uint64_t>);

    else IfTestExist(TestEzMath_Abs<float>);
    else IfTestExist(TestEzMath_Abs<double>);
    else IfTestExist(TestEzMath_Abs<int32_t>);
    else IfTestExist(TestEzMath_Abs<int64_t>);

    else IfTestExist(TestEzMath_Sign<float>);
    else IfTestExist(TestEzMath_Sign<double>);
    else IfTestExist(TestEzMath_Sign<int32_t>);
    else IfTestExist(TestEzMath_Sign<int64_t>);

    else IfTestExist(TestEzMath_Step<float>);
    else IfTestExist(TestEzMath_Step<double>);

    else IfTestExist(TestEzMath_Mod<float>);
    else IfTestExist(TestEzMath_Mod<double>);

    else IfTestExist(TestEzMath_Lerp<float>);
    else IfTestExist(TestEzMath_Lerp<double>);

    else IfTestExist(TestEzMath_Eerp<float>);
    else IfTestExist(TestEzMath_Eerp<double>);

    else IfTestExist(TestEzMath_Mix<float>);
    else IfTestExist(TestEzMath_Mix<double>);

    else IfTestExist(TestEzMath_InvMix<float>);
    else IfTestExist(TestEzMath_InvMix<double>);

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
