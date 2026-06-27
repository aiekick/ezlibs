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

// ezGL is part of the ezLibs project : https://github.com/aiekick/ezLibs.git

/* SdfMap
 * GPU-baked signed distance field of a set of axis-aligned boxes.
 *
 * A compute shader evaluates, per texel of an R32F image over a world-space
 * window, the signed distance to the UNION of the boxes (< 0 inside, same
 * convention and formula as ez::math::sdfToBoxes). The field is read back once
 * into a CPU buffer so the host can sample it in O(1) -- a drop-in replacement
 * for the per-point ez::math::sdfToBoxes loop (which is O(boxes) per query).
 *
 * Requires a GL 4.3+ context (compute shader + SSBO + image store).
 */

#include "../defs.hpp"
#include "../utils.hpp"

#ifdef OPENGL_LOADER
#include OPENGL_LOADER
#endif  // OPENGL_LOADER

#include <ezlibs/ezMath/ezVec2.hpp>

#include <string>
#include <vector>
#include <memory>

namespace ez {
namespace gl {

class SdfMap {
public:
    // One obstacle box, std430-compatible (two vec2 = 16 bytes, array stride 16).
    struct BoxData {
        math::fvec2 bmin;
        math::fvec2 bmax;
    };

public:
    static SdfMapPtr create(const std::string& vName) {
        auto res = std::make_shared<SdfMap>();
        if (!res->init(vName)) {
            res.reset();
        }
        return res;
    }

private:
    std::string m_Name;
    ShaderPtr m_CompShaderPtr = nullptr;
    ProgramPtr m_ProgramPtr = nullptr;
    SSBOPtr<BoxData> m_BoxSsboPtr = nullptr;
    TexturePtr m_SdfTexPtr = nullptr;      // R32F : the signed field, read back to CPU for sampling
    TexturePtr m_PreviewTexPtr = nullptr;  // RGBA8 : isoline-colorized field, for on-canvas debug display
    std::vector<float> m_Field;  // CPU read-back of the SDF image, row-major (resX * resY)
    GLsizei m_ResX = 0;
    GLsizei m_ResY = 0;
    math::fvec2 m_WorldMin;
    math::fvec2 m_WorldMax;
    float m_ColorDistScale = 50.0f;  // distance normalization for the isoline coloring (world units)
    bool m_Baked = false;            // true once a dispatch ran (GPU textures valid), even without a CPU read-back

public:
    SdfMap() = default;
    ~SdfMap() { unit(); }

    bool init(const std::string& vName) {
        ASSERT_THROW(!vName.empty(), "");
        m_Name = vName;
        m_CompShaderPtr = Shader::createFromCode(vName, GL_COMPUTE_SHADER, m_getComputeCode());
        if (m_CompShaderPtr == nullptr) {
            return false;
        }
        m_ProgramPtr = Program::create(vName);
        if (m_ProgramPtr == nullptr) {
            return false;
        }
        if (!m_ProgramPtr->addShader(m_CompShaderPtr)) {
            return false;
        }
        if (!m_ProgramPtr->link()) {
            return false;
        }
        m_BoxSsboPtr = SSBO<BoxData>::create(vName + "_boxes");
        return (m_BoxSsboPtr != nullptr);
    }

    void unit() {
        m_BoxSsboPtr.reset();
        m_SdfTexPtr.reset();
        m_PreviewTexPtr.reset();
        m_ProgramPtr.reset();
        m_CompShaderPtr.reset();
        m_Field.clear();
        m_Baked = false;
    }

