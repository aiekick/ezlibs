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

#include <imguipack.h>

#include "interfaces/icanvas.h"

class Canvas : public ICanvas {
    Datas m_datas;

public:
    void init(const ImVec2& aSize) override {
        m_datas.offset.x = aSize.x * 0.5f;
        m_datas.offset.y = aSize.y * 0.5f;
    }
    Datas& rDatas() override {
        return m_datas;
    }
    const Datas& getDatas() const override {
        return m_datas;
    }
    ImVec2 worldToLocal(const ImVec2& aPos) const override {
        return ImVec2(
            m_datas.origin.x + aPos.x * m_datas.scale + m_datas.offset.x,  //
            m_datas.origin.y + aPos.y * m_datas.scale + m_datas.offset.y   //
        );
    }
    ImVec2 localToWorld(const ImVec2& aPos) const override {
        return ImVec2(
            (aPos.x - m_datas.origin.x - m_datas.offset.x) / m_datas.scale,  //
            (aPos.y - m_datas.origin.y - m_datas.offset.y) / m_datas.scale);
    }
    bool begin(const char* aTitle, ImGuiWindowFlags aFlags) override {
        if (ImGui::BeginChild(ImGui::GetID(aTitle),ImVec2(0,0),0, aFlags)) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            m_datas.origin = ImGui::GetCursorScreenPos();
            ImVec2 avail_size = ImGui::GetContentRegionAvail();
            ImVec2 canvas_end(m_datas.origin.x + avail_size.x, m_datas.origin.y + avail_size.y);

            dl->AddRectFilled(m_datas.origin, canvas_end, IM_COL32(40, 42, 48, 255));
            dl->PushClipRect(m_datas.origin, canvas_end, true);

            ImGuiIO& io = ImGui::GetIO();

            // Grid
            float gs = 20.0f * m_datas.scale;
            for (float x = fmodf(m_datas.offset.x, gs); x < avail_size.x; x += gs) {
                dl->AddLine(ImVec2(m_datas.origin.x + x, m_datas.origin.y), ImVec2(m_datas.origin.x + x, canvas_end.y), IM_COL32(75, 77, 83, 255));
            }
            for (float y = fmodf(m_datas.offset.y, gs); y < avail_size.y; y += gs) {
                dl->AddLine(ImVec2(m_datas.origin.x, m_datas.origin.y + y), ImVec2(canvas_end.x, m_datas.origin.y + y), IM_COL32(75, 77, 83, 255));
            }

            // Draw canvas origin
            auto* pDrawList = ImGui::GetWindowDrawList();
            pDrawList->AddCircleFilled(worldToLocal(ImVec2()), 5.0f * m_datas.scale, IM_COL32(255, 255, 0, 200), 6);

            // Interactions
            if (ImGui::IsWindowHovered()) {
                // Pan
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
                    m_datas.offset.x += io.MouseDelta.x;
                    m_datas.offset.y += io.MouseDelta.y;
                }

                // Central Zoom
                if (io.MouseWheel != 0.0f) {
                    ImVec2 mcv(io.MousePos.x - m_datas.origin.x, io.MousePos.y - m_datas.origin.y);
                    float oz = m_datas.scale;
                    m_datas.scale = ImClamp(m_datas.scale + io.MouseWheel * 0.1f, m_datas.minMaxScale.x, m_datas.minMaxScale.y);
                    float r = m_datas.scale / oz;
                    m_datas.offset.x = mcv.x - (mcv.x - m_datas.offset.x) * r;
                    m_datas.offset.y = mcv.y - (mcv.y - m_datas.offset.y) * r;
                }
            }

            dl->PopClipRect();
            return true;
        }
        return false;   
    }
    void end() override {
        ImGui::EndChild(); 
    }
};
