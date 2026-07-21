// Locates the block table inside a compiled Source 2 resource and reads the
// scalar and block constants out of VRF's --decompile KV3 text.

#include "vvis_import.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <utility>

namespace cs2fow
{
namespace
{

constexpr uint16_t k_header_version = 12;
constexpr uint16_t k_resource_version = 7;
constexpr uint64_t k_block_row_bytes = 12;

bool checked_add(uint64_t left, uint64_t right, uint64_t &result)
{
	if (right > std::numeric_limits<uint64_t>::max() - left)
	{
		return false;
	}
	result = left + right;
	return true;
}

bool checked_multiply(uint64_t left, uint64_t right, uint64_t &result)
{
	if (left != 0 && right > std::numeric_limits<uint64_t>::max() / left)
	{
		return false;
	}
	result = left * right;
	return true;
}

uint16_t read_u16(std::span<const std::byte> bytes, uint64_t offset)
{
	return static_cast<uint16_t>(std::to_integer<uint16_t>(bytes[offset])
		| static_cast<uint16_t>(std::to_integer<uint16_t>(bytes[offset + 1]) << 8));
}

uint32_t read_u32(std::span<const std::byte> bytes, uint64_t offset)
{
	return std::to_integer<uint32_t>(bytes[offset])
		| (std::to_integer<uint32_t>(bytes[offset + 1]) << 8)
		| (std::to_integer<uint32_t>(bytes[offset + 2]) << 16)
		| (std::to_integer<uint32_t>(bytes[offset + 3]) << 24);
}

std::string_view trim(std::string_view value)
{
	constexpr std::string_view whitespace = " \t\r\n";
	const size_t begin = value.find_first_not_of(whitespace);
	if (begin == std::string_view::npos)
	{
		return {};
	}
	const size_t end = value.find_last_not_of(whitespace);
	return value.substr(begin, end - begin + 1);
}

bool parse_u32(std::string_view token, uint32_t &value)
{
	const char *first = token.data();
	const char *last = token.data() + token.size();
	const std::from_chars_result result = std::from_chars(first, last, value);
	return result.ec == std::errc() && result.ptr == last && !token.empty();
}

bool parse_float(std::string_view token, float &value)
{
	const char *first = token.data();
	const char *last = token.data() + token.size();
	const std::from_chars_result result = std::from_chars(first, last, value);
	return result.ec == std::errc() && result.ptr == last && !token.empty();
}

bool parse_vec3_value(std::string_view value, float out[3])
{
	std::string_view body = trim(value);
	if (body.size() < 2 || body.front() != '[' || body.back() != ']')
	{
		return false;
	}
	body = body.substr(1, body.size() - 2u);
	int count = 0;
	size_t pos = 0;
	for (;;)
	{
		const size_t comma = body.find(',', pos);
		const std::string_view token = body.substr(pos, (comma == std::string_view::npos ? body.size() : comma) - pos);
		if (count >= 3 || !parse_float(trim(token), out[count]))
		{
			return false;
		}
		++count;
		if (comma == std::string_view::npos)
		{
			break;
		}
		pos = comma + 1u;
	}
	return count == 3;
}

std::string_view next_line(std::string_view &text)
{
	while (!text.empty())
	{
		const size_t newline = text.find('\n');
		const size_t length = newline == std::string_view::npos ? text.size() : newline;
		const std::string_view line = trim(text.substr(0, length));
		text.remove_prefix(newline == std::string_view::npos ? text.size() : newline + 1u);
		if (!line.empty())
		{
			return line;
		}
	}
	return {};
}

bool split_assignment(std::string_view line, std::string_view &key, std::string_view &value)
{
	const size_t equals = line.find('=');
	if (equals == std::string_view::npos)
	{
		return false;
	}
	key = trim(line.substr(0, equals));
	value = trim(line.substr(equals + 1u));
	return !key.empty();
}

bool find_assignment(std::string_view text, std::string_view wanted, std::string_view &value)
{
	while (!text.empty())
	{
		const std::string_view line = next_line(text);
		std::string_view key;
		if (split_assignment(line, key, value) && key == wanted)
		{
			return true;
		}
	}
	return false;
}

bool parse_field(std::string_view text, std::string_view key, uint32_t &value, std::string &error)
{
	std::string_view token;
	if (find_assignment(text, key, token) && parse_u32(token, value))
	{
		return true;
	}
	error = "missing or invalid vvis field: ";
	error.append(key);
	return false;
}

bool parse_field(std::string_view text, std::string_view key, float &value, std::string &error)
{
	std::string_view token;
	if (find_assignment(text, key, token) && parse_float(token, value))
	{
		return true;
	}
	error = "missing or invalid vvis field: ";
	error.append(key);
	return false;
}

bool parse_vec3(std::string_view text, std::string_view key, float value[3], std::string &error)
{
	std::string_view token;
	if (find_assignment(text, key, token) && parse_vec3_value(token, value))
	{
		return true;
	}
	error = "missing or invalid vvis field: ";
	error.append(key);
	return false;
}

bool parse_block_body(std::string_view &text, std::string_view block_key, uint32_t &offset, uint32_t &count, std::string &error)
{
	if (next_line(text) != "{")
	{
		error = "expected '{' after vvis field: ";
		error.append(block_key);
		return false;
	}
	bool have_offset = false;
	bool have_count = false;
	for (;;)
	{
		const std::string_view line = next_line(text);
		if (line.empty())
		{
			error = "unterminated vvis block: ";
			error.append(block_key);
			return false;
		}
		if (line == "}")
		{
			break;
		}
		std::string_view key;
		std::string_view value;
		if (!split_assignment(line, key, value))
		{
			error = "line is not a field assignment in vvis block: ";
			error.append(block_key);
			return false;
		}
		uint32_t *target = nullptr;
		bool *seen = nullptr;
		if (key == "m_nOffset")
		{
			target = &offset;
			seen = &have_offset;
		}
		else if (key == "m_nElementCount")
		{
			target = &count;
			seen = &have_count;
		}
		else
		{
			continue;
		}
		if (*seen)
		{
			error = "duplicate field in vvis block: ";
			error.append(block_key);
			return false;
		}
		if (!parse_u32(value, *target))
		{
			error = "invalid value in vvis block: ";
			error.append(block_key);
			return false;
		}
		*seen = true;
	}
	if (!have_offset || !have_count)
	{
		error = "vvis block is missing m_nOffset or m_nElementCount: ";
		error.append(block_key);
		return false;
	}
	return true;
}

bool parse_block(std::string_view text, std::string_view key, uint32_t &offset, uint32_t &count, std::string &error)
{
	while (!text.empty())
	{
		const std::string_view line = next_line(text);
		std::string_view found_key;
		std::string_view value;
		if (!split_assignment(line, found_key, value) || found_key != key)
		{
			continue;
		}
		if (!value.empty())
		{
			error = "expected a block body for vvis field: ";
			error.append(key);
			return false;
		}
		return parse_block_body(text, key, offset, count, error);
	}
	error = "missing required vvis field: ";
	error.append(key);
	return false;
}

bool node_is_leaf(uint64_t node)
{
	return (node & 1u) != 0u;
}

uint32_t node_offset(uint64_t node)
{
	return static_cast<uint32_t>((node >> 1) & 0x7fffffffu);
}

uint32_t node_region_count(uint64_t node)
{
	return static_cast<uint32_t>((node >> 32) & 0xffu);
}

uint32_t region_mask_index(uint64_t region)
{
	return static_cast<uint32_t>(region >> 40);
}

} // namespace

bool parse_vxvs_block(std::span<const std::byte> bytes, resource_block &out, std::string &error)
{
	error.clear();

	constexpr size_t header_size = sizeof(uint32_t) + sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint32_t);
	if (bytes.size() < header_size)
	{
		error = "resource is smaller than its header";
		return false;
	}