    // Bake the SDF of vBoxes over the world window [vWorldMin, vWorldMax] into a vResX x vResY R32F
    // image (+ the colorized preview). When vReadBackField is true the field is also read back into
    // the CPU buffer for sample() (the GPU->CPU sync cost) ; pass false for a GPU-only visualize bake
    // (the preview texture is enough). Returns false on a degenerate window or a missing program.
    // Recompute only when geometry changed (host side : a dirty flag) -- never per frame.
    bool bake(
        const std::vector<BoxData>& vBoxes,
        const math::fvec2& vWorldMin,
        const math::fvec2& vWorldMax,
        const GLsizei vResX,
        const GLsizei vResY,
        const bool vReadBackField = true) {
        if (m_ProgramPtr == nullptr || m_BoxSsboPtr == nullptr) {
            return false;
        }
        if (vResX <= 0 || vResY <= 0) {
            return false;
        }
        const math::fvec2 extent = vWorldMax - vWorldMin;
        if (extent.x <= 0.0f || extent.y <= 0.0f) {
            return false;
        }
        m_WorldMin = vWorldMin;
        m_WorldMax = vWorldMax;

        if (!m_ensureTexture(vResX, vResY)) {
            return false;
        }

        // upload the obstacle boxes to the SSBO (binding 0)
        m_BoxSsboPtr->getDatasRef() = vBoxes;
        m_BoxSsboPtr->upload(GL_DYNAMIC_DRAW);
        m_BoxSsboPtr->bind(0);

        // bind the field image (unit 0, R32F) and the colorized preview image (unit 1, RGBA8) ;
        // both are filled by the same dispatch.
        glBindImageTexture(0, m_SdfTexPtr->getId(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
        CheckGLErrors;
        glBindImageTexture(1, m_PreviewTexPtr->getId(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
        CheckGLErrors;

        if (!m_ProgramPtr->use()) {
            return false;
        }
        const GLuint programId = m_ProgramPtr->getProgramId();
        glUniform2i(glGetUniformLocation(programId, "uRes"), m_ResX, m_ResY);
        glUniform2f(glGetUniformLocation(programId, "uWorldMin"), m_WorldMin.x, m_WorldMin.y);
        glUniform2f(glGetUniformLocation(programId, "uWorldMax"), m_WorldMax.x, m_WorldMax.y);
        glUniform1i(glGetUniformLocation(programId, "uBoxCount"), static_cast<GLint>(vBoxes.size()));
        glUniform1f(glGetUniformLocation(programId, "uDistScale"), m_ColorDistScale);
        CheckGLErrors;

        const GLuint groupsX = static_cast<GLuint>((m_ResX + 7) / 8);
        const GLuint groupsY = static_cast<GLuint>((m_ResY + 7) / 8);
        glDispatchCompute(groupsX, groupsY, 1);
        CheckGLErrors;
        // full barrier (baked rarely, on dirty) : covers the glGetTexImage read-back of the field
        // AND the later sampler fetch of the preview texture by ImGui.
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
        CheckGLErrors;
        m_ProgramPtr->unuse();
        m_Baked = true;  // GPU textures (field + preview) are valid now

        // read the field back to the CPU only when the host will sample() it (routing). The preview
        // is a GPU texture, so a visualize-only bake skips this stall entirely.
        if (vReadBackField) {
            m_Field = m_SdfTexPtr->getPixels<float, 1>(m_ResX, m_ResY);
            return (m_Field.size() == static_cast<size_t>(m_ResX) * static_cast<size_t>(m_ResY));
        }
        m_Field.clear();  // stale CPU field would mislead sample() ; cleared until the next read-back bake
        return true;
    }

    // O(1) signed distance lookup : bilinear sampling of the baked field. Matches
    // ez::math::sdfToBoxes within the field discretization. Returns a large value
    // when nothing has been baked yet.
    float sample(const math::fvec2& vPoint) const {
        if (m_Field.empty() || m_ResX <= 0 || m_ResY <= 0) {
            return 1.0e30f;
        }
        const math::fvec2 extent = m_WorldMax - m_WorldMin;
        if (extent.x <= 0.0f || extent.y <= 0.0f) {
            return 1.0e30f;
        }
        // world -> continuous texel coords (texel centers sit at integer + 0.5)
        const float normX = (vPoint.x - m_WorldMin.x) / extent.x;
        const float normY = (vPoint.y - m_WorldMin.y) / extent.y;
        float fx = normX * static_cast<float>(m_ResX) - 0.5f;
        float fy = normY * static_cast<float>(m_ResY) - 0.5f;
        fx = m_clampF(fx, 0.0f, static_cast<float>(m_ResX - 1));
        fy = m_clampF(fy, 0.0f, static_cast<float>(m_ResY - 1));
        const int x0 = static_cast<int>(fx);
        const int y0 = static_cast<int>(fy);
        const int x1 = (x0 + 1 < m_ResX) ? (x0 + 1) : x0;
        const int y1 = (y0 + 1 < m_ResY) ? (y0 + 1) : y0;
        const float tx = fx - static_cast<float>(x0);
        const float ty = fy - static_cast<float>(y0);
        const float v00 = m_at(x0, y0);
        const float v10 = m_at(x1, y0);
        const float v01 = m_at(x0, y1);
        const float v11 = m_at(x1, y1);
        const float top = v00 + (v10 - v00) * tx;
        const float bot = v01 + (v11 - v01) * tx;
        return top + (bot - top) * ty;
    }

    const std::vector<float>& getField() const { return m_Field; }
    GLsizei getResX() const { return m_ResX; }
    GLsizei getResY() const { return m_ResY; }
    bool isBaked() const { return m_Baked; }                 // a dispatch ran (GPU textures valid)
    bool hasField() const { return !m_Field.empty(); }       // the CPU field is available for sample()

    // GL id of the baked R32F field (0 if nothing baked yet) and the world window it
    // covers : let the host draw the field as a canvas-background quad for visual
    // verification (placed at [worldMin, worldMax]) before the nodes are drawn.
    GLuint getTextureId() const { return (m_SdfTexPtr != nullptr) ? m_SdfTexPtr->getId() : 0U; }
    // GL id of the RGBA8 isoline-colorized preview : draw THIS one on the canvas background
    // (a plain ImGui image, no remap needed -- the colorization is baked in).
    GLuint getPreviewTextureId() const { return (m_PreviewTexPtr != nullptr) ? m_PreviewTexPtr->getId() : 0U; }
    const math::fvec2& getWorldMin() const { return m_WorldMin; }
    const math::fvec2& getWorldMax() const { return m_WorldMax; }
    // Distance scale of the isoline coloring : the IQ constants assume a ~unit distance, so
    // the field (in world/pixel units) is divided by this before coloring. Larger = wider bands.
    void setColorDistanceScale(const float vScale) { m_ColorDistScale = (vScale > 1.0e-3f) ? vScale : 1.0e-3f; }
    float getColorDistanceScale() const { return m_ColorDistScale; }

private:
    bool m_ensureTexture(const GLsizei vResX, const GLsizei vResY) {
        if (m_SdfTexPtr != nullptr && m_PreviewTexPtr != nullptr && m_ResX == vResX && m_ResY == vResY) {
            return true;
        }
        m_ResX = vResX;
        m_ResY = vResY;
        // R32F field (channels = 1, GL_FLOAT) from a zeroed buffer ; createEmpty would force
        // RGBA32F, so go through createFromBuffer. nearest filtering : the CPU read-back samples it.
        const std::vector<float> zeros(static_cast<size_t>(vResX) * static_cast<size_t>(vResY), 0.0f);
        m_SdfTexPtr = Texture::createFromBuffer(
            reinterpret_cast<const uint8_t*>(zeros.data()), vResX, vResY, 1, GL_FLOAT, "clamp", "nearest", false);
        if (m_SdfTexPtr == nullptr) {
            return false;
        }
        // RGBA8 colorized preview, linear filtering for a smooth on-canvas display.
        const std::vector<uint8_t> zerosRgba(static_cast<size_t>(vResX) * static_cast<size_t>(vResY) * 4U, 0U);
        m_PreviewTexPtr = Texture::createFromBuffer(zerosRgba.data(), vResX, vResY, 4, GL_UNSIGNED_BYTE, "clamp", "linear", false);
        return (m_PreviewTexPtr != nullptr);
    }

    float m_at(const int vX, const int vY) const {
        return m_Field[static_cast<size_t>(vY) * static_cast<size_t>(m_ResX) + static_cast<size_t>(vX)];
    }

    static float m_clampF(const float vValue, const float vMin, const float vMax) {
        if (vValue < vMin) {
            return vMin;
        }
        if (vValue > vMax) {
            return vMax;
        }
        return vValue;
    }

    static std::string m_getComputeCode() {
        // clang-format off
        return
            "#version 430\n"
            "layout(local_size_x = 8, local_size_y = 8) in;\n"
            "layout(r32f, binding = 0) writeonly uniform image2D uSdfImage;\n"
            "layout(rgba8, binding = 1) writeonly uniform image2D uPreview;\n"
            "struct Box { vec2 bmin; vec2 bmax; };\n"
            "layout(std430, binding = 0) readonly buffer Boxes { Box boxes[]; };\n"
            "uniform ivec2 uRes;\n"
            "uniform vec2 uWorldMin;\n"
            "uniform vec2 uWorldMax;\n"
            "uniform int uBoxCount;\n"
            "uniform float uDistScale;\n"
            "float sdBox(vec2 p, vec2 bmin, vec2 bmax) {\n"
            "    vec2 center = 0.5 * (bmin + bmax);\n"
            "    vec2 halfExtent = 0.5 * (bmax - bmin);\n"
            "    vec2 d = abs(p - center) - halfExtent;\n"
            "    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);\n"
            "}\n"
            "void main() {\n"
            "    ivec2 px = ivec2(gl_GlobalInvocationID.xy);\n"
            "    if (px.x >= uRes.x || px.y >= uRes.y) { return; }\n"
            "    vec2 uv = (vec2(px) + 0.5) / vec2(uRes);\n"
            "    vec2 p = mix(uWorldMin, uWorldMax, uv);\n"
            "    float best = 1.0e30;\n"
            "    for (int i = 0; i < uBoxCount; ++i) {\n"
            "        best = min(best, sdBox(p, boxes[i].bmin, boxes[i].bmax));\n"
            "    }\n"
            "    imageStore(uSdfImage, px, vec4(best, 0.0, 0.0, 0.0));\n"
            "    // IQ-style distance-field visualization (sign tint + falloff + isolines).\n"
            "    // The constants assume a ~unit distance, so normalize the world-unit field first.\n"
            "    float dv = best / uDistScale;\n"
            "    vec3 col = vec3(1.0) - sign(best) * vec3(0.1, 0.4, 0.7);\n"
            "    col *= 1.0 - exp(-2.0 * abs(dv));\n"
            "    col *= 0.8 + 0.2 * cos(140.0 * dv);\n"
            "    col = mix(col, vec3(1.0), 1.0 - smoothstep(0.0, 0.02, abs(dv)));\n"
            "    imageStore(uPreview, px, vec4(col, 1.0));\n"
            "}\n";
        // clang-format on
    }
};

// std430 requires the obstacle struct to pack as two tightly-laid vec2 (stride 16).
static_assert(sizeof(SdfMap::BoxData) == 4 * sizeof(float), "SdfMap::BoxData must match std430 layout (two vec2)");

}  // namespace gl
}  // namespace ez
