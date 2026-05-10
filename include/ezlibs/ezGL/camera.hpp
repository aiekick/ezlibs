#pragma once

/*
MIT License

Copyright (c) 2014-2024 Stephane Cuillerdier (aka aiekick)

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

// ez::gl::camera is part of the ezLibs project : https://github.com/aiekick/ezLibs.git

#include <array>
#include <cmath>
#include "../ezMath/ezVec3.hpp"
#include "../ezMath/ezMat4.hpp"

namespace ez {
namespace gl {

template <typename T>
class Camera3D {
    static_assert(std::is_arithmetic<T>::value, "Camera3D requires arithmetic T");
    math::vec3<T> m_position{T(0), T(0), T(1)};
    math::vec3<T> m_target{T(0), T(0), T(0)};
    math::vec3<T> m_up{T(0), T(1), T(0)};

public:  // methods
    Camera3D() {}

    // --- Setters/Getters ---
    void setPosition(const math::vec3<T>& vPosition) { m_position = vPosition; }
    void setTarget(const math::vec3<T>& vTarget) { m_target = vTarget; }
    void setUp(const math::vec3<T>& vUp) { m_up = vUp; }

    const math::vec3<T>& getPosition() const { return m_position; }
    const math::vec3<T>& getTarget() const { return m_target; }
    const math::vec3<T>& getUp() const { return m_up; }

    // --- Core matrices ---
    // View matrix (right-handed). LookAt takes std::array<T, 3>; the
    // members are math::vec3<T>, so we pack them explicitly.
    math::mat4<T> computeViewMatrix() const {
        return math::mat4<T>::LookAt(
            std::array<T, 3>{{m_position.x, m_position.y, m_position.z}},
            std::array<T, 3>{{m_target.x,   m_target.y,   m_target.z  }},
            std::array<T, 3>{{m_up.x,       m_up.y,       m_up.z      }});
    }

    // Perspective (OpenGL clip space: z in [-1,1])
    static math::mat4<T> makeGLPerspective(T vFovYRadians, T vAspect, T vNear, T vFar) { return math::mat4<T>::PerspectiveGL(vFovYRadians, vAspect, vNear, vFar); }

    // Perspective (Vulkan clip space: z in [0,1] + Y flip)
    static math::mat4<T> makeVKPerspective(T vFovYRadians, T vAspect, T vNear, T vFar) { return math::mat4<T>::PerspectiveVK(vFovYRadians, vAspect, vNear, vFar); }

    // Orthographic (OpenGL)
    static math::mat4<T> makeGLOrtho(T vLeft, T vRight, T vBottom, T vTop, T vNear, T vFar) { return math::mat4<T>::OrthoGL(vLeft, vRight, vBottom, vTop, vNear, vFar); }

    // Orthographic (Vulkan)
    static math::mat4<T> makeVKOrtho(T vLeft, T vRight, T vBottom, T vTop, T vNear, T vFar) { return math::mat4<T>::OrthoVK(vLeft, vRight, vBottom, vTop, vNear, vFar); }

    // Frustum (OpenGL)
    static math::mat4<T> makeGLFrustum(T vLeft, T vRight, T vBottom, T vTop, T vNear, T vFar) { return math::mat4<T>::FrustumGL(vLeft, vRight, vBottom, vTop, vNear, vFar); }

    // Frustum (Vulkan)
    static math::mat4<T> makeVKFrustum(T vLeft, T vRight, T vBottom, T vTop, T vNear, T vFar) { return math::mat4<T>::FrustumVK(vLeft, vRight, vBottom, vTop, vNear, vFar); }
};

template <typename T>
class Camera3DOrbit : public Camera3D<T> {
    math::vec3<T> m_orbitTarget{T(0), T(0), T(0)};
    T m_distance{T(3)};
    T m_yawRadians{T(0)};    // azimuth around world up (y)
    T m_pitchRadians{T(0)};  // elevation
    T m_minDistance{T(0.01)};

public:
    void setOrbitTarget(const math::vec3<T>& vTarget) { m_orbitTarget = vTarget; }
    void setDistance(T vDistance) { m_distance = (vDistance < m_minDistance) ? m_minDistance : vDistance; }
    void setAngles(T vYawRadians, T vPitchRadians) {
        m_yawRadians = vYawRadians;
        m_pitchRadians = vPitchRadians;
    }
    void setMinDistance(T vMinDistance) {
        m_minDistance = (vMinDistance <= T(0)) ? T(0.000001) : vMinDistance;
        if (m_distance < m_minDistance)
            m_distance = m_minDistance;
    }

    // Update base camera position and target from orbit parameters
    void update() {
        const T cosYaw = static_cast<T>(std::cos(static_cast<double>(m_yawRadians)));
        const T sinYaw = static_cast<T>(std::sin(static_cast<double>(m_yawRadians)));
        const T cosPitch = static_cast<T>(std::cos(static_cast<double>(m_pitchRadians)));
        const T sinPitch = static_cast<T>(std::sin(static_cast<double>(m_pitchRadians)));

        // World axes: X(right), Y(up), Z(forward)
        const math::vec3<T> worldRight{T(1), T(0), T(0)};
        const math::vec3<T> worldUp{T(0), T(1), T(0)};
        const math::vec3<T> worldFwd{T(0), T(0), T(1)};

        // Spherical direction from yaw/pitch (right-handed)
        // Forward dir in world space:
        math::vec3<T> forwardDir = {
            cosPitch * sinYaw,  // x
            sinPitch,           // y
            cosPitch * cosYaw   // z
        };

        // Eye = target - forward * distance
        math::vec3<T> eye = {m_orbitTarget[0] - forwardDir[0] * m_distance, m_orbitTarget[1] - forwardDir[1] * m_distance, m_orbitTarget[2] - forwardDir[2] * m_distance};

        this->setPosition(eye);
        this->setTarget(m_orbitTarget);
        this->setUp(worldUp);
    }
};

// Y-up turntable camera.
//
// Two degrees of freedom around a pivot:
//   - yaw   : rotation around the world Y axis (azimuth)
//   - pitch : rotation around the camera's local right axis (elevation),
//             clamped to (-pi/2 + eps, +pi/2 - eps) so the world up vector
//             stays world Y at all times — i.e. the user can never flip
//             the scene upside down.
//
// Plus one linear DoF:
//   - distance : how far the eye is from the pivot, with a configurable
//                lower bound to prevent the eye from collapsing onto the
//                target.
//
// Spherical eye position (when pivot is origin and pitch/yaw are 0, the
// eye is on the +Z half-axis looking back toward origin):
//
//     eye = pivot + ( cos(pitch) * sin(yaw),
//                     sin(pitch),
//                     cos(pitch) * cos(yaw) ) * distance
//
// Mouse-friendly setters (addYaw, addPitch, multiplyDistance) are provided
// for incremental input deltas. setAngle/setAxis exist as backwards-
// compatibility aliases for the older single-axis turntable.
template <typename T>
class Camera3DTurntable : public Camera3D<T> {
    math::vec3<T> m_pivot{T(0), T(0), T(0)};
    T m_yawRadians{T(0)};
    T m_pitchRadians{T(0)};
    T m_distance{T(3)};
    T m_minDistance{static_cast<T>(0.01)};

    // Half-pi constant; std::numbers::pi_v is C++20 and ezlibs is C++11.
    // Hard-coded as a plain literal to keep the header self-contained.
    static constexpr T k_halfPi = static_cast<T>(1.57079632679489661923);
    // Tiny offset so the pitch never reaches exactly +/- pi/2 (gimbal lock
    // would make the up vector degenerate).
    static constexpr T k_pitchEpsilon = static_cast<T>(0.01);

public:
    void setPivot(const math::vec3<T>& vPivot) { m_pivot = vPivot; }
    void setYaw(T vYawRadians) { m_yawRadians = vYawRadians; }
    void setPitch(T vPitchRadians) { m_pitchRadians = m_clampPitch(vPitchRadians); }
    void setDistance(T vDistance) { m_distance = (vDistance < m_minDistance) ? m_minDistance : vDistance; }
    void setMinDistance(T vMinDistance) {
        m_minDistance = (vMinDistance <= T(0)) ? static_cast<T>(0.000001) : vMinDistance;
        if (m_distance < m_minDistance) {
            m_distance = m_minDistance;
        }
    }

    T getYaw() const { return m_yawRadians; }
    T getPitch() const { return m_pitchRadians; }
    T getDistance() const { return m_distance; }
    const math::vec3<T>& getPivot() const { return m_pivot; }

    // Apply an incremental delta. Pitch is automatically clamped.
    void addYaw(T vDeltaRadians) { m_yawRadians += vDeltaRadians; }
    void addPitch(T vDeltaRadians) { m_pitchRadians = m_clampPitch(m_pitchRadians + vDeltaRadians); }
    // Multiplicative zoom is more natural with mouse-wheel events than an
    // additive delta (a fixed wheel notch doubles or halves the distance
    // regardless of the current scale).
    void multiplyDistance(T vFactor) { setDistance(m_distance * vFactor); }

    // Backwards-compatibility aliases for the older single-axis turntable.
    // setAxis is intentionally a no-op: this turntable assumes world Y up.
    void setAngle(T vYawRadians) { setYaw(vYawRadians); }
    void setAxis(const math::vec3<T>& /*vAxisNorm*/) {}

    void update() {
        const T cosYaw = static_cast<T>(std::cos(static_cast<double>(m_yawRadians)));
        const T sinYaw = static_cast<T>(std::sin(static_cast<double>(m_yawRadians)));
        const T cosPitch = static_cast<T>(std::cos(static_cast<double>(m_pitchRadians)));
        const T sinPitch = static_cast<T>(std::sin(static_cast<double>(m_pitchRadians)));

        // Spherical coordinates around pivot, world Y up. At yaw=pitch=0
        // the eye sits on the +Z half-axis at distance from the pivot.
        // Members .x/.y/.z are accessed directly because math::vec3<T>::operator[]
        // is non-const and we want to keep `offset` const.
        const math::vec3<T> offset = {
            cosPitch * sinYaw * m_distance,
            sinPitch * m_distance,
            cosPitch * cosYaw * m_distance};

        this->setPosition({m_pivot.x + offset.x, m_pivot.y + offset.y, m_pivot.z + offset.z});
        this->setTarget(m_pivot);
        this->setUp({T(0), T(1), T(0)});
    }