	const uint32_t file_size = read_u32(bytes, 0);
	const uint16_t header_version = read_u16(bytes, 4);
	const uint16_t resource_version = read_u16(bytes, 6);
	const uint64_t block_offset = read_u32(bytes, 8);
	const uint32_t block_count = read_u32(bytes, 12);
	if (header_version != k_header_version)
	{
		error = "unsupported header version";
		return false;
	}
	if (resource_version != k_resource_version)
	{
		error = "unsupported resource version";
		return false;
	}
	if (block_count != 3)
	{
		error = "unsupported block count, did you pass in a vvis file?";
		return false;
	}
	if (file_size != bytes.size())
	{
		error = "resource file size does not match its header";
		return false;
	}

	uint64_t table_start = 0;
	uint64_t vxvs_row = 0;
	uint64_t table_end = 0;
	if (!checked_add(8u, block_offset, table_start)
		|| !checked_add(table_start, 2u * k_block_row_bytes, vxvs_row)
		|| !checked_add(table_start, 3u * k_block_row_bytes, table_end)
		|| table_end > bytes.size())
	{
		error = "resource block table is out of range";
		return false;
	}
	if (std::memcmp(bytes.data() + vxvs_row, "VXVS", sizeof(out.fourcc)) != 0)
	{
		error = "third resource block is not VXVS";
		return false;
	}

