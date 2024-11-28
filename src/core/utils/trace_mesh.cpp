#include "trace_mesh.h"
#include <iostream>

namespace core_utils
{
    void TraceMeshBuilder::setGeometry(const std::string& objPath)
    {
        loadTraceableGeometryFromObj(objPath, vertices_, triangles_);
    }

    void TraceMeshBuilder::setMaterial(glm::vec3 color, float emissiveStrength, float roughness, float metallic)
    {
        core::TraceMaterial m = {color};
        m.emissiveStrength = emissiveStrength;
        m.metallic = metallic;
        m.roughness = roughness;
        material_ = m;
    }

    core::TraceMesh TraceMeshBuilder::build()
    {
        buildBVH();
        return core::TraceMesh{vertices_, triangles_, material_, nodes_};
    }

    void TraceMeshBuilder::buildBVH()
    {
        // Pre-calculate all the centroids
        for (auto& tri : triangles_)
        {
            tri.centroid = (vertices_[tri.v0].position + vertices_[tri.v1].position + vertices_[tri.v2].position) *
                0.333f;
        }
        // Set de root node
        core::TraceBVHNode root{};
        root.index = 0;
        root.triangleCount = triangles_.size();
        nodes_.push_back(root);

        updateNodeBounds(0);
        splitNode(0, 0);
    }

    void TraceMeshBuilder::updateNodeBounds(uint32_t nodeIndex)
    {
        core::TraceBVHNode& node = nodes_[nodeIndex];
        node.aabbMin = glm::vec3(1e30f);
        node.aabbMax = glm::vec3(-1e30f);
        // checks all the triangles contained in a bounding box and adjusts the min and max pos
        // since we want to update a leaf bounds, the index corresponds to "first triangle index"
        for (uint32_t first = node.index, i = 0; i < node.triangleCount; i++)
        {
            core::TraceTriangle& leafTri = triangles_[first + i];
            node.aabbMin = glm::min(node.aabbMin, vertices_[leafTri.v0].position);
            node.aabbMin = glm::min(node.aabbMin, vertices_[leafTri.v1].position);
            node.aabbMin = glm::min(node.aabbMin, vertices_[leafTri.v2].position);
            node.aabbMax = glm::max(node.aabbMax, vertices_[leafTri.v0].position);
            node.aabbMax = glm::max(node.aabbMax, vertices_[leafTri.v1].position);
            node.aabbMax = glm::max(node.aabbMax, vertices_[leafTri.v2].position);
        }
    }

    float TraceMeshBuilder::giveSplitPosAlongAxis(int axis, const core::TraceBVHNode& node)
    {
        float sum = 0;
        // since we want to split a leaf, the index corresponds to "first triangle index"
        for (uint32_t i = node.index; i < node.index + node.triangleCount; i++)
        {
            sum += triangles_[i].centroid[axis];
        }
        return sum / static_cast<float>(node.triangleCount);
    }

    void TraceMeshBuilder::splitNode(uint32_t nodeIndex, int currentDepth)
    {
        core::TraceBVHNode& node = nodes_[nodeIndex];

        glm::vec3 extent = node.aabbMax - node.aabbMin;
        // select the axis of the split plane
        int axis = 0;
        if (extent.y > extent.x) axis = 1;
        if (extent.z > extent[axis]) axis = 2;

        // average position of the triangles in the bounding box ==> TODO heuristic
        float splitPos = giveSplitPosAlongAxis(axis, node);

        // puts all the triangles with centroid before the splitPos on the left
        // i will help determine the triangles count for the leftChild
        unsigned int i = node.index;
        unsigned int j = i + node.triangleCount - 1;
        while (i <= j)
        {
            if (triangles_[i].centroid[axis] < splitPos)
                i++;
            else
                std::swap(triangles_[i], triangles_[j--]);
        }
        unsigned int leftCount = i - node.index;
        unsigned int rightCount = node.triangleCount - leftCount;

        if (currentDepth < 3 && leftCount > 1 && rightCount > 1)
        {
            auto firstChildIdx = static_cast<uint32_t>(nodes_.size());

            core::TraceBVHNode leftChild{};
            core::TraceBVHNode rightChild{};
            leftChild.index = node.index; // now the children are leafs, so we pass "first triangle index" to them
            leftChild.triangleCount = leftCount;
            rightChild.index = i;
            rightChild.triangleCount = rightCount;
            nodes_.push_back(leftChild);
            nodes_.push_back(rightChild);

            // the to-split node is not a leaf anymore
            // we avoid dangling pointer by using vector access instead
            nodes_[nodeIndex].index = firstChildIdx;
            nodes_[nodeIndex].triangleCount = 0;

            // recalculate the bounding box for each child
            updateNodeBounds(firstChildIdx);
            updateNodeBounds(firstChildIdx + 1);

            // build the tree recursively
            splitNode(firstChildIdx, currentDepth + 1);
            splitNode(firstChildIdx + 1, currentDepth + 1);
        }
    }

    void TraceMeshBuilder::traverseBVH(uint32_t index)
    {
        assert(index < nodes_.size());

        core::TraceBVHNode& node = nodes_[index];
        bool isLeaf = node.triangleCount != 0;

        if (!isLeaf)
        {
            std::cout << "Node number " << index << " : Left child " << node.index << " | Right child : " << node.index+1 << std::endl;
            traverseBVH(node.index);
            traverseBVH(node.index + 1);
        }
        else
        {
            std::cout << "Node number " << index <<  " : First triangle index : " << node.index << " | Triangle count " << node.triangleCount << std::endl;
        }
    }
} // core_utils