private:
    T m_clampPitch(T vRadians) const {
        const T limit = k_halfPi - k_pitchEpsilon;
        if (vRadians > limit) return limit;
        if (vRadians < -limit) return -limit;
        return vRadians;
    }
};

template <typename T>
class Camera3DFreeFlight : public Camera3D<T> {
    math::vec3<T> m_forward{T(0), T(0), T(1)};  // normalized
    math::vec3<T> m_right{T(1), T(0), T(0)};    // normalized
    math::vec3<T> m_up{T(0), T(1), T(0)};       // normalized

public:
    // Init from yaw/pitch and position
    void setFromYawPitch(const math::vec3<T>& vPosition, T vYawRadians, T vPitchRadians) {
        const T cosYaw = static_cast<T>(std::cos(static_cast<double>(vYawRadians)));
        const T sinYaw = static_cast<T>(std::sin(static_cast<double>(vYawRadians)));
        const T cosPitch = static_cast<T>(std::cos(static_cast<double>(vPitchRadians)));
        const T sinPitch = static_cast<T>(std::sin(static_cast<double>(vPitchRadians)));

        m_forward = {cosPitch * sinYaw, sinPitch, cosPitch * cosYaw};
        m_right = {cosYaw, T(0), -sinYaw};
        m_up = {-sinPitch * sinYaw, cosPitch, -sinPitch * cosYaw};

        this->setPosition(vPosition);
        this->setTarget({vPosition[0] + m_forward[0], vPosition[1] + m_forward[1], vPosition[2] + m_forward[2]});
        this->setUp(m_up);
    }

    // Move along local axes
    void moveLocal(T vDeltaForward, T vDeltaRight, T vDeltaUp) {
        const math::vec3<T> pos = this->getPosition();
        math::vec3<T> newPos = {
            pos[0] + m_forward[0] * vDeltaForward + m_right[0] * vDeltaRight + m_up[0] * vDeltaUp,
            pos[1] + m_forward[1] * vDeltaForward + m_right[1] * vDeltaRight + m_up[1] * vDeltaUp,
            pos[2] + m_forward[2] * vDeltaForward + m_right[2] * vDeltaRight + m_up[2] * vDeltaUp};
        this->setPosition(newPos);
        this->setTarget({newPos[0] + m_forward[0], newPos[1] + m_forward[1], newPos[2] + m_forward[2]});
        this->setUp(m_up);
    }
};

}  // namespace gl
} // namespace ez