	uint64_t offset_field = vxvs_row + 4u;
	uint64_t block_start = 0;
	uint64_t block_end = 0;
	if (!checked_add(offset_field, read_u32(bytes, offset_field), block_start)
		|| !checked_add(block_start, read_u32(bytes, vxvs_row + 8u), block_end)
		|| block_end > bytes.size())
	{
		error = "vxvs block extends past the file";
		return false;
	}

	std::memcpy(out.fourcc, bytes.data() + vxvs_row, sizeof(out.fourcc));
	out.offset = static_cast<uint32_t>(block_start);
	out.size = static_cast<uint32_t>(block_end - block_start);
	return true;
}

bool parse_vvis_text(std::string_view text, vvis_constants &out, std::string &error)
{
	error.clear();
	out = {};

	bool result = true;
	result &= parse_field(text, "m_nBaseClusterCount", out.m_nBaseClusterCount, error);
	result &= parse_field(text, "m_nPVSBytesPerCluster", out.m_nPVSBytesPerCluster, error);

	result &= parse_vec3(text, "m_vMinBounds", out.m_vMinBounds, error);
	result &= parse_vec3(text, "m_vMaxBounds", out.m_vMaxBounds, error);

	result &= parse_field(text, "m_flGridSize", out.m_flGridSize, error);
	result &= parse_field(text, "m_nSkyVisibilityCluster", out.m_nSkyVisibilityCluster, error);
	result &= parse_field(text, "m_nSunVisibilityCluster", out.m_nSunVisibilityCluster, error);

	result &= parse_block(text, "m_NodeBlock", out.m_NodeBlock_m_nOffset, out.m_NodeBlock_m_nElementCount, error);
	result &= parse_block(text, "m_RegionBlock", out.m_RegionBlock_m_nOffset, out.m_RegionBlock_m_nElementCount, error);
	result &= parse_block(text, "m_EnclosedClusterListBlock", out.m_EnclosedClusterListBlock_m_nOffset, out.m_EnclosedClusterListBlock_m_nElementCount, error);
	result &= parse_block(text, "m_EnclosedClustersBlock", out.m_EnclosedClustersBlock_m_nOffset, out.m_EnclosedClustersBlock_m_nElementCount, error);
	result &= parse_block(text, "m_MasksBlock", out.m_MasksBlock_m_nOffset, out.m_MasksBlock_m_nElementCount, error);
	result &= parse_block(text, "m_nVisBlocks", out.m_nVisBlocks_m_nOffset, out.m_nVisBlocks_m_nElementCount, error);

	return result;
}

