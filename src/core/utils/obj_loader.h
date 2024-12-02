#pragma once
#include "types.h"
#include <string>

namespace core_utils {
    void loadTraceableGeometryFromObj(const std::string& objPath,
        std::vector<core::Vertex>& vertices, std::vector<path_tracing::Triangle>& triangles);
} // core_utils
