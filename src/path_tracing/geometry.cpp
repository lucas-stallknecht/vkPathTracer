#include "geometry.h"
#include <iostream>

namespace path_tracing
{
    void Geometry::buildBVH(uint32_t depth)
    {
        this->depth = depth;
        // Set the root node
        BVHNode root{};
        root.index = 0;
        root.triangleCount = triangles.size();
        nodes.push_back(root);

        updateNodeBounds(0);
        splitNode(0, 0);
    }

    void Geometry::updateNodeBounds(uint32_t nodeIndex)
    {
        BVHNode& node = nodes[nodeIndex];
        node.aabbMin = glm::vec3(1e30f);
        node.aabbMax = glm::vec3(-1e30f);
        // checks all the triangles contained in a bounding box and adjusts the min and max pos
        // since we want to update a leaf bounds, the index corresponds to "first triangle index"
        for (uint32_t first = node.index, i = 0; i < node.triangleCount; i++)
        {
            Triangle& leafTri = triangles[first + i];
            node.aabbMin = glm::min(node.aabbMin, vertices[leafTri.v0].position);
            node.aabbMin = glm::min(node.aabbMin, vertices[leafTri.v1].position);
            node.aabbMin = glm::min(node.aabbMin, vertices[leafTri.v2].position);
            node.aabbMax = glm::max(node.aabbMax, vertices[leafTri.v0].position);
            node.aabbMax = glm::max(node.aabbMax, vertices[leafTri.v1].position);
            node.aabbMax = glm::max(node.aabbMax, vertices[leafTri.v2].position);
        }
    }

    float Geometry::giveSplitPosAlongAxis(int axis, const BVHNode& node)
    {
        float sum = 0;
        // since we want to split a leaf, the index corresponds to "first triangle index"
        for (uint32_t i = node.index; i < node.index + node.triangleCount; i++)
        {
            sum += computeCentroid(triangles[i])[axis];
        }
        return sum / static_cast<float>(node.triangleCount);
    }

    void Geometry::splitNode(uint32_t nodeIndex, int currentDepth)
    {
        BVHNode& node = nodes[nodeIndex];

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
            glm::vec3 centroid = computeCentroid(triangles[i]);
            if (centroid[axis] < splitPos)
                i++;
            else
                std::swap(triangles[i], triangles[j--]);
        }
        unsigned int leftCount = i - node.index;
        unsigned int rightCount = node.triangleCount - leftCount;

        if (currentDepth < depth && leftCount > 1 && rightCount > 1)
        {
            auto firstChildIdx = static_cast<uint32_t>(nodes.size());

            BVHNode leftChild{};
            BVHNode rightChild{};
            leftChild.index = node.index; // now the children are leafs, so we pass "first triangle index" to them
            leftChild.triangleCount = leftCount;
            rightChild.index = i;
            rightChild.triangleCount = rightCount;
            nodes.push_back(leftChild);
            nodes.push_back(rightChild);

            // the to-split node is not a leaf anymore
            // we avoid dangling pointer by using vector access instead
            nodes[nodeIndex].index = firstChildIdx;
            nodes[nodeIndex].triangleCount = 0;

            // recalculate the bounding box for each child
            updateNodeBounds(firstChildIdx);
            updateNodeBounds(firstChildIdx + 1);

            // build the tree recursively
            splitNode(firstChildIdx, currentDepth + 1);
            splitNode(firstChildIdx + 1, currentDepth + 1);
        }
    }

    glm::vec3 Geometry::computeCentroid(const Triangle& tri)
    {
        return (vertices[tri.v0].position + vertices[tri.v1].position + vertices[tri.v2].position) / 3.0f;
    }

    glm::vec3 Geometry::computeTangent(const std::array<core::Vertex, 3>& verts)
    {
        glm::vec3 edge1 = verts[0].position - verts[1].position;
        glm::vec3 edge2 = verts[0].position - verts[2].position;
        float deltaU1 = verts[1].uv1 - verts[0].uv1;
        float deltaV1 = verts[1].uv2 - verts[0].uv2;
        float deltaU2 = verts[2].uv1 - verts[0].uv1;
        float deltaV2 = verts[2].uv2 - verts[0].uv2;
        float denom = (deltaU1 * deltaV2 - deltaU2 * deltaV1);
        if (glm::abs(denom) < 1e-6f) {
            return glm::vec3(0.0f);
        }
        return glm::normalize((deltaV2 * edge1 - deltaV1 * edge2) / denom);
    }

    void Geometry::traverseBVH(uint32_t index)
    {
        assert(index < nodes.size());

        BVHNode& node = nodes[index];
        bool isLeaf = node.triangleCount != 0;

        if (!isLeaf)
        {
            std::cout << "Node number " << index << " : Left child " << node.index << " | Right child : " << node.index
                + 1 << std::endl;
            traverseBVH(node.index);
            traverseBVH(node.index + 1);
        }
        else
        {
            std::cout << "Node number " << index << " : First triangle index : " << node.index << " | Triangle count "
                << node.triangleCount << std::endl;
        }
    }
} // path_tracing
