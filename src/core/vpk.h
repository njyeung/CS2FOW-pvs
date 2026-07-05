#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace cs2fow
{

struct vpk_entry
{
	uint32_t crc32 {};
	uint64_t size {};
	uint16_t archive_index {};
	uint32_t archive_offset {};
};

bool find_vpk_entry(const std::filesystem::path &vpk_path, const std::string &entry_path, vpk_entry &entry, std::string &error);

} // namespace cs2fow

