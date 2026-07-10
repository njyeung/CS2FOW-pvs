#pragma once

// Declares the offline triangle-to-BVH8 builder. It receives accepted map
// triangles and either returns one complete tree or a plain error.

#include "bvh8.h"

#include <span>

namespace cs2fow
{

bool build_bvh8(std::span<const triangle> triangles, bvh8_data &result, std::string &error);

} // namespace cs2fow
