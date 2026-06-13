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

#include <ezlibs/ezCnt.hpp>

struct Node ;
struct Link ;

class ISolver {
public:
    struct Datas {
        struct System {
            float energy{1.0f};               // Énergie totale du système (pour convergence)
            float damping{0.99f};             // Amortissement de la vélocité (0-1, plus bas = freine plus)
            float nodeGap{40.0f};             // Distance minimale entre les bords des rectangles
            float maxForce{50.0f};            // Force maximale applicable (évite l'explosion)
            float nodeRepulsion{5000.0f};     // Intensité de la répulsion entre nodes
            float linkAttraction{0.005f};     // Intensité de l'attraction des liens (ressort)
            float gravity{0.002f};            // Force vers le centroïde (évite l'éparpillement)
            float slotGapMultiplier{1.0f};    // Gap supplémentaire par slot (espacement proportionnel aux connexions)
            float nodeToLinkRepulsion{5.0f};  // coef de repulstion entre node et link
            ImVec2 anchorPoint{};             // Point d'ancrage du graph
            float anchorStrength{1.0f};       // Force de rappel vers le point d'ancrage
            bool sideSlots{};                 // Slots sur les côtés des nodes (sinon centrés)
            float snapGridSpacing{30.0f};     // Espacement de la grille
            float snapGridStrength{0.1f};     // Force d'attraction vers les méridiens (faible = doux)
            bool enableRepulseNodes{true};
            bool enableRepulseNodesFromLinks{true};
            bool enableAttractLinks{true};
            bool enableSnapToGrid{true};
            bool enableCentroidGravity{true};
        } system;
        struct Containers {
            ez::cnt::DicoVector<std::string, Node> nodes;
            std::vector<Link> links;
        } containers;
        struct Computed {
            ImVec2 centroid;
        } computed;
    };
    virtual Datas& rDatas() = 0;
    virtual const Datas& getDatas() const = 0;

    virtual int32_t addNode(const Node& aNode) = 0;
    virtual int32_t addLink(const Link& aLink) = 0;

    virtual void init() = 0;   // will init the system
    virtual void updateLinks() = 0;  // update links pos
    virtual float step(float aDt) = 0;  // will run a step and return the energy of the system
};