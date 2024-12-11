#include "mesh.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <iostream>

namespace std
{
    template <>
    struct hash<core::Vertex>
    {
        size_t operator()(core::Vertex const& vertex) const
        {
            return ((hash<glm::vec3>()(vertex.position) ^
                    (hash<glm::vec3>()(vertex.normal) << 1)) >> 1) ^
                (hash<float>()(vertex.uv1) << 1);
        }
    };
}

namespace path_tracing
{
    std::vector<Mesh> loadFromObj(const std::filesystem::path& objPath)
    {
        tinyobj::ObjReaderConfig reader_config;
        reader_config.mtl_search_path = objPath.root_directory().string();
        tinyobj::ObjReader reader;

        if (!reader.ParseFromFile(objPath.string(), reader_config))
        {
            if (!reader.Error().empty())
            {
                std::cerr << "TinyObjReader: " << reader.Error();
            }
            exit(1);
        }
        if (!reader.Warning().empty())
        {
            std::cout << "TinyObjReader: " << reader.Warning();
        }

        auto& attrib = reader.GetAttrib();
        auto& shapes = reader.GetShapes();
        auto& mats = reader.GetMaterials();
        std::unordered_map<core::Vertex, uint32_t> uniqueVertices{};
        std::vector<Mesh> scene{};

        // Build the geometry
        for (const auto& shape : shapes)
        {
            Mesh outputMesh{};

            size_t indexOffset = 0;
            for (size_t tI = 0; tI < shape.mesh.num_face_vertices.size(); tI++)
            {
                // We create a triangle and check for vertex duplicates
                Triangle triangle{};
                std::array<core::Vertex, 3> vertForTangents;

                for (size_t vI = 0; vI < 3; vI++)
                {
                    tinyobj::index_t index = shape.mesh.indices[indexOffset + vI];
                    core::Vertex vertex{};

                    vertex.position = {
                        attrib.vertices[3 * index.vertex_index + 0],
                        attrib.vertices[3 * index.vertex_index + 1],
                        attrib.vertices[3 * index.vertex_index + 2]
                    };

                    // Check if `normal_index` is zero or positive. negative = no normal data
                    if (index.normal_index >= 0)
                    {
                        vertex.normal = {
                            attrib.normals[3 * index.normal_index + 0],
                            attrib.normals[3 * index.normal_index + 1],
                            attrib.normals[3 * index.normal_index + 2]
                        };
                    }

                    // Check if `texcoord_index` is zero or positive. negative = no texcoord data
                    if (index.texcoord_index >= 0)
                    {
                        vertex.uv1 = attrib.texcoords[2 * index.texcoord_index + 0];
                        vertex.uv2 = 1.0 - attrib.texcoords[2 * index.texcoord_index + 1];
                    }

                    if (!uniqueVertices.contains(vertex))
                    {
                        uniqueVertices[vertex] = static_cast<uint32_t>(outputMesh.geometry.vertices.size());
                        outputMesh.geometry.vertices.push_back(vertex);
                    }

                    if (vI == 0)
                        triangle.v0 = uniqueVertices[vertex];
                    else if (vI == 1)
                        triangle.v1 = uniqueVertices[vertex];
                    else
                        triangle.v2 = uniqueVertices[vertex];

                    vertForTangents[vI] = vertex;
                }
                triangle.tangent = Geometry::computeTangent(vertForTangents);
                indexOffset += 3;
                outputMesh.geometry.triangles.push_back(triangle);
            }

            // Since we group obj shapes by material, a shape is associated with maximum one material
            if(shape.mesh.material_ids[0] != -1)
            {
                const tinyobj::material_t& mat = mats[shape.mesh.material_ids[0]];
                outputMesh.material.color = glm::vec3(mat.diffuse[0], mat.diffuse[1], mat.diffuse[2]);
                if (mat.diffuse_texname != "")
                    outputMesh.material.colorMap =  objPath.parent_path().string() + "/" + mat.diffuse_texname;
                outputMesh.material.emissiveStrength = mat.emission[0];
                outputMesh.material.roughness = mat.roughness;
                if (mat.roughness_texname != "")
                    outputMesh.material.roughnessMap =  objPath.parent_path().string() + "/" + mat.roughness_texname;
                outputMesh.material.metallic = mat.metallic;
                if (mat.metallic_texname != "")
                    outputMesh.material.metallicMap =  objPath.parent_path().string() + "/" + mat.metallic_texname;
                if (mat.bump_texname != "")
                    outputMesh.material.normalMap =  objPath.parent_path().string() + "/" + mat.bump_texname;
            }

            std::cout << shape.name << " | vertices : " << outputMesh.geometry.vertices.size()
            << " | triangles : " << outputMesh.geometry.vertices.size() << std::endl;

            const uint32_t defaultBVHDepth = std::ceil(std::log2(outputMesh.geometry.triangles.size() /4));
            outputMesh.geometry.buildBVH(defaultBVHDepth);
            scene.push_back(outputMesh);
        }

        return scene;
    }
}
