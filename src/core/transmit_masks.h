#pragma once

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace cs2fow
{

inline constexpr size_t k_checktransmit_aux_mask_count = 3;
inline constexpr size_t k_checktransmit_max_mask_count = 1 + k_checktransmit_aux_mask_count;

struct checktransmit_private_offsets
{
	std::array<uint32_t, k_checktransmit_aux_mask_count> aux_mask_offsets {};
	uint32_t full_update_offset {};
};

template <typename mask_type>
struct transmit_masks
{
	std::array<mask_type *, k_checktransmit_max_mask_count> values {};
	mask_type *primary {};
	size_t count {};
};

inline std::string_view trim_ascii(std::string_view text)
{
	while (!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '\r' || text.front() == '\n'))
	{
		text.remove_prefix(1);
	}
	while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r' || text.back() == '\n'))
	{
		text.remove_suffix(1);
	}
	return text;
}

inline bool parse_gamedata_uint32(std::string_view text, uint32_t &value)
{
	text = trim_ascii(text);
	const auto [end, conversion_error] = std::from_chars(text.data(), text.data() + text.size(), value);
	return !text.empty() && conversion_error == std::errc {} && end == text.data() + text.size();
}

inline bool valid_gamedata_offset(uint32_t offset, uint32_t alignment, uint32_t max_offset)
{
	return offset != 0 && offset <= max_offset && offset % alignment == 0;
}

inline bool parse_checktransmit_aux_mask_offsets(std::string_view text,
	std::array<uint32_t, k_checktransmit_aux_mask_count> &offsets, uint32_t max_offset)
{
	offsets = {};
	size_t count = 0;
	for (;;)
	{
		const size_t comma = text.find(',');
		const std::string_view part = comma == std::string_view::npos ? text : text.substr(0, comma);
		uint32_t value {};
		if (count >= offsets.size() || !parse_gamedata_uint32(part, value)
			|| !valid_gamedata_offset(value, static_cast<uint32_t>(alignof(void *)), max_offset))
		{
			return false;
		}
		for (size_t index = 0; index < count; ++index)
		{
			if (offsets[index] == value)
			{
				return false;
			}
		}
		offsets[count++] = value;
		if (comma == std::string_view::npos)
		{
			break;
		}
		text.remove_prefix(comma + 1);
		if (text.empty())
		{
			return false;
		}
	}
	return count == offsets.size();
}

inline bool transmit_pointer_aligned(const void *pointer)
{
	return (reinterpret_cast<uintptr_t>(pointer) % alignof(void *)) == 0;
}

template <typename mask_type>
inline bool collect_transmit_masks(const void *info, mask_type *primary,
	const std::array<uint32_t, k_checktransmit_aux_mask_count> &aux_offsets, transmit_masks<mask_type> &masks)
{
	masks = {};
	if (info == nullptr || primary == nullptr || !transmit_pointer_aligned(primary))
	{
		return false;
	}
	masks.primary = primary;
	masks.values[masks.count++] = primary;
	const char *bytes = static_cast<const char *>(info);
	for (uint32_t offset : aux_offsets)
	{
		mask_type *mask {};
		std::memcpy(&mask, bytes + offset, sizeof(mask));
		if (mask == nullptr || !transmit_pointer_aligned(mask))
		{
			return false;
		}
		bool duplicate = false;
		for (size_t index = 0; index < masks.count; ++index)
		{
			duplicate = duplicate || masks.values[index] == mask;
		}
		if (!duplicate)
		{
			masks.values[masks.count++] = mask;
		}
	}
	return true;
}

inline bool read_checktransmit_full_update(const void *info, uint32_t offset)
{
	bool value {};
	if (info != nullptr)
	{
		std::memcpy(&value, static_cast<const char *>(info) + offset, sizeof(value));
	}
	return value;
}

template <typename mask_type, typename handle_type, size_t max_count, typename resolver_type, typename valid_type>
inline void clear_transmit_group(const transmit_masks<mask_type> &masks, const std::array<handle_type, max_count> &handles,
	size_t count, resolver_type resolver, valid_type valid)
{
	for (size_t entity = 0; entity < count; ++entity)
	{
		const int index = resolver(handles[entity]);
		if (!valid(index))
		{
			continue;
		}
		for (size_t mask = 0; mask < masks.count; ++mask)
		{
			masks.values[mask]->Clear(index);
		}
	}
}

} // namespace cs2fow
