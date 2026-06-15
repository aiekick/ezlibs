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

#include <memory>
#include <vector>
#include <string>
#include <cstdint>

#include <ezlibs/ezCnt.hpp>

#include "inode.h"
#include "ilink.h"
#include "../../../ezMath/ezMath.hpp"
template<typename TColor = uint32_t, typename TVec2 = ez::math::fvec2, typename TID = uintptr_t>
class ISolver {
public:
    struct Datas {
        struct System {
            float energy{1.0f};               // total energy of the system (convergence)
            float damping{0.99f};             // velocity damping (0-1, lower = brakes more)
            float nodeGap{40.0f};             // minimal distance between node edges
            float maxForce{50.0f};            // max applicable force (avoids explosion)
            float nodeRepulsion{5000.0f};     // node to node repulsion intensity
            float linkAttraction{0.005f};     // link attraction intensity (spring)
            float gravity{0.002f};            // force toward the centroid (avoids spreading)
            float slotGapMultiplier{1.0f};    // extra gap per slot (proportional to connection count)
            float nodeToLinkRepulsion{5.0f};  // node to link repulsion coef
            TVec2 anchorPoint{};             // graph anchor point
            float anchorStrength{1.0f};       // pull strength toward the anchor point
            bool sideSlots{};                 // slots on node sides (else centered)
            float snapGridSpacing{30.0f};     // grid spacing
            float snapGridStrength{0.1f};     // attraction strength toward grid lines (low = soft)
            bool enableRepulseNodes{true};
            bool enableRepulseNodesFromLinks{true};
            bool enableAttractLinks{true};
            bool enableSnapToGrid{true};
            bool enableCentroidGravity{true};
        } system;
        struct Containers {
            ez::cnt::DicoVector<std::string, std::shared_ptr<INode<TColor, TVec2, TID>>> nodes;
            std::vector<std::shared_ptr<ILink<TColor, TVec2, TID>>> links;
        } containers;
        struct Computed {
            TVec2 centroid;
        } computed;
    };
    virtual ~ISolver() = default;
    virtual Datas& rDatas() = 0;
    virtual const Datas& getDatas() const = 0;

    virtual int32_t addNode(const std::shared_ptr<INode<TColor, TVec2, TID>>& aNode) = 0;
    virtual int32_t addLink(const std::shared_ptr<ILink<TColor, TVec2, TID>>& aLink) = 0;

    virtual void init() = 0;   // will init the system
    virtual void updateLinks() = 0;  // update links pos
    virtual float step(float aDt) = 0;  // will run a step and return the energy of the system
};
