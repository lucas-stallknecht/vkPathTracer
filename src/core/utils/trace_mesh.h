#pragma once
#include "types.h"
#include <string>

#include "obj_loader.h"

namespace core_utils {

class TraceMeshBuilder {
public:
    void setGeometry(const std::string& objPath);
    void setMaterial(glm::vec3 color, float emissiveStrength, float roughness, float metallic);
    core::TraceMesh build();
    void traverseBVH(uint32_t index); // used for debugging only

private:
    void buildBVH();
    void updateNodeBounds(uint32_t nodeIndex);
    float giveSplitPosAlongAxis(int axis, const core::TraceBVHNode& node);
    void splitNode(uint32_t nodeIndex, int currentDepth);

    std::vector<core::Vertex> vertices_;
    std::vector<core::TraceTriangle> triangles_;
    core::TraceMaterial material_{};
    std::vector<core::TraceBVHNode> nodes_;
};

} // core_utils
