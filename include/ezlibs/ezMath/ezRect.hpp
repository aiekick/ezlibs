#pragma once

#ifndef EZ_TOOLS_rect
#define EZ_TOOLS_rect
#endif  // EZ_TOOLS_rect

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

// ezrect is part of the ezLibs project : https://github.com/aiekick/ezLibs.git

namespace ez {
namespace math {
	
template <typename TTYPE>
struct rect {
    vec2<TTYPE> pos;
    vec2<TTYPE> size;
    rect() = default;
    rect(vec2<TTYPE> aPos, vec2<TTYPE> aSize) : pos(aPos), size(aSize) {};
    // typed conversion, riding the vec2 one : an irect from a frect in one
    // call, the per-field casts written nowhere
    template <typename UTYPE>
    rect(const rect<UTYPE>& aOther) : pos(aOther.pos), size(aOther.size) {
    }
    bool contains(const vec2<TTYPE>& aPoint) const {
        return (aPoint.x >= pos.x) && (aPoint.y >= pos.y) &&  //
            (aPoint.x < pos.x + size.x) && (aPoint.y < pos.y + size.y);
    }
    bool intersects(const rect<TTYPE>& aOther) const {
        return (pos.x < aOther.pos.x + aOther.size.x) && (pos.x + size.x > aOther.pos.x) &&  //
            (pos.y < aOther.pos.y + aOther.size.y) && (pos.y + size.y > aOther.pos.y);
    }
    bool empty() const {
        return (size.x <= static_cast<TTYPE>(0)) || (size.y <= static_cast<TTYPE>(0));
    }
    bool contains(const rect<TTYPE>& aOther) const {
        return (aOther.pos.x >= pos.x) && (aOther.pos.y >= pos.y) &&  //
            (aOther.pos.x + aOther.size.x <= pos.x + size.x) && (aOther.pos.y + aOther.size.y <= pos.y + size.y);
    }
    // aabb intersection; empty() result when the rects do not overlap
    rect<TTYPE> intersected(const rect<TTYPE>& aOther) const {
        const auto minX = (pos.x > aOther.pos.x) ? pos.x : aOther.pos.x;
        const auto minY = (pos.y > aOther.pos.y) ? pos.y : aOther.pos.y;
        const auto maxX = (pos.x + size.x < aOther.pos.x + aOther.size.x) ? (pos.x + size.x) : (aOther.pos.x + aOther.size.x);
        const auto maxY = (pos.y + size.y < aOther.pos.y + aOther.size.y) ? (pos.y + size.y) : (aOther.pos.y + aOther.size.y);
        rect<TTYPE> result;
        result.pos = vec2<TTYPE>(minX, minY);
        // clamped to zero so unsigned TTYPE never wraps
        result.size.x = (maxX > minX) ? (maxX - minX) : static_cast<TTYPE>(0);
        result.size.y = (maxY > minY) ? (maxY - minY) : static_cast<TTYPE>(0);
        return result;
    }
    // aabb union; an empty side is ignored so a default rect is a neutral seed
    rect<TTYPE> merged(const rect<TTYPE>& aOther) const {
        if (empty()) {
            return aOther;
        }
        if (aOther.empty()) {
            return *this;
        }
        const auto minX = (pos.x < aOther.pos.x) ? pos.x : aOther.pos.x;
        const auto minY = (pos.y < aOther.pos.y) ? pos.y : aOther.pos.y;
        const auto maxX = (pos.x + size.x > aOther.pos.x + aOther.size.x) ? (pos.x + size.x) : (aOther.pos.x + aOther.size.x);
        const auto maxY = (pos.y + size.y > aOther.pos.y + aOther.size.y) ? (pos.y + size.y) : (aOther.pos.y + aOther.size.y);
        return rect<TTYPE>(vec2<TTYPE>(minX, minY), vec2<TTYPE>(maxX - minX, maxY - minY));
    }
    bool operator==(const rect<TTYPE>& aOther) const {
        return (pos.x == aOther.pos.x) && (pos.y == aOther.pos.y) &&  //
            (size.x == aOther.size.x) && (size.y == aOther.size.y);
    }
    bool operator!=(const rect<TTYPE>& aOther) const {
        return !(*this == aOther);
    }
    rect<TTYPE> reframe(const vec2<TTYPE>& aOffsetTopLeft, const vec2<TTYPE>& aOffsetBottomRight) const {
        return rect<TTYPE>(pos + aOffsetTopLeft, size - aOffsetTopLeft - aOffsetBottomRight);
    }
};

using frect = rect<float>;
using irect = rect<int32_t>;
using urect = rect<uint32_t>;

}  // namespace math
}  // namespace ez
