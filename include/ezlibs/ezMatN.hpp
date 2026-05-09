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

// ezMatN is part of the ezLibs project : https://github.com/aiekick/ezLibs.git
//
// Runtime-sized dense matrix (rows x columns both defined at runtime), used as
// the working type for numerical decompositions (SVD, eigen, etc.). Storage is
// column-major to stay consistent with ezMat2/3/4.
//
// Reference: Golub & Van Loan, "Matrix Computations" 4th ed., chap. 1.

#include <vector>
#include <cstddef>
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

template <typename T>
class matN {
    static_assert(std::is_floating_point<T>::value, "matN requires a floating-point T");

public:
    static matN Identity(std::size_t aDimension) {
        matN result(aDimension, aDimension);
        for (std::size_t indexOnDiagonal = 0; indexOnDiagonal < aDimension; ++indexOnDiagonal) {
            result(indexOnDiagonal, indexOnDiagonal) = T(1);
        }
        return result;
    }

    static matN Zero(std::size_t aRowCount, std::size_t aColumnCount) {
        return matN(aRowCount, aColumnCount);
    }

private:
    std::vector<T> m_data{};
    std::size_t m_rows{0};
    std::size_t m_columns{0};

public:
    matN() = default;

    matN(std::size_t aRowCount, std::size_t aColumnCount)
        : m_data(aRowCount * aColumnCount, T(0)), m_rows(aRowCount), m_columns(aColumnCount) {
    }

    matN(std::size_t aRowCount, std::size_t aColumnCount, T aFillValue)
        : m_data(aRowCount * aColumnCount, aFillValue), m_rows(aRowCount), m_columns(aColumnCount) {
    }

    std::size_t rows() const {
        return m_rows;
    }
    std::size_t columns() const {
        return m_columns;
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

    // Column-major linear index: (column * rows) + row
    T& operator()(std::size_t aRowIndex, std::size_t aColumnIndex) {
        return m_data[aColumnIndex * m_rows + aRowIndex];
    }
    T operator()(std::size_t aRowIndex, std::size_t aColumnIndex) const {
        return m_data[aColumnIndex * m_rows + aRowIndex];
    }

    matN transpose() const {
        matN result(m_columns, m_rows);
        for (std::size_t columnIndex = 0; columnIndex < m_columns; ++columnIndex) {
            for (std::size_t rowIndex = 0; rowIndex < m_rows; ++rowIndex) {
                result(columnIndex, rowIndex) = (*this)(rowIndex, columnIndex);
            }
        }
        return result;
    }

    // Naive O(n^3) matrix product. Returns an empty matrix if dimensions
    // do not match (m_columns must equal aRight.m_rows).
    matN operator*(const matN& aRight) const {
        if (m_columns != aRight.m_rows || m_data.empty() || aRight.m_data.empty()) {
            return matN();
        }
        matN result(m_rows, aRight.m_columns);
        for (std::size_t columnIndex = 0; columnIndex < aRight.m_columns; ++columnIndex) {
            for (std::size_t rowIndex = 0; rowIndex < m_rows; ++rowIndex) {
                T accumulator = T(0);
                for (std::size_t innerIndex = 0; innerIndex < m_columns; ++innerIndex) {
                    accumulator += (*this)(rowIndex, innerIndex) * aRight(innerIndex, columnIndex);
                }
                result(rowIndex, columnIndex) = accumulator;
            }
        }
        return result;
    }
};

}  // namespace ez

#ifdef _MSC_VER
#pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
