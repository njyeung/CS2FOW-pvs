#include "pvs.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace cs2fow
{
namespace
{

bool node_is_leaf(uint64_t node)
{
	return (node & 1u) != 0u;
}

// bits 1-31
uint32_t node_offset(uint64_t node)
{
	return static_cast<uint32_t>((node >> 1) & 0x7fffffffu);
}

// bits 32-39
uint32_t node_region_count(uint64_t node)
{
	return static_cast<uint32_t>((node >> 32) & 0xffu);
}

// bits 0-14
uint32_t region_cluster(uint64_t region)
{
	return static_cast<uint32_t>(region & 0x7fffu);
}

// bits 40-63
uint32_t region_mask_index(uint64_t region)
{
	return static_cast<uint32_t>(region >> 40);
}

vec3 to_vec3(const float values[3])
{
	return vec3 {values[0], values[1], values[2]};
}

void set_cluster_bit(std::span<uint8_t> bits, uint32_t cluster)
{
	bits[cluster >> 3] |= static_cast<uint8_t>(1u << (cluster & 7u));
}

int find_leaf_node(const pvs_data &data, vec3 point, vec3 &min, vec3 &max)
{
	if (point.x < min.x || point.y < min.y || point.z < min.z
		|| point.x > max.x || point.y > max.y || point.z > max.z)
	{
		return -1;
	}
	if (node_is_leaf(data.nodes[0]))
	{
		return 0;
	}
	uint32_t node_index = 0;
	for (;;)
	{
		const uint32_t child_base = node_offset(data.nodes[node_index]);
		const float mid_x = (min.x + max.x) * 0.5f;
		const float mid_y = (min.y + max.y) * 0.5f;
		const float mid_z = (min.z + max.z) * 0.5f;
		uint32_t octant = 0;
		if (mid_x < point.x) octant |= 1u;
		if (mid_y < point.y) octant |= 2u;
		if (mid_z < point.z) octant |= 4u;
		node_index = child_base + octant;
		if ((octant & 1u) != 0u) min.x = mid_x; else max.x = mid_x;
		if ((octant & 2u) != 0u) min.y = mid_y; else max.y = mid_y;
		if ((octant & 4u) != 0u) min.z = mid_z; else max.z = mid_z;
		if (node_index >= data.nodes.size())
		{
			return -1;
		}
		if (node_is_leaf(data.nodes[node_index]))
		{
			return static_cast<int>(node_index);
		}
	}
}

uint64_t compute_spatial_mask(vec3 point, vec3 min, vec3 max)
{
	static constexpr uint8_t level1[8] = {0, 2, 8, 10, 32, 34, 40, 42};
	static constexpr uint8_t level2[8] = {0, 1, 4, 5, 16, 17, 20, 21};
	float mid_x = (min.x + max.x) * 0.5f;
	float mid_y = (min.y + max.y) * 0.5f;
	float mid_z = (min.z + max.z) * 0.5f;
	uint32_t octant1 = 0;
	if (mid_x < point.x) octant1 |= 1u;
	if (mid_y < point.y) octant1 |= 2u;
	if (mid_z < point.z) octant1 |= 4u;
	if ((octant1 & 1u) != 0u) min.x = mid_x; else max.x = mid_x;
	if ((octant1 & 2u) != 0u) min.y = mid_y; else max.y = mid_y;
	if ((octant1 & 4u) != 0u) min.z = mid_z; else max.z = mid_z;
	mid_x = (min.x + max.x) * 0.5f;
	mid_y = (min.y + max.y) * 0.5f;
	mid_z = (min.z + max.z) * 0.5f;
	uint32_t octant2 = 0;
	if (mid_x < point.x) octant2 |= 1u;
	if (mid_y < point.y) octant2 |= 2u;
	if (mid_z < point.z) octant2 |= 4u;
	return static_cast<uint64_t>(1) << (level1[octant1] + level2[octant2]);
}

} // namespace

bool pvs_add_point_clusters(const pvs_data &data, vec3 point, std::span<uint8_t> cluster_bits)
{
	if (cluster_bits.size() != data.header.pvs_bytes_per_cluster || data.nodes.empty())
	{
		return false;
	}
	if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z))
	{
		return false;
	}
	vec3 min = to_vec3(data.header.world_min);
	vec3 max = to_vec3(data.header.world_max);
	const int leaf_index = find_leaf_node(data, point, min, max);
	if (leaf_index < 0)
	{
		return false;
	}
	const uint64_t leaf = data.nodes[static_cast<uint32_t>(leaf_index)];
	const uint32_t region_count = node_region_count(leaf);
	if (region_count == 0)
	{
		return false;
	}
	const uint64_t spatial_mask = compute_spatial_mask(point, min, max);
	const uint32_t region_start = node_offset(leaf);
	bool found = false;
	for (uint32_t r = 0; r < region_count; ++r)
	{
		const uint32_t region_index = region_start + r;
		if (region_index >= data.regions.size())
		{
			break;
		}
		const uint64_t region = data.regions[region_index];
		const uint32_t cluster = region_cluster(region);
		if ((spatial_mask & data.masks[region_mask_index(region)]) == 0 || cluster >= data.header.base_cluster_count)
		{
			continue;
		}
		set_cluster_bit(cluster_bits, cluster);
		found = true;
	}
	return found;
}

bool pvs_build_visible_clusters(const pvs_data &data, std::span<const uint8_t> cluster_bits, std::span<uint8_t> visible_bits)
{
	const uint32_t row_bytes = data.header.pvs_bytes_per_cluster;
	if (cluster_bits.size() != row_bytes || visible_bits.size() != row_bytes)
	{
		return false;
	}
	for (uint32_t byte_index = 0; byte_index < row_bytes; ++byte_index)
	{
		const uint8_t byte = cluster_bits[byte_index];
		if (byte == 0)
		{
			continue;
		}
		for (uint32_t bit = 0; bit < 8u; ++bit)
		{
			if ((byte & (1u << bit)) == 0)
			{
				continue;
			}
			const uint32_t cluster = byte_index * 8u + bit;
			if (cluster >= data.header.base_cluster_count)
			{
				continue;
			}
			const uint8_t *row = data.vis.data() + static_cast<size_t>(cluster) * row_bytes;
			for (uint32_t i = 0; i < row_bytes; ++i)
			{
				visible_bits[i] |= row[i];
			}
		}
	}
	return true;
}

bool pvs_clusters_intersect(std::span<const uint8_t> observer_visible_bits, std::span<const uint8_t> target_cluster_bits)
{
	const size_t count = std::min(observer_visible_bits.size(), target_cluster_bits.size());
	for (size_t i = 0; i < count; ++i)
	{
		if ((observer_visible_bits[i] & target_cluster_bits[i]) != 0)
		{
			return true;
		}
	}
	return false;
}

} // namespace cs2fow
