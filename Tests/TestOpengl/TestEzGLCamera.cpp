
#include <ezlibs/ezMath/ezMath.hpp>
#include <ezlibs/ezGL/camera.hpp>
#include <ezlibs/ezCTest.hpp>

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
bool TestEzGL_Camera_TurntableInitialState() {
    // Defaults: pivot at origin, yaw=0, pitch=0, distance=3.
    // Expected: eye at (0, 0, 3), target at origin, up at (0, 1, 0).
    ez::gl::Camera3DTurntable<T> turntable;
    turntable.update();
    const auto position = turntable.getPosition();
    const auto target = turntable.getTarget();
    const auto up = turntable.getUp();
    const T tolerance = static_cast<T>(1e-5);
    CTEST_ASSERT(ez::math::isEqual(position[0], static_cast<T>(0), tolerance));
    CTEST_ASSERT(ez::math::isEqual(position[1], static_cast<T>(0), tolerance));
    CTEST_ASSERT(ez::math::isEqual(position[2], static_cast<T>(3), tolerance));
    CTEST_ASSERT(ez::math::isEqual(target[0], static_cast<T>(0), tolerance));
    CTEST_ASSERT(ez::math::isEqual(target[1], static_cast<T>(0), tolerance));
    CTEST_ASSERT(ez::math::isEqual(target[2], static_cast<T>(0), tolerance));
    CTEST_ASSERT(ez::math::isEqual(up[0], static_cast<T>(0), tolerance));
    CTEST_ASSERT(ez::math::isEqual(up[1], static_cast<T>(1), tolerance));
    CTEST_ASSERT(ez::math::isEqual(up[2], static_cast<T>(0), tolerance));
    return true;
}

template <typename T>
bool TestEzGL_Camera_TurntableYawRotatesAroundY() {
    // yaw = pi/2 with pitch = 0 and distance = 3 should land the eye on +X.
    ez::gl::Camera3DTurntable<T> turntable;
    turntable.setYaw(static_cast<T>(1.57079632679489661923));  // pi/2
    turntable.update();
    const auto position = turntable.getPosition();
    const T tolerance = static_cast<T>(1e-5);
    CTEST_ASSERT(ez::math::isEqual(position[0], static_cast<T>(3), tolerance));
    CTEST_ASSERT(ez::math::isEqual(position[1], static_cast<T>(0), tolerance));
    CTEST_ASSERT(ez::math::isEqual(position[2], static_cast<T>(0), tolerance));
    return true;
}

template <typename T>
bool TestEzGL_Camera_TurntablePitchLiftsEye() {
    // pitch > 0 should put the eye above the pivot (positive Y).
    ez::gl::Camera3DTurntable<T> turntable;
    turntable.setPitch(static_cast<T>(0.7853981633974483));  // pi/4
    turntable.update();
    const auto position = turntable.getPosition();
    // sin(pi/4) * 3 ≈ 2.1213
    const T expectedY = static_cast<T>(std::sin(0.7853981633974483) * 3.0);
    const T tolerance = static_cast<T>(1e-5);
    CTEST_ASSERT(ez::math::isEqual(position[1], expectedY, tolerance));
    CTEST_ASSERT(position[1] > static_cast<T>(0));
    return true;
}

template <typename T>
bool TestEzGL_Camera_TurntablePitchIsClamped() {
    // Pushing pitch past pi/2 must be clamped to (pi/2 - epsilon).
    ez::gl::Camera3DTurntable<T> turntable;
    turntable.setPitch(static_cast<T>(10.0));  // way above pi/2
    const T limit = static_cast<T>(1.57079632679489661923) - static_cast<T>(0.01);
    const T tolerance = static_cast<T>(1e-5);
    CTEST_ASSERT(ez::math::isEqual(turntable.getPitch(), limit, tolerance));

    turntable.setPitch(static_cast<T>(-10.0));
    CTEST_ASSERT(ez::math::isEqual(turntable.getPitch(), -limit, tolerance));
    return true;
}

template <typename T>
bool TestEzGL_Camera_TurntableAddYawAndPitchAccumulate() {
    ez::gl::Camera3DTurntable<T> turntable;
    turntable.addYaw(static_cast<T>(0.5));
    turntable.addYaw(static_cast<T>(0.3));
    turntable.addPitch(static_cast<T>(0.2));
    turntable.addPitch(static_cast<T>(0.1));
    const T tolerance = static_cast<T>(1e-5);
    CTEST_ASSERT(ez::math::isEqual(turntable.getYaw(), static_cast<T>(0.8), tolerance));
    CTEST_ASSERT(ez::math::isEqual(turntable.getPitch(), static_cast<T>(0.3), tolerance));
    return true;
}

