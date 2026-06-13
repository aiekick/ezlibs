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

class ICanvas {
public:
    struct Datas {
        ImVec2 origin{0, 0};
        ImVec2 offset{0, 0};
        float scale{1.0f};
        ImVec2 minMaxScale{0.2f, 6.0f};
    };
    virtual Datas& rDatas() = 0;
    virtual const Datas& getDatas() const = 0;
    virtual ImVec2 worldToLocal(const ImVec2& aPos) const = 0;
    virtual ImVec2 localToWorld(const ImVec2& aPos) const = 0;
    virtual void init(const ImVec2& aSize) = 0;
    virtual bool begin(const char* aTitle, ImGuiWindowFlags aFlags) = 0;
    virtual void end() = 0;
};
