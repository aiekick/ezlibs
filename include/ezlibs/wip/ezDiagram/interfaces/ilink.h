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

#include <cstdint>
#include <vector>
#include "../../../ezVec2.hpp"
template<typename TColor = uint32_t, typename TVec2 = ez::math::fvec2, typename TID = uintptr_>
class ILink {
public:
    struct Datas {
		TID linkId{};
        TID srcNodeID{};
        TID srcNodeSlotID{};
        TID dstNodeID{};
        TID dstNodeSlotID{};
        TColor color{};
        std::vector<TVec2> corners;
    };
    virtual ~ILink() = default;
    virtual Datas& rDatas() = 0;
    virtual const Datas& getDatas() const = 0;
    // Draw the link via ImNodal between the given editor-owned slot ids.
    virtual void draw(TID aLinkId, TID aFromSlotId, TID aToSlotId) const = 0;
};
