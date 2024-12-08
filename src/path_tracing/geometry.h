#pragma once
#include "types.h"
#include <string>
#include <vector>

#include "geometry.h"

namespace path_tracing
{
    struct Geometry
    {
        std::vector<core::Vertex> vertices_{};
        std::vector<Triangle> triangles_{};
        std::vector<BVHNode> nodes_;
        uint32_t depth_;

        void buildBVH(uint32_t depth = 0);
        void traverseBVH(uint32_t index); // used for debugging only

    private:
        void updateNodeBounds(uint32_t nodeIndex);
        float giveSplitPosAlongAxis(int axis, const BVHNode& node);
        void splitNode(uint32_t nodeIndex, int currentDepth);
    };
} // path_tracing
