#include "bvh8.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <immintrin.h>
#include <limits>

#if defined(_WIN32)
#include <intrin.h>
#else
#include <cpuid.h>
#endif

namespace cs2fow
{
namespace
{

constexpr char k_magic[8] = {'C', 'S', '2', 'F', 'O', 'W', '8', '\0'};
constexpr float k_ray_epsilon = 1.0e-5f;

uint64_t align_up(uint64_t value, uint64_t alignment)
{
	return (value + alignment - 1u) & ~(alignment - 1u);
}

bool finite_bounds(const float *minimum, const float *maximum)
{
	for (int i = 0; i < 3; ++i)
	{
		if (!std::isfinite(minimum[i]) || !std::isfinite(maximum[i]) || minimum[i] > maximum[i])
		{
			return false;
		}
	}
	return true;
}

bool read_exact(std::ifstream &stream, void *destination, size_t size)
{
	stream.read(static_cast<char *>(destination), static_cast<std::streamsize>(size));
	return stream.good();
}

uint32_t packet_mask(uint32_t count)
{
	return count == 8 ? 0xffu : ((1u << count) - 1u);
}

uint32_t hit_packet(const triangle_packet8 &packet, uint32_t count, vec3 origin, vec3 direction)
{
	const __m256 ox = _mm256_set1_ps(origin.x);
	const __m256 oy = _mm256_set1_ps(origin.y);
	const __m256 oz = _mm256_set1_ps(origin.z);
	const __m256 dx = _mm256_set1_ps(direction.x);
	const __m256 dy = _mm256_set1_ps(direction.y);
	const __m256 dz = _mm256_set1_ps(direction.z);
	const __m256 e1x = _mm256_load_ps(packet.edge1_x);
	const __m256 e1y = _mm256_load_ps(packet.edge1_y);
	const __m256 e1z = _mm256_load_ps(packet.edge1_z);
	const __m256 e2x = _mm256_load_ps(packet.edge2_x);
	const __m256 e2y = _mm256_load_ps(packet.edge2_y);
	const __m256 e2z = _mm256_load_ps(packet.edge2_z);

	const __m256 px = _mm256_sub_ps(_mm256_mul_ps(dy, e2z), _mm256_mul_ps(dz, e2y));
	const __m256 py = _mm256_sub_ps(_mm256_mul_ps(dz, e2x), _mm256_mul_ps(dx, e2z));
	const __m256 pz = _mm256_sub_ps(_mm256_mul_ps(dx, e2y), _mm256_mul_ps(dy, e2x));
	const __m256 det = _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(e1x, px), _mm256_mul_ps(e1y, py)), _mm256_mul_ps(e1z, pz));
	const __m256 abs_det = _mm256_andnot_ps(_mm256_set1_ps(-0.0f), det);
	__m256 valid = _mm256_cmp_ps(abs_det, _mm256_set1_ps(k_ray_epsilon), _CMP_GT_OQ);
	const __m256 inv_det = _mm256_div_ps(_mm256_set1_ps(1.0f), det);

	const __m256 tx = _mm256_sub_ps(ox, _mm256_load_ps(packet.v0_x));
	const __m256 ty = _mm256_sub_ps(oy, _mm256_load_ps(packet.v0_y));
	const __m256 tz = _mm256_sub_ps(oz, _mm256_load_ps(packet.v0_z));
	const __m256 u = _mm256_mul_ps(_mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(tx, px), _mm256_mul_ps(ty, py)), _mm256_mul_ps(tz, pz)), inv_det);
	valid = _mm256_and_ps(valid, _mm256_cmp_ps(u, _mm256_setzero_ps(), _CMP_GE_OQ));
	valid = _mm256_and_ps(valid, _mm256_cmp_ps(u, _mm256_set1_ps(1.0f), _CMP_LE_OQ));

	const __m256 qx = _mm256_sub_ps(_mm256_mul_ps(ty, e1z), _mm256_mul_ps(tz, e1y));
	const __m256 qy = _mm256_sub_ps(_mm256_mul_ps(tz, e1x), _mm256_mul_ps(tx, e1z));
	const __m256 qz = _mm256_sub_ps(_mm256_mul_ps(tx, e1y), _mm256_mul_ps(ty, e1x));
	const __m256 v = _mm256_mul_ps(_mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(dx, qx), _mm256_mul_ps(dy, qy)), _mm256_mul_ps(dz, qz)), inv_det);
	valid = _mm256_and_ps(valid, _mm256_cmp_ps(v, _mm256_setzero_ps(), _CMP_GE_OQ));
	valid = _mm256_and_ps(valid, _mm256_cmp_ps(_mm256_add_ps(u, v), _mm256_set1_ps(1.0f), _CMP_LE_OQ));

	const __m256 distance = _mm256_mul_ps(_mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(e2x, qx), _mm256_mul_ps(e2y, qy)), _mm256_mul_ps(e2z, qz)), inv_det);
	valid = _mm256_and_ps(valid, _mm256_cmp_ps(distance, _mm256_set1_ps(k_ray_epsilon), _CMP_GT_OQ));
	valid = _mm256_and_ps(valid, _mm256_cmp_ps(distance, _mm256_set1_ps(1.0f - k_ray_epsilon), _CMP_LT_OQ));
	return static_cast<uint32_t>(_mm256_movemask_ps(valid)) & packet_mask(count);
}

