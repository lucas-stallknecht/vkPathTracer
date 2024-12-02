#include "obj_loader.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include <iostream>


namespace core_utils
{
    void loadTraceableGeometryFromObj(const std::string& objPath,
        std::vector<core::Vertex>& vertices, std::vector<path_tracing::Triangle>& triangles)
    {
        tinyobj::ObjReaderConfig reader_config;
        reader_config.mtl_search_path = "../resources/objects";
        tinyobj::ObjReader reader;

        if (!reader.ParseFromFile(objPath, reader_config))
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

        for (const auto& shape : shapes)
        {
            size_t indexOffset = 0;
            for (size_t tI = 0; tI < shape.mesh.num_face_vertices.size(); tI++)
            {
                path_tracing::Triangle triangle{};
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
                        vertex.uv2 = attrib.texcoords[2 * index.texcoord_index + 1];
                    }
                    vertices.push_back(vertex);

                    if(vI == 0)
                        triangle.v0 = indexOffset;
                    else if (vI == 1)
                        triangle.v1 = indexOffset+1;
                    else
                        triangle.v2 = indexOffset+2;
                }
                indexOffset += 3;
                triangles.push_back(triangle);
            }
        }
    }
} // core_utils