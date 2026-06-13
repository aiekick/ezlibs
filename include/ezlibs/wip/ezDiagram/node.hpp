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

#include <vector>
#include <cstdint>

#include "utils.hpp"
#include "interfaces/icanvas.h"
#include "interfaces/inode.h"

class Node : public INode {
    Datas m_datas;
    float m_x_padding{10.0f};
    float m_y_padding{15.0f};
    float m_y_spacing{15.0f};

public:
    Node() = default;
    Node(const std::string& aName, int32_t aSlotsCount) {
        m_datas.name = aName;
        float slot_y{m_x_padding};
        for (int32_t i = 0; i < aSlotsCount; ++i) {
            slot_y += m_y_spacing;
            m_datas.slots_y.push_back(slot_y);
        }
        m_datas.pos = ImVec2(0.0f, 0.0f);
        m_datas.size.x = ImGui::CalcTextSize(m_datas.name.c_str()).x + m_y_padding * 2.0f;
        m_datas.size.y = ((!m_datas.slots_y.empty()) ? m_datas.slots_y.back() : 0.0f) + m_x_padding;
    }
    Datas& rDatas()override {
        return m_datas;
    }
    const Datas& getDatas() const override {
        return m_datas;
    }
    void draw(const ICanvas& arCanvas) const override {
        const auto p0 = arCanvas.worldToLocal(m_datas.pos);
        const auto p1 = arCanvas.worldToLocal(ImVec2(m_datas.pos.x + m_datas.size.x, m_datas.pos.y + m_datas.size.y));

        const auto canvas_scale = arCanvas.getDatas().scale;
        auto* pDrawList = ImGui::GetWindowDrawList();

        const auto rnd = 6.0f * canvas_scale;  // rect rounding

        // Shadow
        const auto sh = 3.0f * canvas_scale;
        pDrawList->AddRectFilled(
            ImVec2(p0.x + sh, p0.y + sh),  // rect min
            ImVec2(p1.x + sh, p1.y + sh),  // rect max
            IM_COL32(0, 0, 0, 50),         // rect color
            rnd                            // rect rounding
        );

        // Body
        const ImU32 bg = m_datas.locked ? IM_COL32(80, 80, 100, 255) : m_datas.color;
        pDrawList->AddRectFilled(
            p0,  // rect min
            p1,  // rect max
            bg,  // rect color
            rnd  // rect rounding
        );
        pDrawList->AddRect(
            p0,                            // rect min
            p1,                            // rect max
            IM_COL32(200, 200, 200, 255),  // rect color
            rnd,                           // rect rounding
            0,                             // rect flags
            1.5f * canvas_scale            // rect thickness
        );

        // Slots
        for (int s = 0; s < (int)m_datas.slots_y.size(); ++s) {
            const float wy = m_datas.pos.y + m_datas.slots_y[s];
            pDrawList->AddRectFilled(
                arCanvas.worldToLocal(ImVec2(m_datas.pos.x, wy - 3)),                     // rect min
                arCanvas.worldToLocal(ImVec2(m_datas.pos.x + m_datas.size.x, wy + 3)),  // rect max
                IM_COL32(200, 200, 100, 60)                                              // rect color
            );
        }

        // Text
        const auto tsz = ImGui::CalcTextSize(m_datas.name.c_str());
        pDrawList->AddText(
            nullptr,                                                                           // font
            13.0f * canvas_scale,                                                              // font size
            {p0.x + (p1.x - p0.x - tsz.x * canvas_scale) * 0.5f, p0.y + 6.0f * canvas_scale},  // text pos
            IM_COL32(255, 255, 255, 255),                                                      // text color
            m_datas.name.c_str()                                                               // text
        );
    }
};
