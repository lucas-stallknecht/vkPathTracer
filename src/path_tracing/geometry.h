#pragma once
#include "types.h"
#include <string>
#include <array>
#include <vector>

#include "geometry.h"

namespace path_tracing
{
    struct Geometry
    {
        std::vector<core::Vertex> vertices{};
        std::vector<Triangle> triangles{};
        std::vector<BVHNode> nodes;
        uint32_t depth;

        void buildBVH(uint32_t depth = 0);
        void traverseBVH(uint32_t index); // used for debugging only
        static glm::vec3 computeTangent(const std::array<core::Vertex, 3>& verts);

    private:
        glm::vec3 computeCentroid(const Triangle& tri);
        void updateNodeBounds(uint32_t nodeIndex);
        float giveSplitPosAlongAxis(int axis, const BVHNode& node);
        void splitNode(uint32_t nodeIndex, int currentDepth);
    };
} // path_tracing
