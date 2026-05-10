#include <ezlibs/ezMat4.hpp>
#include <ezlibs/ezMath.hpp>
#include <ezlibs/ezCTest.hpp>

#include <array>
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

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

template <typename T>
bool TestEzMat4_LookAtMapsEyeToOrigin() {
    // The view matrix maps the eye position to the view-space origin.
    // Eye at (0, 0, 5), target at origin, up Y. Apply the matrix to the
    // eye expressed in homogeneous coords and expect (0, 0, 0, 1).
    const std::array<T, 3> eye{T(0), T(0), T(5)};
    const std::array<T, 3> target{T(0), T(0), T(0)};
    const std::array<T, 3> up{T(0), T(1), T(0)};
    const ez::mat4<T> view = ez::mat4<T>::LookAt(eye, target, up);
    const std::array<T, 4> mapped = view.mulVec({eye[0], eye[1], eye[2], T(1)});
    const T tolerance = static_cast<T>(1e-5);
    CTEST_ASSERT(ez::isEqual(mapped[0], T(0), tolerance));
    CTEST_ASSERT(ez::isEqual(mapped[1], T(0), tolerance));
    CTEST_ASSERT(ez::isEqual(mapped[2], T(0), tolerance));
    CTEST_ASSERT(ez::isEqual(mapped[3], T(1), tolerance));
    return true;
}

template <typename T>
bool TestEzMat4_LookAtMapsTargetToNegativeZ() {
    // The view matrix maps the look-at target to (0, 0, -d, 1) in view
    // space, where d is the eye-to-target distance and the camera looks
    // down -Z (RH OpenGL convention).
    const std::array<T, 3> eye{T(0), T(0), T(5)};
    const std::array<T, 3> target{T(0), T(0), T(0)};
    const std::array<T, 3> up{T(0), T(1), T(0)};
    const ez::mat4<T> view = ez::mat4<T>::LookAt(eye, target, up);
    const std::array<T, 4> mapped = view.mulVec({target[0], target[1], target[2], T(1)});
    const T tolerance = static_cast<T>(1e-5);
    CTEST_ASSERT(ez::isEqual(mapped[0], T(0), tolerance));
    CTEST_ASSERT(ez::isEqual(mapped[1], T(0), tolerance));
    CTEST_ASSERT(ez::isEqual(mapped[2], T(-5), tolerance));
    CTEST_ASSERT(ez::isEqual(mapped[3], T(1), tolerance));
    return true;
}

template <typename T>
bool TestEzMat4_PerspectiveGLClipWEqualsMinusViewZ() {
    // For a column-vector perspective matrix in GL convention,
    // gl_Position.w = -view_space_z. Verify with a point at view-space
    // (0, 0, -5, 1): expect clip.w = 5 (the perspective division
    // factor).
    const T fovY = static_cast<T>(1.5707963267948966);  // 90 degrees
    const T aspect = T(1);
    const T dist_near = T(1);  // near is a reserved token on msvc
    const T dist_far = T(10);  // far is a reserved token on msvc
    const ez::mat4<T> proj = ez::mat4<T>::PerspectiveGL(fovY, aspect, dist_near, dist_far);
    const std::array<T, 4> mapped = proj.mulVec({T(0), T(0), T(-5), T(1)});
    const T tolerance = static_cast<T>(1e-5);
    CTEST_ASSERT(ez::isEqual(mapped[0], T(0), tolerance));
    CTEST_ASSERT(ez::isEqual(mapped[1], T(0), tolerance));
    CTEST_ASSERT(ez::isEqual(mapped[3], T(5), tolerance));
    return true;
}

template <typename T>
bool TestEzMat4_OrthoGLMapsCornerToNDCCorner() {
    // Off-center ortho: (right, top, -dist_far, 1) should map to NDC (1, 1, 1, 1)
    // in OpenGL (z in [-1, 1]). Use a symmetric box for clarity.
    const T left = T(-2);
    const T right = T(2);
    const T bottom = T(-3);
    const T top = T(3);
    const T dist_near = T(1);  // near is a reserved token on msvc
    const T dist_far = T(10);  // far is a reserved token on msvc
    const ez::mat4<T> ortho = ez::mat4<T>::OrthoGL(left, right, bottom, top, dist_near, dist_far);
    // The corner at view-space (right, top, -dist_far) should land on the NDC
    // corner (+1, +1, +1).
    const std::array<T, 4> mapped = ortho.mulVec({right, top, -dist_far, T(1)});
    const T tolerance = static_cast<T>(1e-5);
    CTEST_ASSERT(ez::isEqual(mapped[0], T(1), tolerance));
    CTEST_ASSERT(ez::isEqual(mapped[1], T(1), tolerance));
    CTEST_ASSERT(ez::isEqual(mapped[2], T(1), tolerance));
    CTEST_ASSERT(ez::isEqual(mapped[3], T(1), tolerance));
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzMat4(const std::string& vTest) {
    IfTestExist(TestEzMat4_LookAtMapsEyeToOrigin<float>);
    else IfTestExist(TestEzMat4_LookAtMapsEyeToOrigin<double>);
    else IfTestExist(TestEzMat4_LookAtMapsTargetToNegativeZ<float>);
    else IfTestExist(TestEzMat4_LookAtMapsTargetToNegativeZ<double>);
    else IfTestExist(TestEzMat4_PerspectiveGLClipWEqualsMinusViewZ<float>);
    else IfTestExist(TestEzMat4_PerspectiveGLClipWEqualsMinusViewZ<double>);
    else IfTestExist(TestEzMat4_OrthoGLMapsCornerToNDCCorner<float>);
    else IfTestExist(TestEzMat4_OrthoGLMapsCornerToNDCCorner<double>);
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
