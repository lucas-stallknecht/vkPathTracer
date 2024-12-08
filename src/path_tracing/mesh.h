#pragma once
#include "geometry.h"
#include <string>
#include <filesystem>

namespace path_tracing
{
    struct Mesh
    {
        Geometry geometry;
        Material material;
        std::vector<Texture> textures;
    };

    Mesh loadFromObj(const std::filesystem::path& objPath);
} // path_tracing
