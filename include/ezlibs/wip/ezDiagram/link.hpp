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

// ezDiagram is part of the ezLibs project : https://github.com/aiekick/ezLibs.git

#include <cmath>
#include <vector>
#include <cstdint>

#include <imguipack.h>

#include "utils.hpp"
#include "interfaces/icanvas.h"
#include "interfaces/ilink.h"

class Link : public ILink {
    Datas m_datas;
    const char* mp_name{};

public:
    Link() = default;
    Link(
        int32_t aSrcNodeID,      // the source node id
        int32_t aSrcNodeSlotID,  // the slot id of the source node
        int32_t aDstNodeID,      // the target node id
        int32_t aDstNodeSlotID,  // the slot id of the target node
        int32_t aNumMidPoints)   // the middles points count, 0 mean a line
    {
        m_datas.srcNodeID = aSrcNodeID;
        m_datas.srcNodeSlotID = aSrcNodeSlotID;
        m_datas.dstNodeID = aDstNodeID;
        m_datas.dstNodeSlotID = aDstNodeSlotID;
        m_datas.color = IM_COL32(255, 255, 255, 255);
        m_datas.corners.resize(aNumMidPoints + 2);  // + 2 extermities
    }
    Datas& rDatas() override {
        return m_datas;
    }
    const Datas& getDatas() const override {
        return m_datas;
    }
    void draw(const ICanvas& arCanvas) const override {
        const float canvas_scale = arCanvas.getDatas().scale;
        auto* pDrawList = ImGui::GetWindowDrawList();
        float th = 2.0f * canvas_scale;
        for (size_t i = 0; i + 1 < m_datas.corners.size(); ++i) {
            pDrawList->AddLine(
                arCanvas.worldToLocal(m_datas.corners[i]),       // line p0
                arCanvas.worldToLocal(m_datas.corners[i + 1]),  // line p1
                m_datas.color,                             // line color
                th                                         // line thinckness
            );
        }

        // Arrow
        if (m_datas.corners.size() > 1) {
            ImVec2 tip = arCanvas.worldToLocal(m_datas.corners.back());
            ImVec2 d = m_datas.corners.back() - m_datas.corners[m_datas.corners.size() - 2U];
            float l = std::sqrt(ImLengthSqr(d));
            if (l > 0.1f) {
                d = d * (1.0f / l);
                float as = 8.0f * canvas_scale;
                ImVec2 pp = {-d.y, d.x};
                pDrawList->AddTriangleFilled(
                    tip,                                                                         // triangle p0
                    {tip.x - d.x * as + pp.x * as * 0.4f, tip.y - d.y * as + pp.y * as * 0.4f},  // triangle p1
                    {tip.x - d.x * as - pp.x * as * 0.4f, tip.y - d.y * as - pp.y * as * 0.4f},  // triangle p2
                    m_datas.color                                                                // triangle color
                );
            }
        }

#ifdef _DEBUG
        // Debug: m_corners
        for (size_t i = 1; i < m_datas.corners.size() - 1; ++i) {
            pDrawList->AddCircleFilled(
                arCanvas.worldToLocal(m_datas.corners[i]),  // circle center
                3.0f * canvas_scale,                   // circle radius
                IM_COL32(255, 255, 0, 150),            // circle color
                4                                      // circle segments count
            );
        }
#endif
    }
};