bool import_vvis(std::span<const std::byte> vvis_c_bytes, std::string_view kv3_text, pvs_data &out, std::string &error)
{
	error.clear();
	out = {};

	resource_block vxvs {};
	if (!parse_vxvs_block(vvis_c_bytes, vxvs, error))
	{
		return false;
	}
	vvis_constants constants {};
	if (!parse_vvis_text(kv3_text, constants, error))
	{
		return false;
	}

	for (int axis = 0; axis < 3; ++axis)
	{
		if (!std::isfinite(constants.m_vMinBounds[axis]) || !std::isfinite(constants.m_vMaxBounds[axis])
			|| constants.m_vMinBounds[axis] >= constants.m_vMaxBounds[axis])
		{
			error = "vvis world bounds are invalid";
			return false;
		}
	}
	if (!std::isfinite(constants.m_flGridSize) || constants.m_flGridSize <= 0.0f)
	{
		error = "vvis grid size is invalid";
		return false;
	}

	struct block_layout
	{
		std::string key;
		uint32_t offset;
		uint32_t count;
		uint32_t element_size;
	};
	const std::array<block_layout, 6> blocks {{
		{"m_NodeBlock", constants.m_NodeBlock_m_nOffset, constants.m_NodeBlock_m_nElementCount, 8u},
		{"m_RegionBlock", constants.m_RegionBlock_m_nOffset, constants.m_RegionBlock_m_nElementCount, 8u},
		{"m_EnclosedClusterListBlock", constants.m_EnclosedClusterListBlock_m_nOffset, constants.m_EnclosedClusterListBlock_m_nElementCount, 8u},
		{"m_EnclosedClustersBlock", constants.m_EnclosedClustersBlock_m_nOffset, constants.m_EnclosedClustersBlock_m_nElementCount, 2u},
		{"m_MasksBlock", constants.m_MasksBlock_m_nOffset, constants.m_MasksBlock_m_nElementCount, 8u},
		{"m_nVisBlocks", constants.m_nVisBlocks_m_nOffset, constants.m_nVisBlocks_m_nElementCount, 1u},
	}};
	uint64_t expected_offset = 0;
	for (const block_layout &block : blocks)
	{
		uint64_t block_bytes = 0;
		uint64_t block_end = 0;
		if (!checked_multiply(block.count, block.element_size, block_bytes)
			|| !checked_add(block.offset, block_bytes, block_end)
			|| block_end > vxvs.size)
		{
			error = "vvis " + block.key + " block extends past the VXVS block";
			return false;
		}
		if (block.offset != expected_offset)
		{
			error = "vvis blocks do not tile the VXVS block without gaps or overlaps";
			return false;
		}
		expected_offset = block_end;
	}
	if (expected_offset != vxvs.size)
	{
		error = "vvis blocks do not tile the VXVS block without gaps or overlaps";
		return false;
	}
	uint64_t expected_vis_bytes = 0;
	if (!checked_multiply(static_cast<uint64_t>(constants.m_nBaseClusterCount) + 2u, constants.m_nPVSBytesPerCluster, expected_vis_bytes)
		|| expected_vis_bytes != constants.m_nVisBlocks_m_nElementCount)
	{
		error = "vvis vis block size does not match the cluster count";
		return false;
	}
	if (constants.m_NodeBlock_m_nElementCount == 0)
	{
		error = "vvis octree has no root node";
		return false;
	}

	const std::byte *vxvs_bytes = vvis_c_bytes.data() + vxvs.offset;
	const auto copy_u64_block = [&](uint32_t offset, uint32_t count, std::vector<uint64_t> &dest)
	{
		dest.resize(count);
		if (count != 0)
		{
			std::memcpy(dest.data(), vxvs_bytes + offset, static_cast<size_t>(count) * sizeof(uint64_t));
		}
	};
	copy_u64_block(constants.m_NodeBlock_m_nOffset, constants.m_NodeBlock_m_nElementCount, out.nodes);
	copy_u64_block(constants.m_RegionBlock_m_nOffset, constants.m_RegionBlock_m_nElementCount, out.regions);
	copy_u64_block(constants.m_MasksBlock_m_nOffset, constants.m_MasksBlock_m_nElementCount, out.masks);
	out.vis.resize(static_cast<size_t>(constants.m_nBaseClusterCount) * constants.m_nPVSBytesPerCluster);
	if (out.vis.empty())
	{
		error = "no vis blocks";
		return false;
	}

	std::memcpy(out.vis.data(), vxvs_bytes + constants.m_nVisBlocks_m_nOffset, out.vis.size());


	std::vector<uint8_t> visited(out.nodes.size(), 0u);
	std::vector<uint32_t> stack;
	stack.push_back(0u);
	visited[0] = 1u;
	uint64_t visited_count = 1;
	while (!stack.empty())
	{
		const uint32_t index = stack.back();
		stack.pop_back();
		const uint64_t node = out.nodes[index];
		if (node_is_leaf(node))
		{
			const uint64_t region_start = node_offset(node);
			const uint64_t region_count = node_region_count(node);
			if (region_start + region_count > out.regions.size())
			{
				error = "vvis leaf references regions past the region array";
				return false;
			}
			continue;
		}
		const uint64_t child_base = node_offset(node);
		if (child_base + 8u > out.nodes.size())
		{
			error = "vvis internal node references children past the node array";
			return false;
		}
		for (uint32_t child = 0; child < 8u; ++child)
		{
			const uint32_t child_index = static_cast<uint32_t>(child_base) + child;
			if (visited[child_index] != 0u)
			{
				error = "vvis octree references a node more than once";
				return false;
			}
			visited[child_index] = 1u;
			++visited_count;
			stack.push_back(child_index);
		}
	}
	if (visited_count != out.nodes.size())
	{
		error = "vvis octree does not reach every node";
		return false;
	}
	for (const uint64_t region : out.regions)
	{
		if (region_mask_index(region) >= out.masks.size())
		{
			error = "vvis region references a mask past the mask array";
			return false;
		}
	}

	out.header.base_cluster_count = constants.m_nBaseClusterCount;
	out.header.pvs_bytes_per_cluster = constants.m_nPVSBytesPerCluster;
	out.header.node_count = static_cast<uint32_t>(out.nodes.size());
	out.header.region_count = static_cast<uint32_t>(out.regions.size());
	out.header.mask_count = static_cast<uint32_t>(out.masks.size());
	for (int axis = 0; axis < 3; ++axis)
	{
		out.header.world_min[axis] = constants.m_vMinBounds[axis];
		out.header.world_max[axis] = constants.m_vMaxBounds[axis];
	}
	out.header.grid_size = constants.m_flGridSize;
	return true;
}

} // namespace cs2fow
