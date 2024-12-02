#pragma once
#include "types.h"
#include <string>

namespace path_tracing {

class TraceMeshBuilder {
public:
    void setGeometry(const std::string& objPath);
    void setMaterial(const MaterialProperties& material);
    Mesh build(uint32_t depth = 0);
    void traverseBVH(uint32_t index); // used for debugging only

private:
    void buildBVH();
    void updateNodeBounds(uint32_t nodeIndex);
    float giveSplitPosAlongAxis(int axis, const BVHNode& node);
    void splitNode(uint32_t nodeIndex, int currentDepth);

    std::vector<core::Vertex> vertices_;
    std::vector<Triangle> triangles_;
    MaterialProperties material_{};
    std::vector<BVHNode> nodes_;
    uint32_t maxDepth_ = 0;
};

    int handleMapProperty(const std::optional<std::string>& property, std::unordered_map<std::string, int>& map, int currentIndex);

} // core_utils
