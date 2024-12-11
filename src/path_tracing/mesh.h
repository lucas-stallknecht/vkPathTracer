#pragma once
#include "geometry.h"
#include <string>
#include <filesystem>

namespace path_tracing
{
    struct Mesh
    {
        Geometry geometry;
        Material material{};
        // std::vector<Texture> textures;
    };

    glm::vec3 calculateTangent(const std::array<core::Vertex, 3>& vertices);
    std::vector<Mesh> loadFromObj(const std::filesystem::path& objPath);
} // path_tracing