uint32_t hit_children(const bvh8_node &node, vec3 origin, vec3 direction)
{
	__m256 near_t = _mm256_set1_ps(-std::numeric_limits<float>::infinity());
	__m256 far_t = _mm256_set1_ps(std::numeric_limits<float>::infinity());
	const float origins[3] = {origin.x, origin.y, origin.z};
	const float directions[3] = {direction.x, direction.y, direction.z};
	const float *mins[3] = {node.min_x, node.min_y, node.min_z};
	const float *maxs[3] = {node.max_x, node.max_y, node.max_z};

	for (int axis = 0; axis < 3; ++axis)
	{
		const __m256 minimum = _mm256_load_ps(mins[axis]);
		const __m256 maximum = _mm256_load_ps(maxs[axis]);
		const __m256 position = _mm256_set1_ps(origins[axis]);
		if (std::fabs(directions[axis]) < 1.0e-12f)
		{
			const __m256 inside = _mm256_and_ps(_mm256_cmp_ps(position, minimum, _CMP_GE_OQ), _mm256_cmp_ps(position, maximum, _CMP_LE_OQ));
			far_t = _mm256_blendv_ps(_mm256_set1_ps(-std::numeric_limits<float>::infinity()), far_t, inside);
			continue;
		}

		const __m256 inverse = _mm256_set1_ps(1.0f / directions[axis]);
		const __m256 a = _mm256_mul_ps(_mm256_sub_ps(minimum, position), inverse);
		const __m256 b = _mm256_mul_ps(_mm256_sub_ps(maximum, position), inverse);
		near_t = _mm256_max_ps(near_t, _mm256_min_ps(a, b));
		far_t = _mm256_min_ps(far_t, _mm256_max_ps(a, b));
	}

	__m256 hit = _mm256_cmp_ps(far_t, _mm256_max_ps(near_t, _mm256_setzero_ps()), _CMP_GE_OQ);
	hit = _mm256_and_ps(hit, _mm256_cmp_ps(near_t, _mm256_set1_ps(1.0f - k_ray_epsilon), _CMP_LT_OQ));
	uint32_t mask = static_cast<uint32_t>(_mm256_movemask_ps(hit));
	for (uint32_t i = 0; i < 8; ++i)
	{
		if (node.child[i] == k_invalid_ref)
		{
			mask &= ~(1u << i);
		}
	}
	return mask;
}

} // namespace

uint32_t crc32(std::span<const std::byte> bytes)
{
	uint32_t value = 0xffffffffu;
	for (const std::byte byte : bytes)
	{
		value ^= static_cast<uint8_t>(byte);
		for (int bit = 0; bit < 8; ++bit)
		{
			value = (value >> 1u) ^ (0xedb88320u & (0u - (value & 1u)));
		}
	}
	return ~value;
}

bool cpu_supports_avx()
{
#if defined(_WIN32)
	int registers[4] {};
	__cpuid(registers, 1);
	return (registers[2] & (1 << 27)) != 0 && (registers[2] & (1 << 28)) != 0 && (_xgetbv(0) & 6) == 6;
#else
	unsigned int eax = 0;
	unsigned int ebx = 0;
	unsigned int ecx = 0;
	unsigned int edx = 0;
	if (!__get_cpuid(1, &eax, &ebx, &ecx, &edx) || (ecx & bit_OSXSAVE) == 0 || (ecx & bit_AVX) == 0)
	{
		return false;
	}
	uint32_t xcr0_low = 0;
	uint32_t xcr0_high = 0;
	__asm__ volatile("xgetbv" : "=a"(xcr0_low), "=d"(xcr0_high) : "c"(0));
	return (xcr0_low & 6u) == 6u;
#endif
}

