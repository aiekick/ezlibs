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
#include <string>

#include <imguipack.h>

class INode {
public:
    struct Datas {
        ImVec2 pos;
        ImVec2 size;
        ImU32 color{};
        std::vector<float> slots_y;
        bool locked{};
        ImVec2 velocity;
        ImVec2 force;
        std::string name{};
    };
    virtual ~INode() = default;
    virtual Datas& rDatas() = 0;
    virtual const Datas& getDatas() const = 0;
    // Emit the node content (title + slots) between the editor's ImNodal BeginNode/EndNode.
    // aSlotIds are the editor-owned ImNodal ids for this node's slots (size == slots_y.size()).
    virtual void drawImNodalContent(const std::vector<ImNodal::Id>& aSlotIds) = 0;
};
