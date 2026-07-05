#pragma once

#include "bvh8.h"

#include <span>

namespace cs2fow
{

bool build_bvh8(std::span<const triangle> triangles, bvh8_data &result, std::string &error);

} // namespace cs2fow