bool validate_bvh8(const bvh8_data &data, std::string &error)
{
	const bvh8_header &header = data.header;
	if (std::memcmp(header.magic, k_magic, sizeof(k_magic)) != 0 || header.version != k_bvh8_version || header.header_size != sizeof(bvh8_header))
	{
		error = "invalid BVH8 magic or version";
		return false;
	}
	if ((header.flags & ~k_bvh8_known_flags) != 0)
	{
		error = "BVH8 contains unsupported flags";
		return false;
	}
	if (header.map_name[0] == '\0' || header.map_name[sizeof(header.map_name) - 1] != '\0')
	{
		error = "invalid map name";
		return false;
	}
	if (header.node_count == 0 || header.packet_count == 0 || header.triangle_count == 0 || header.max_depth == 0 || header.max_depth > k_max_tree_depth)
	{
		error = "invalid BVH8 counts or depth";
		return false;
	}
	if (header.node_count != data.nodes.size() || header.packet_count != data.packets.size() || !finite_bounds(header.world_min, header.world_max))
	{
		error = "BVH8 header does not match payload";
		return false;
	}
	if ((header.nodes_offset & 31u) != 0 || (header.packets_offset & 31u) != 0 || header.nodes_offset < sizeof(bvh8_header))
	{
		error = "BVH8 payload alignment is invalid";
		return false;
	}
	const uint64_t expected_packets = header.nodes_offset + static_cast<uint64_t>(header.node_count) * sizeof(bvh8_node);
	const uint64_t expected_size = header.packets_offset + static_cast<uint64_t>(header.packet_count) * sizeof(triangle_packet8);
	if (header.packets_offset != align_up(expected_packets, 32) || header.file_size != expected_size)
	{
		error = "BVH8 offsets or file size are invalid";
		return false;
	}

	uint64_t counted_triangles = 0;
	for (uint32_t node_index = 0; node_index < header.node_count; ++node_index)
	{
		const bvh8_node &node = data.nodes[node_index];
		for (uint32_t child_index = 0; child_index < 8; ++child_index)
		{
			const uint32_t ref = node.child[child_index];
			if (ref == k_invalid_ref)
			{
				continue;
			}
			const float minimum[3] = {node.min_x[child_index], node.min_y[child_index], node.min_z[child_index]};
			const float maximum[3] = {node.max_x[child_index], node.max_y[child_index], node.max_z[child_index]};
			if (!finite_bounds(minimum, maximum))
			{
				error = "BVH8 contains invalid child bounds";
				return false;
			}
			if (is_leaf_ref(ref))
			{
				if (leaf_index(ref) >= header.packet_count)
				{
					error = "BVH8 leaf reference is out of range";
					return false;
				}
				counted_triangles += leaf_count(ref);
			}
			else if (ref >= header.node_count || ref <= node_index)
			{
				error = "BVH8 node reference is invalid";
				return false;
			}
		}
	}
	if (counted_triangles != header.triangle_count)
	{
		error = "BVH8 triangle count is inconsistent";
		return false;
	}
	return true;
}

bool load_bvh8(const std::filesystem::path &path, bvh8_data &data, std::string &error)
{
	data = {};
	std::ifstream stream(path, std::ios::binary | std::ios::ate);
	if (!stream)
	{
		error = "could not open BVH8 file";
		return false;
	}
	const uint64_t actual_size = static_cast<uint64_t>(stream.tellg());
	stream.seekg(0);
	if (actual_size < sizeof(bvh8_header) || !read_exact(stream, &data.header, sizeof(data.header)) || data.header.file_size != actual_size)
	{
		error = "BVH8 file is truncated or has the wrong size";
		return false;
	}
	if (data.header.node_count > 100000000u || data.header.packet_count > 100000000u)
	{
		error = "BVH8 count exceeds safety limit";
		return false;
	}
	data.nodes.resize(data.header.node_count);
	data.packets.resize(data.header.packet_count);
	stream.seekg(static_cast<std::streamoff>(data.header.nodes_offset));
	if (!read_exact(stream, data.nodes.data(), data.nodes.size() * sizeof(bvh8_node)))
	{
		error = "could not read BVH8 nodes";
		return false;
	}
	stream.seekg(static_cast<std::streamoff>(data.header.packets_offset));
	if (!read_exact(stream, data.packets.data(), data.packets.size() * sizeof(triangle_packet8)))
	{
		error = "could not read BVH8 packets";
		return false;
	}
	if (!validate_bvh8(data, error))
	{
		return false;
	}
	std::vector<std::byte> payload(data.nodes.size() * sizeof(bvh8_node) + data.packets.size() * sizeof(triangle_packet8));
	std::memcpy(payload.data(), data.nodes.data(), data.nodes.size() * sizeof(bvh8_node));
	std::memcpy(payload.data() + data.nodes.size() * sizeof(bvh8_node), data.packets.data(), data.packets.size() * sizeof(triangle_packet8));
	if (crc32(payload) != data.header.payload_crc32)
	{
		error = "BVH8 payload CRC mismatch";
		return false;
	}
	return true;
}

