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

// ezEigenSym is part of the ezLibs project : https://github.com/aiekick/ezLibs.git
//
// Eigenvalue / eigenvector decomposition for a real symmetric matrix.
//
// For a symmetric n x n matrix A, computes:
//
//     A = V * diag(d) * V^T
//
// where V is n x n orthogonal (eigenvectors as columns) and d is the
// n-vector of eigenvalues, sorted in decreasing order.
//
// Algorithm: Householder tridiagonalization followed by QL iteration with
// implicit shifts.
//
// References:
//   - Numerical Recipes 3rd ed., chap. 11.4 (tred2 / tqli)
//   - Golub & Van Loan, "Matrix Computations" 4th ed., chap. 8.3
//
// Convention note on identifier names:
//   The single-letter variables used in this file (n for matrix size; i, j,
//   k, l, m for loop indices; d/e for the diagonal / off-diagonal of the
//   working tridiagonal matrix; c, s for cosine/sine of Givens rotations;
//   f, g, h, p, r, b for the scalar intermediates of the rotations) mirror
//   the canonical Householder-QL presentation in the references above.
//   Renaming them to long descriptive identifiers would obscure the
//   correspondence to the literature. Each is documented in the comment
//   block of the function where it is introduced.
//
//   The input is expected to be symmetric. No symmetry check is performed
//   (the caller is responsible).

#include "ezMatN.hpp"
#include "ezVecN.hpp"

