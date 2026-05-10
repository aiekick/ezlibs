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

// ezVecN is part of the ezLibs project : https://github.com/aiekick/ezLibs.git
//
// Runtime-sized dense vector (size defined at runtime), companion to matN
// for numerical linear algebra (decompositions, projections, etc.).
//
// Reference: Golub & Van Loan, "Matrix Computations" 4th ed., chap. 1.

#include <vector>
#include <cstddef>
#include <type_traits>
#include <initializer_list>
#include <cmath>

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
namespace math {

template <typename T>
class vecN {
    static_assert(std::is_floating_point<T>::value, "vecN requires a floating-point T");

public:
    static vecN Zero(std::size_t aSize) {
        return vecN(aSize);
    }

private:
    std::vector<T> m_data{};

public:
    vecN() = default;

    explicit vecN(std::size_t aSize) : m_data(aSize, T(0)) {
    }

    vecN(std::size_t aSize, T aFillValue) : m_data(aSize, aFillValue) {
    }

    vecN(std::initializer_list<T> aValues) : m_data(aValues) {
    }

    std::size_t size() const {
        return m_data.size();
    }
    bool empty() const {
        return m_data.empty();
    }

    T* data() {
        return m_data.data();
    }
    const T* data() const {
        return m_data.data();
    }

    T& operator[](std::size_t aIndex) {
        return m_data[aIndex];
    }
    T operator[](std::size_t aIndex) const {
        return m_data[aIndex];
    }

    T lengthSquared() const {
        T accumulator = T(0);
        for (std::size_t componentIndex = 0; componentIndex < m_data.size(); ++componentIndex) {
            accumulator += m_data[componentIndex] * m_data[componentIndex];
        }
        return accumulator;
    }

    T length() const {
        return std::sqrt(lengthSquared());
    }

    // Returns 0 when the two vectors have different sizes (mismatched dot
    // product is treated as a no-op rather than an error).
    T dot(const vecN& aRight) const {
        if (m_data.size() != aRight.m_data.size()) {
            return T(0);
        }
        T accumulator = T(0);
        for (std::size_t componentIndex = 0; componentIndex < m_data.size(); ++componentIndex) {
            accumulator += m_data[componentIndex] * aRight.m_data[componentIndex];
        }
        return accumulator;
    }

    // In-place normalization. Returns false (and leaves *this untouched)
    // if the vector length is below aEpsilon (e.g. null vector).
    bool normalize(T aEpsilon = T(1e-12)) {
        T currentLength = length();
        if (currentLength <= aEpsilon) {
            return false;
        }
        T inverseLength = T(1) / currentLength;
        for (std::size_t componentIndex = 0; componentIndex < m_data.size(); ++componentIndex) {
            m_data[componentIndex] *= inverseLength;
        }
        return true;
    }

    vecN normalized(T aEpsilon = T(1e-12)) const {
        vecN copy(*this);
        copy.normalize(aEpsilon);
        return copy;
    }
};

}  // namespace math
}  // namespace ez

#ifdef _MSC_VER
#pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
