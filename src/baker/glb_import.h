#pragma once

#include "bvh8.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace cs2fow
{

struct physics_group_report
{
	std::string name;
	std::vector<std::string> tags;
	uint64_t triangles {};
	bool accepted {};
};

struct import_report
{
	uint64_t raw_triangles {};
	uint64_t accepted_triangles {};
	uint64_t rejected_invalid {};
	std::vector<physics_group_report> groups;
};

bool import_physics_glb(const std::filesystem::path &path, std::vector<triangle> &triangles, import_report &report, std::string &error);
bool physics_tags_accepted(const std::vector<std::string> &tags);

} // namespace cs2fow