#include <cstddef>
#include <cmath>
#include <algorithm>
#include <type_traits>
#include <limits>

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
namespace eigenSym {

template <typename T>
struct result {
    matN<T> v{};
    vecN<T> d{};
    bool success{false};
};

namespace detail {

// Numerically safe sqrt(a^2 + b^2).
template <typename T>
inline T safeHypot(T a, T b) {
    T absA = std::abs(a);
    T absB = std::abs(b);
    if (absA > absB) {
        T ratio = absB / absA;
        return absA * std::sqrt(T(1) + ratio * ratio);
    }
    if (absB > T(0)) {
        T ratio = absA / absB;
        return absB * std::sqrt(T(1) + ratio * ratio);
    }
    return T(0);
}

// |a| with the sign of b. b == 0 yields +|a|.
template <typename T>
inline T copySign(T a, T b) {
    return (b >= T(0)) ? std::abs(a) : -std::abs(a);
}

// Householder reduction of the symmetric matrix aoZ (n x n) to tridiagonal form.
// On input  : aoZ holds the symmetric matrix.
// On output : aoDiag holds the diagonal entries of the resulting tridiagonal,
//             aoOffDiag holds the off-diagonal entries with aoOffDiag[0] = 0
//             (the off-diagonal entries are stored at indices 1..n-1),
//             aoZ holds the orthogonal transformation Q such that
//             Q^T * A_input * Q = tridiag(aoDiag, aoOffDiag).
template <typename T>
void householderTridiag(matN<T>& aoZ, vecN<T>& aoDiag, vecN<T>& aoOffDiag) {
    const int n = static_cast<int>(aoZ.rows());

    auto idx = [](int aSignedIndex) -> std::size_t { return static_cast<std::size_t>(aSignedIndex); };

    for (int i = n - 1; i > 0; --i) {
        int l = i - 1;
        T h = T(0);
        T scale = T(0);
        if (l > 0) {
            for (int k = 0; k <= l; ++k) {
                scale += std::abs(aoZ(idx(i), idx(k)));
            }
            if (scale == T(0)) {
                aoOffDiag[idx(i)] = aoZ(idx(i), idx(l));
            } else {
                for (int k = 0; k <= l; ++k) {
                    aoZ(idx(i), idx(k)) /= scale;
                    h += aoZ(idx(i), idx(k)) * aoZ(idx(i), idx(k));
                }
                T f = aoZ(idx(i), idx(l));
                T g = (f >= T(0)) ? -std::sqrt(h) : std::sqrt(h);
                aoOffDiag[idx(i)] = scale * g;
                h -= f * g;
                aoZ(idx(i), idx(l)) = f - g;
                f = T(0);
                for (int j = 0; j <= l; ++j) {
                    aoZ(idx(j), idx(i)) = aoZ(idx(i), idx(j)) / h;
                    g = T(0);
                    for (int k = 0; k <= j; ++k) {
                        g += aoZ(idx(j), idx(k)) * aoZ(idx(i), idx(k));
                    }
                    for (int k = j + 1; k <= l; ++k) {
                        g += aoZ(idx(k), idx(j)) * aoZ(idx(i), idx(k));
                    }
                    aoOffDiag[idx(j)] = g / h;
                    f += aoOffDiag[idx(j)] * aoZ(idx(i), idx(j));
                }
                T hh = f / (h + h);
                for (int j = 0; j <= l; ++j) {
                    f = aoZ(idx(i), idx(j));
                    T eValue = aoOffDiag[idx(j)] - hh * f;
                    aoOffDiag[idx(j)] = eValue;
                    for (int k = 0; k <= j; ++k) {
                        aoZ(idx(j), idx(k)) -= (f * aoOffDiag[idx(k)] + eValue * aoZ(idx(i), idx(k)));
                    }
                }
            }
        } else {
            aoOffDiag[idx(i)] = aoZ(idx(i), idx(l));
        }
        aoDiag[idx(i)] = h;
    }
    aoDiag[0] = T(0);
    aoOffDiag[0] = T(0);

    // Accumulate the transformations into aoZ.
    for (int i = 0; i < n; ++i) {
        int l = i - 1;
        if (aoDiag[idx(i)] != T(0)) {
            for (int j = 0; j <= l; ++j) {
                T g = T(0);
                for (int k = 0; k <= l; ++k) {
                    g += aoZ(idx(i), idx(k)) * aoZ(idx(k), idx(j));
                }
                for (int k = 0; k <= l; ++k) {
                    aoZ(idx(k), idx(j)) -= g * aoZ(idx(k), idx(i));
                }
            }
        }
        aoDiag[idx(i)] = aoZ(idx(i), idx(i));
        aoZ(idx(i), idx(i)) = T(1);
        for (int j = 0; j <= l; ++j) {
            aoZ(idx(j), idx(i)) = T(0);
            aoZ(idx(i), idx(j)) = T(0);
        }
    }
}

// QL algorithm with implicit shifts on a tridiagonal symmetric matrix
// (aoDiag, aoOffDiag). aoZ accumulates the eigenvectors, started from the
// Householder transformations produced by householderTridiag.
//
// Returns false if any QL iteration fails to converge within
// aMaxIterations, true otherwise.
template <typename T>
bool qlIteration(vecN<T>& aoDiag, vecN<T>& aoOffDiag, matN<T>& aoZ, int aMaxIterations, T aTolerance) {
    const int n = static_cast<int>(aoDiag.size());

    auto idx = [](int aSignedIndex) -> std::size_t { return static_cast<std::size_t>(aSignedIndex); };

    // Shift the off-diagonal: tred2 stored e at index i, tqli expects it at index i-1.
    for (int i = 1; i < n; ++i) {
        aoOffDiag[idx(i - 1)] = aoOffDiag[idx(i)];
    }
    aoOffDiag[idx(n - 1)] = T(0);

    for (int l = 0; l < n; ++l) {
        int iter = 0;
        int m = 0;
        do {
            for (m = l; m < n - 1; ++m) {
                T dd = std::abs(aoDiag[idx(m)]) + std::abs(aoDiag[idx(m + 1)]);
                if (std::abs(aoOffDiag[idx(m)]) <= aTolerance * dd) {
                    break;
                }
            }
            if (m != l) {
                if (iter == aMaxIterations) {
                    return false;
                }
                ++iter;
                T g = (aoDiag[idx(l + 1)] - aoDiag[idx(l)]) / (T(2) * aoOffDiag[idx(l)]);
                T r = safeHypot(g, T(1));
                g = aoDiag[idx(m)] - aoDiag[idx(l)] + aoOffDiag[idx(l)] / (g + copySign(r, g));
                T s = T(1);
                T c = T(1);
                T p = T(0);
                int i = 0;
                bool restart = false;
                for (i = m - 1; i >= l; --i) {
                    T f = s * aoOffDiag[idx(i)];
                    T b = c * aoOffDiag[idx(i)];
                    r = safeHypot(f, g);
                    aoOffDiag[idx(i + 1)] = r;
                    if (r == T(0)) {
                        aoDiag[idx(i + 1)] -= p;
                        aoOffDiag[idx(m)] = T(0);
                        restart = true;
                        break;
                    }
                    s = f / r;
                    c = g / r;
                    g = aoDiag[idx(i + 1)] - p;
                    r = (aoDiag[idx(i)] - g) * s + T(2) * c * b;
                    p = s * r;
                    aoDiag[idx(i + 1)] = g + p;
                    g = c * r - b;
                    // Update eigenvectors.
                    for (int k = 0; k < n; ++k) {
                        T tmp = aoZ(idx(k), idx(i + 1));
                        aoZ(idx(k), idx(i + 1)) = s * aoZ(idx(k), idx(i)) + c * tmp;
                        aoZ(idx(k), idx(i)) = c * aoZ(idx(k), idx(i)) - s * tmp;
                    }
                }
                if (restart) {
                    continue;
                }
                aoDiag[idx(l)] -= p;
                aoOffDiag[idx(l)] = g;
                aoOffDiag[idx(m)] = T(0);
            }
        } while (m != l);
    }
    return true;
}

// Sort eigenvalues in descending order, applying the same column permutation
// to the eigenvector matrix.
template <typename T>
void sortDescending(vecN<T>& aoDiag, matN<T>& aoZ) {
    const std::size_t n = aoDiag.size();
    for (std::size_t i = 0; i + 1 < n; ++i) {
        std::size_t maxIndex = i;
        T maxValue = aoDiag[i];
        for (std::size_t j = i + 1; j < n; ++j) {
            if (aoDiag[j] > maxValue) {
                maxValue = aoDiag[j];
                maxIndex = j;
            }
        }
        if (maxIndex != i) {
            std::swap(aoDiag[i], aoDiag[maxIndex]);
            for (std::size_t k = 0; k < n; ++k) {
                std::swap(aoZ(k, i), aoZ(k, maxIndex));
            }
        }
    }
}

}  // namespace detail

template <typename T>
class solver {
    static_assert(std::is_floating_point<T>::value, "eigenSym::solver requires a floating-point T");

private:
    // Per-eigenvalue iteration cap. 30 is the value used by Numerical Recipes;
    // QL with implicit shifts converges cubically, so this is generous.
    static const int s_maxIterationsPerEigenvalue = 30;

public:
    // Decompose a real symmetric matrix.
    //
    // aMatrix     : square symmetric matrix (no symmetry check is performed)
    // aTolerance  : convergence threshold for the off-diagonal entries
    //
    // Returns:
    //   result.success == false if input is empty, non-square, or QL did not
    //   converge for some eigenvalue within the iteration cap.
    result<T> compute(
        const matN<T>& aMatrix,
        T aTolerance = std::numeric_limits<T>::epsilon() * T(8)) const {
        result<T> output;
        if (aMatrix.empty() || aMatrix.rows() != aMatrix.columns()) {
            return output;
        }

        const std::size_t n = aMatrix.rows();
        matN<T> z = aMatrix;
        vecN<T> diag(n);
        vecN<T> offDiag(n);

        detail::householderTridiag(z, diag, offDiag);
        const bool converged = detail::qlIteration(diag, offDiag, z, s_maxIterationsPerEigenvalue, aTolerance);
        if (!converged) {
            return output;
        }
        detail::sortDescending(diag, z);

        output.v = z;
        output.d = diag;
        output.success = true;
        return output;
    }
};

}  // namespace eigenSym
}  // namespace math
}  // namespace ez

#ifdef _MSC_VER
#pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
