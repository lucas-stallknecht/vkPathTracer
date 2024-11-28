#pragma once
#include "types.h"
#include <string>

namespace core_utils {
    void loadTraceableGeometryFromObj(const std::string& objPath,
        std::vector<core::Vertex>& vertices, std::vector<core::TraceTriangle>& triangles);
} // core_utils
