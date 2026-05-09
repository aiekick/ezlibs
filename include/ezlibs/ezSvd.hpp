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

// ezSvd is part of the ezLibs project : https://github.com/aiekick/ezLibs.git
//
// Singular Value Decomposition for a runtime-sized real matrix.
//
// Decomposes any m x n matrix A (with m >= n; if m < n the input is transposed
// internally) into:
//
//     A = U * diag(s) * V^T
//
// where U is m x n with orthonormal columns, s is the n-vector of singular
// values sorted in decreasing order, and V is n x n orthonormal.
//
// Algorithm: Householder bidiagonalization followed by QR iteration with
// implicit shifts (Golub-Reinsch).
//
// References:
//   - Golub & Van Loan, "Matrix Computations" 4th ed., chap. 8.6
//   - Numerical Recipes 3rd ed., chap. 2.6 / 11.3
//   - The JAMA library (NIST, public domain) for cross-checking
//
// Convention note on identifier names:
//   The single-letter variables used in this file (m, n, i, j, k for sizes
//   and loop indices, s/e for diagonal/super-diagonal of the working
//   bidiagonal matrix, c/sn for cosine/sine of Givens rotations, f/g/h/t
//   for the scalar intermediates of those rotations) deliberately mirror
//   the Golub-Reinsch presentation in the references above. Renaming them
//   to long descriptive identifiers would obscure the correspondence to
//   the canonical formulae and make cross-checking against the literature
//   significantly harder. Each such variable is documented in the comment
//   block of the function where it is introduced.

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
namespace svd {

template <typename T>
struct result {
    matN<T> u{};
    vecN<T> s{};
    matN<T> v{};
    bool success{false};
};

namespace detail {

// Numerically safe hypot: sqrt(a^2 + b^2) without overflow / underflow.
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

// Sign-with-zero-as-positive helper, matching Numerical Recipes "SIGN(a, b)"
// macro: returns |a| with the sign of b. b == 0 yields +|a|.
template <typename T>
inline T copySign(T a, T b) {
    return (b >= T(0)) ? std::abs(a) : -std::abs(a);
}

}  // namespace detail

template <typename T>
class solver {
    static_assert(std::is_floating_point<T>::value, "svd::solver requires a floating-point T");

private:
    // Maximum number of QR iterations before giving up on a singular value.
    // The QR step on a 2x2 trailing block converges quadratically in practice;
    // 75 is the safety net JAMA uses.
    static const int s_maxQrIterations = 75;

public:
    // Compute the SVD of aMatrix.
    //
    // aTolerance:
    //   Convergence tolerance on the off-diagonal entries of the bidiagonal
    //   working matrix. The default scales with type epsilon; pass a larger
    //   value to converge faster on huge problems where you can accept
    //   slightly less accurate singular values.
    //
    // Returns:
    //   result.success == false if input is empty, or if the QR iteration
    //   failed to converge for any singular value (extremely rare).
    result<T> compute(
        const matN<T>& aMatrix,
        T aTolerance = std::numeric_limits<T>::epsilon() * T(8)) const {
        result<T> output;
        if (aMatrix.empty()) {
            return output;
        }

        // If m < n, work on the transpose and swap u <-> v at the end.
        const bool transposedInput = (aMatrix.rows() < aMatrix.columns());
        const matN<T> workInput = transposedInput ? aMatrix.transpose() : aMatrix;

        // From here on m >= n.
        // m: number of rows of the working matrix
        // n: number of columns of the working matrix
        const std::size_t m = workInput.rows();
        const std::size_t n = workInput.columns();

        // Working buffers. JAMA convention:
        //   work : m x n matrix progressively reduced to bidiagonal form
        //   s    : n-vector for the diagonal of the bidiagonal matrix
        //          (becomes the singular values after QR iteration)
        //   e    : n-vector for the super-diagonal of the bidiagonal matrix
        //          (driven to zero by the QR iteration)
        //   uMat : m x n matrix accumulating left orthogonal transforms
        //   vMat : n x n matrix accumulating right orthogonal transforms
        matN<T> work = workInput;
        vecN<T> s(n);
        vecN<T> e(n);
        matN<T> uMat = matN<T>::Zero(m, n);
        matN<T> vMat = matN<T>::Zero(n, n);

        // Number of column / row Householder transforms to actually perform
        // (last column / row would only annihilate already-zero entries).
        const std::size_t nct = (m < n) ? m : n;             // = n here
        const std::size_t nrt = (n >= 2) ? (n - 2) : 0;
        const std::size_t outerLimit = (nct > nrt) ? nct : nrt;

        // ---------- Phase 1 : Householder bidiagonalization ----------
        for (std::size_t k = 0; k < outerLimit; ++k) {
            if (k < nct) {
                // Compute the transformation for the k-th column and place
                // the resulting norm in s[k].
                T columnNorm = T(0);
                for (std::size_t i = k; i < m; ++i) {
                    columnNorm = detail::safeHypot(columnNorm, work(i, k));
                }
                if (columnNorm != T(0)) {
                    if (work(k, k) < T(0)) {
                        columnNorm = -columnNorm;
                    }
                    for (std::size_t i = k; i < m; ++i) {
                        work(i, k) = work(i, k) / columnNorm;
                    }
                    work(k, k) = work(k, k) + T(1);
                }
                s[k] = -columnNorm;
            }

            // Apply the column transform to the remaining columns.
            for (std::size_t j = k + 1; j < n; ++j) {
                if (k < nct && s[k] != T(0)) {
                    T t = T(0);
                    for (std::size_t i = k; i < m; ++i) {
                        t += work(i, k) * work(i, j);
                    }
                    t = -t / work(k, k);
                    for (std::size_t i = k; i < m; ++i) {
                        work(i, j) += t * work(i, k);
                    }
                }
                // Place the k-th row of the working matrix into e[] for the
                // next phase's row transform.
                e[j] = work(k, j);
            }

            if (k < nct) {
                // Save the column transform in the appropriate column of
                // uMat so it can be replayed when generating U.
                for (std::size_t i = k; i < m; ++i) {
                    uMat(i, k) = work(i, k);
                }
            }

            if (k < nrt) {
                // Compute the transformation for the k-th row.
                T rowNorm = T(0);
                for (std::size_t i = k + 1; i < n; ++i) {
                    rowNorm = detail::safeHypot(rowNorm, e[i]);
                }
                if (rowNorm != T(0)) {
                    if (e[k + 1] < T(0)) {
                        rowNorm = -rowNorm;
                    }
                    for (std::size_t i = k + 1; i < n; ++i) {
                        e[i] = e[i] / rowNorm;
                    }
                    e[k + 1] = e[k + 1] + T(1);
                }
                e[k] = -rowNorm;

                if (k + 1 < m && e[k] != T(0)) {
                    // Apply the row transform to the remaining rows of work.
                    vecN<T> work2(m, T(0));
                    for (std::size_t i = k + 1; i < m; ++i) {
                        for (std::size_t j = k + 1; j < n; ++j) {
                            work2[i] += e[j] * work(i, j);
                        }
                    }
                    for (std::size_t j = k + 1; j < n; ++j) {
                        T t = -e[j] / e[k + 1];
                        for (std::size_t i = k + 1; i < m; ++i) {
                            work(i, j) += t * work2[i];
                        }
                    }
                }

                // Save the row transform in vMat to replay when generating V.
                for (std::size_t i = k + 1; i < n; ++i) {
                    vMat(i, k) = e[i];
                }
            }
        }

        // Number of singular values to handle below.
        std::size_t p = n;
        if (nct < n) {
            s[nct] = work(nct, nct);
        }
        if (m < p) {
            s[p - 1] = T(0);
        }
        if (nrt + 1 < p) {
            e[nrt] = work(nrt, p - 1);
        }
        e[p - 1] = T(0);

        // ---------- Generate U from the saved column transforms ----------
        for (std::size_t j = nct; j < n; ++j) {
            for (std::size_t i = 0; i < m; ++i) {
                uMat(i, j) = T(0);
            }
            uMat(j, j) = T(1);
        }
        for (std::size_t kStep = nct; kStep > 0; --kStep) {
            std::size_t k = kStep - 1;
            if (s[k] != T(0)) {
                for (std::size_t j = k + 1; j < n; ++j) {
                    T t = T(0);
                    for (std::size_t i = k; i < m; ++i) {
                        t += uMat(i, k) * uMat(i, j);
                    }
                    t = -t / uMat(k, k);
                    for (std::size_t i = k; i < m; ++i) {
                        uMat(i, j) += t * uMat(i, k);
                    }
                }
                for (std::size_t i = k; i < m; ++i) {
                    uMat(i, k) = -uMat(i, k);
                }
                uMat(k, k) = T(1) + uMat(k, k);
                for (std::size_t i = 0; i + 1 < k; ++i) {
                    uMat(i, k) = T(0);
                }
            } else {
                for (std::size_t i = 0; i < m; ++i) {
                    uMat(i, k) = T(0);
                }
                uMat(k, k) = T(1);
            }
        }

        // ---------- Generate V from the saved row transforms ----------
        for (std::size_t kStep = n; kStep > 0; --kStep) {
            std::size_t k = kStep - 1;
            if (k < nrt && e[k] != T(0)) {
                for (std::size_t j = k + 1; j < n; ++j) {
                    T t = T(0);
                    for (std::size_t i = k + 1; i < n; ++i) {
                        t += vMat(i, k) * vMat(i, j);
                    }
                    t = -t / vMat(k + 1, k);
                    for (std::size_t i = k + 1; i < n; ++i) {
                        vMat(i, j) += t * vMat(i, k);
                    }
                }
            }
            for (std::size_t i = 0; i < n; ++i) {
                vMat(i, k) = T(0);
            }
            vMat(k, k) = T(1);
        }

        // ---------- Phase 2 : QR iteration on the bidiagonal s/e ----------
        //
        // Indices in this phase use signed int because k and ks are required
        // to take the sentinel value -1 ("not found"), and arithmetic such as
        // p - 2 with p == 1 would underflow std::size_t. The cast helper
        // converts back when accessing the size_t-indexed s/e/uMat/vMat.
        const int pp = static_cast<int>(p) - 1;
        int pSigned = static_cast<int>(p);
        int iter = 0;
        bool converged = true;

        auto toIndex = [](int aSignedIndex) -> std::size_t {
            return static_cast<std::size_t>(aSignedIndex);
        };

        while (pSigned > 0) {
            int k = 0;
            int caseSelector = 0;

            // Find the largest k in [-1, pSigned-2] such that e[k] is
            // negligible. JAMA-style: the loop allows k to reach -1.
            for (k = pSigned - 2; k >= -1; --k) {
                if (k == -1) {
                    break;
                }
                T threshold = aTolerance * (std::abs(s[toIndex(k)]) + std::abs(s[toIndex(k + 1)]));
                if (std::abs(e[toIndex(k)]) <= threshold) {
                    e[toIndex(k)] = T(0);
                    break;
                }
            }

            if (k == pSigned - 2) {
                caseSelector = 4;
            } else {
                int ks = 0;
                for (ks = pSigned - 1; ks >= k; --ks) {
                    if (ks == k) {
                        break;
                    }
                    T threshold = T(0);
                    if (ks != pSigned) {
                        threshold += std::abs(e[toIndex(ks)]);
                    }
                    if (ks != k + 1) {
                        threshold += std::abs(e[toIndex(ks - 1)]);
                    }
                    if (std::abs(s[toIndex(ks)]) <= aTolerance * threshold) {
                        s[toIndex(ks)] = T(0);
                        break;
                    }
                }
                if (ks == k) {
                    caseSelector = 3;
                } else if (ks == pSigned - 1) {
                    caseSelector = 1;
                } else {
                    caseSelector = 2;
                    k = ks;
                }
            }
            ++k;

            switch (caseSelector) {
                case 1: {
                    // Deflate negligible s[pSigned-1].
                    T f = e[toIndex(pSigned - 2)];
                    e[toIndex(pSigned - 2)] = T(0);
                    for (int j = pSigned - 2; j >= k; --j) {
                        T t = detail::safeHypot(s[toIndex(j)], f);
                        T c = s[toIndex(j)] / t;
                        T sn = f / t;
                        s[toIndex(j)] = t;
                        if (j != k) {
                            f = -sn * e[toIndex(j - 1)];
                            e[toIndex(j - 1)] = c * e[toIndex(j - 1)];
                        }
                        for (std::size_t i = 0; i < n; ++i) {
                            T tmp = c * vMat(i, toIndex(j)) + sn * vMat(i, toIndex(pSigned - 1));
                            vMat(i, toIndex(pSigned - 1)) = -sn * vMat(i, toIndex(j)) + c * vMat(i, toIndex(pSigned - 1));
                            vMat(i, toIndex(j)) = tmp;
                        }
                    }
                    break;
                }
                case 2: {
                    // Split at negligible s[k-1].
                    T f = e[toIndex(k - 1)];
                    e[toIndex(k - 1)] = T(0);
                    for (int j = k; j < pSigned; ++j) {
                        T t = detail::safeHypot(s[toIndex(j)], f);
                        T c = s[toIndex(j)] / t;
                        T sn = f / t;
                        s[toIndex(j)] = t;
                        f = -sn * e[toIndex(j)];
                        e[toIndex(j)] = c * e[toIndex(j)];
                        for (std::size_t i = 0; i < m; ++i) {
                            T tmp = c * uMat(i, toIndex(j)) + sn * uMat(i, toIndex(k - 1));
                            uMat(i, toIndex(k - 1)) = -sn * uMat(i, toIndex(j)) + c * uMat(i, toIndex(k - 1));
                            uMat(i, toIndex(j)) = tmp;
                        }
                    }
                    break;
                }
                case 3: {
                    // QR step with implicit shift derived from the
                    // trailing 2x2 block of B^T * B.
                    T scale = std::max(
                        std::abs(s[toIndex(pSigned - 1)]),
                        std::max(std::abs(s[toIndex(pSigned - 2)]),
                            std::max(std::abs(e[toIndex(pSigned - 2)]),
                                std::max(std::abs(s[toIndex(k)]), std::abs(e[toIndex(k)])))));
                    if (scale == T(0)) {
                        scale = T(1);
                    }
                    T sp = s[toIndex(pSigned - 1)] / scale;
                    T spm1 = s[toIndex(pSigned - 2)] / scale;
                    T epm1 = e[toIndex(pSigned - 2)] / scale;
                    T sk = s[toIndex(k)] / scale;
                    T ek = e[toIndex(k)] / scale;
                    T b = ((spm1 + sp) * (spm1 - sp) + epm1 * epm1) / T(2);
                    T cTerm = (sp * epm1) * (sp * epm1);
                    T shift = T(0);
                    if (b != T(0) || cTerm != T(0)) {
                        shift = std::sqrt(b * b + cTerm);
                        if (b < T(0)) {
                            shift = -shift;
                        }
                        shift = cTerm / (b + shift);
                    }
                    T f = (sk + sp) * (sk - sp) + shift;
                    T g = sk * ek;

                    for (int j = k; j < pSigned - 1; ++j) {
                        T t = detail::safeHypot(f, g);
                        T c = f / t;
                        T sn = g / t;
                        if (j != k) {
                            e[toIndex(j - 1)] = t;
                        }
                        f = c * s[toIndex(j)] + sn * e[toIndex(j)];
                        e[toIndex(j)] = c * e[toIndex(j)] - sn * s[toIndex(j)];
                        g = sn * s[toIndex(j + 1)];
                        s[toIndex(j + 1)] = c * s[toIndex(j + 1)];
                        for (std::size_t i = 0; i < n; ++i) {
                            T tmp = c * vMat(i, toIndex(j)) + sn * vMat(i, toIndex(j + 1));
                            vMat(i, toIndex(j + 1)) = -sn * vMat(i, toIndex(j)) + c * vMat(i, toIndex(j + 1));
                            vMat(i, toIndex(j)) = tmp;
                        }
                        t = detail::safeHypot(f, g);
                        c = f / t;
                        sn = g / t;
                        s[toIndex(j)] = t;
                        f = c * e[toIndex(j)] + sn * s[toIndex(j + 1)];
                        s[toIndex(j + 1)] = -sn * e[toIndex(j)] + c * s[toIndex(j + 1)];
                        g = sn * e[toIndex(j + 1)];
                        e[toIndex(j + 1)] = c * e[toIndex(j + 1)];
                        if (j < static_cast<int>(m) - 1) {
                            for (std::size_t i = 0; i < m; ++i) {
                                T tmp = c * uMat(i, toIndex(j)) + sn * uMat(i, toIndex(j + 1));
                                uMat(i, toIndex(j + 1)) = -sn * uMat(i, toIndex(j)) + c * uMat(i, toIndex(j + 1));
                                uMat(i, toIndex(j)) = tmp;
                            }
                        }
                    }
                    e[toIndex(pSigned - 2)] = f;
                    ++iter;
                    if (iter > s_maxQrIterations) {
                        converged = false;
                        pSigned = 0;
                    }
                    break;
                }
                case 4: {
                    // Convergence: a singular value is settled.
                    if (s[toIndex(k)] <= T(0)) {
                        s[toIndex(k)] = (s[toIndex(k)] < T(0)) ? -s[toIndex(k)] : T(0);
                        for (int i = 0; i <= pp; ++i) {
                            vMat(toIndex(i), toIndex(k)) = -vMat(toIndex(i), toIndex(k));
                        }
                    }
                    // Bubble s[k] up to its sorted position. Bound is pp
                    // (the original n-1, kept constant) and NOT pSigned-1,
                    // because newly-settled values must be allowed to move
                    // across the whole [0, n-1] range, not only the shrinking
                    // remaining sub-problem.
                    while (k < pp) {
                        if (s[toIndex(k)] >= s[toIndex(k + 1)]) {
                            break;
                        }
                        std::swap(s[toIndex(k)], s[toIndex(k + 1)]);
                        if (k < static_cast<int>(n) - 1) {
                            for (std::size_t i = 0; i < n; ++i) {
                                std::swap(vMat(i, toIndex(k)), vMat(i, toIndex(k + 1)));
                            }
                        }
                        if (k < static_cast<int>(m) - 1) {
                            for (std::size_t i = 0; i < m; ++i) {
                                std::swap(uMat(i, toIndex(k)), uMat(i, toIndex(k + 1)));
                            }
                        }
                        ++k;
                    }
                    iter = 0;
                    --pSigned;
                    break;
                }
                default:
                    converged = false;
                    pSigned = 0;
                    break;
            }
        }

        if (transposedInput) {
            // We computed SVD(A^T) = U' * diag(s) * V'^T, i.e. A = V' * diag(s) * U'^T,
            // so the U and V of A are V' and U' respectively.
            output.u = vMat;
            output.s = s;
            output.v = uMat;
        } else {
            output.u = uMat;
            output.s = s;
            output.v = vMat;
        }
        output.success = converged;
        return output;
    }
};

}  // namespace svd
}  // namespace ez

#ifdef _MSC_VER
#pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