template <typename T>
bool TestEzGL_Camera_TurntableMultiplyDistance() {
    ez::gl::Camera3DTurntable<T> turntable;  // default distance = 3
    turntable.multiplyDistance(static_cast<T>(2));
    const T tolerance = static_cast<T>(1e-5);
    CTEST_ASSERT(ez::math::isEqual(turntable.getDistance(), static_cast<T>(6), tolerance));
    turntable.multiplyDistance(static_cast<T>(0.25));
    CTEST_ASSERT(ez::math::isEqual(turntable.getDistance(), static_cast<T>(1.5), tolerance));
    return true;
}

template <typename T>
bool TestEzGL_Camera_TurntableDistanceClampedToMin() {
    ez::gl::Camera3DTurntable<T> turntable;
    turntable.setMinDistance(static_cast<T>(0.5));
    turntable.setDistance(static_cast<T>(0.1));  // below the min
    const T tolerance = static_cast<T>(1e-5);
    CTEST_ASSERT(ez::math::isEqual(turntable.getDistance(), static_cast<T>(0.5), tolerance));
    return true;
}

template <typename T>
bool TestEzGL_Camera_TurntableSetAngleIsAliasForSetYaw() {
    // setAngle is a backwards-compatibility alias kept for the older API.
    ez::gl::Camera3DTurntable<T> turntable;
    turntable.setAngle(static_cast<T>(0.42));
    const T tolerance = static_cast<T>(1e-5);
    CTEST_ASSERT(ez::math::isEqual(turntable.getYaw(), static_cast<T>(0.42), tolerance));
    return true;
}

template <typename T>
bool TestEzGL_Camera_ComputeViewMatrixInstantiates() {
    // Regression test: Camera3D::computeViewMatrix calls mat4<T>::LookAt,
    // and the function template must instantiate cleanly. Until anyone
    // actually called it, a vec3-vs-std::array signature mismatch went
    // undetected. We exercise the call here on default state and check
    // the resulting matrix is non-degenerate.
    ez::gl::Camera3DTurntable<T> turntable;
    turntable.update();
    const ez::math::mat4<T> viewMatrix = turntable.computeViewMatrix();
    bool anyNonZero = false;
    for (int i = 0; i < 16; ++i) {
        if (viewMatrix.data()[i] != T(0)) {
            anyNonZero = true;
            break;
        }
    }
    CTEST_ASSERT(anyNonZero);
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzGL_Camera(const std::string& vTest) {
    IfTestExist(TestEzGL_Camera_TurntableInitialState<float>);
    else IfTestExist(TestEzGL_Camera_TurntableInitialState<double>);
    else IfTestExist(TestEzGL_Camera_TurntableYawRotatesAroundY<float>);
    else IfTestExist(TestEzGL_Camera_TurntableYawRotatesAroundY<double>);
    else IfTestExist(TestEzGL_Camera_TurntablePitchLiftsEye<float>);
    else IfTestExist(TestEzGL_Camera_TurntablePitchLiftsEye<double>);
    else IfTestExist(TestEzGL_Camera_TurntablePitchIsClamped<float>);
    else IfTestExist(TestEzGL_Camera_TurntablePitchIsClamped<double>);
    else IfTestExist(TestEzGL_Camera_TurntableAddYawAndPitchAccumulate<float>);
    else IfTestExist(TestEzGL_Camera_TurntableAddYawAndPitchAccumulate<double>);
    else IfTestExist(TestEzGL_Camera_TurntableMultiplyDistance<float>);
    else IfTestExist(TestEzGL_Camera_TurntableMultiplyDistance<double>);
    else IfTestExist(TestEzGL_Camera_TurntableDistanceClampedToMin<float>);
    else IfTestExist(TestEzGL_Camera_TurntableDistanceClampedToMin<double>);
    else IfTestExist(TestEzGL_Camera_TurntableSetAngleIsAliasForSetYaw<float>);
    else IfTestExist(TestEzGL_Camera_TurntableSetAngleIsAliasForSetYaw<double>);
    else IfTestExist(TestEzGL_Camera_ComputeViewMatrixInstantiates<float>);
    else IfTestExist(TestEzGL_Camera_ComputeViewMatrixInstantiates<double>);
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