bool write_bvh8(const std::filesystem::path &path, bvh8_data &data, std::string &error)
{
	std::memcpy(data.header.magic, k_magic, sizeof(k_magic));
	data.header.version = k_bvh8_version;
	data.header.header_size = sizeof(bvh8_header);
	data.header.node_count = static_cast<uint32_t>(data.nodes.size());
	data.header.packet_count = static_cast<uint32_t>(data.packets.size());
	data.header.nodes_offset = align_up(sizeof(bvh8_header), 32);
	data.header.packets_offset = align_up(data.header.nodes_offset + data.nodes.size() * sizeof(bvh8_node), 32);
	data.header.file_size = data.header.packets_offset + data.packets.size() * sizeof(triangle_packet8);
	std::vector<std::byte> payload(data.nodes.size() * sizeof(bvh8_node) + data.packets.size() * sizeof(triangle_packet8));
	std::memcpy(payload.data(), data.nodes.data(), data.nodes.size() * sizeof(bvh8_node));
	std::memcpy(payload.data() + data.nodes.size() * sizeof(bvh8_node), data.packets.data(), data.packets.size() * sizeof(triangle_packet8));
	data.header.payload_crc32 = crc32(payload);
	if (!validate_bvh8(data, error))
	{
		return false;
	}

	std::error_code filesystem_error;
	std::filesystem::create_directories(path.parent_path(), filesystem_error);
	const std::filesystem::path temporary = path.string() + ".tmp";
	std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
	if (!stream)
	{
		error = "could not create temporary BVH8 file";
		return false;
	}
	stream.write(reinterpret_cast<const char *>(&data.header), sizeof(data.header));
	stream.seekp(static_cast<std::streamoff>(data.header.nodes_offset));
	stream.write(reinterpret_cast<const char *>(data.nodes.data()), static_cast<std::streamsize>(data.nodes.size() * sizeof(bvh8_node)));
	stream.seekp(static_cast<std::streamoff>(data.header.packets_offset));
	stream.write(reinterpret_cast<const char *>(data.packets.data()), static_cast<std::streamsize>(data.packets.size() * sizeof(triangle_packet8)));
	stream.flush();
	if (!stream.good())
	{
		error = "failed while writing BVH8 file";
		stream.close();
		std::filesystem::remove(temporary, filesystem_error);
		return false;
	}
	stream.close();
	std::filesystem::remove(path, filesystem_error);
	filesystem_error.clear();
	std::filesystem::rename(temporary, path, filesystem_error);
	if (filesystem_error)
	{
		error = "could not atomically install BVH8 file: " + filesystem_error.message();
		std::filesystem::remove(temporary, filesystem_error);
		return false;
	}
	return true;
}

bool packet_blocks_segment(const triangle_packet8 &packet, uint32_t count, vec3 origin, vec3 target)
{
	return hit_packet(packet, count, origin, {target.x - origin.x, target.y - origin.y, target.z - origin.z}) != 0;
}

ray_hit segment_blocked(const bvh8_data &data, vec3 origin, vec3 target, uint32_t cached_packet)
{
	const vec3 direction {target.x - origin.x, target.y - origin.y, target.z - origin.z};
	if (cached_packet < data.packets.size() && hit_packet(data.packets[cached_packet], 8, origin, direction) != 0)
	{
		return {true, cached_packet};
	}

	uint32_t stack[512] {};
	uint32_t stack_size = 1;
	stack[0] = 0;
	while (stack_size != 0)
	{
		const uint32_t node_index = stack[--stack_size];
		const bvh8_node &node = data.nodes[node_index];
		uint32_t mask = hit_children(node, origin, direction);
		while (mask != 0)
		{
			const uint32_t lane = std::countr_zero(mask);
			mask &= mask - 1u;
			const uint32_t ref = node.child[lane];
			if (is_leaf_ref(ref))
			{
				const uint32_t packet_index = leaf_index(ref);
				if (packet_index != cached_packet && hit_packet(data.packets[packet_index], leaf_count(ref), origin, direction) != 0)
				{
					return {true, packet_index};
				}
			}
			else if (stack_size < std::size(stack))
			{
				stack[stack_size++] = ref;
			}
			else
			{
				return {};
			}
		}
	}
	return {};
}

} // namespace cs2fow
