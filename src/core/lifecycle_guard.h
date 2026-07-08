#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace cs2fow
{

struct lifecycle_key
{
	bool has_controller {};
	bool hltv {};
	int pawn_entity {-1};
	uint8_t team {};
	bool alive {};
	bool spawning {};
	int32_t death_flags {};
	bool has_death_info {};
	float death_time {};
	float death_info_time {};
};

struct lifecycle_guard
{
	lifecycle_key key;
	std::chrono::steady_clock::time_point fail_open_until {};
	bool initialized {};
};

struct pair_guard
{
	lifecycle_key observer_key;
	lifecycle_key target_key;
	std::chrono::steady_clock::time_point fail_open_until {};
	uint64_t baseline_sequence {};
	bool baseline_opened {};
	bool initialized {};
};

template <typename handle_type, size_t max_count>
struct hidden_entity_group
{
	std::array<handle_type, max_count> handles {};
	handle_type source {};
	size_t count {};
	std::chrono::steady_clock::time_point quarantine_until {};
};

inline bool lifecycle_changed(const lifecycle_key &left, const lifecycle_key &right)
{
	return left.has_controller != right.has_controller
		|| left.hltv != right.hltv
		|| left.pawn_entity != right.pawn_entity
		|| left.team != right.team
		|| left.alive != right.alive
		|| left.spawning != right.spawning
		|| left.death_flags != right.death_flags
		|| left.has_death_info != right.has_death_info
		|| left.death_time != right.death_time
		|| left.death_info_time != right.death_info_time;
}

inline void update_lifecycle_guard(lifecycle_guard &guard, const lifecycle_key &key, bool stable,
	std::chrono::steady_clock::time_point now, std::chrono::milliseconds grace)
{
	if (!guard.initialized || lifecycle_changed(guard.key, key) || !stable)
	{
		guard.fail_open_until = now + grace;
	}
	guard.key = key;
	guard.initialized = true;
}

inline bool lifecycle_allows_hiding(const lifecycle_guard &guard, std::chrono::steady_clock::time_point now)
{
	return guard.initialized && now >= guard.fail_open_until;
}

inline void update_pair_guard(pair_guard &guard, const lifecycle_key &observer_key, bool observer_stable,
	const lifecycle_key &target_key, bool target_stable, std::chrono::steady_clock::time_point now, std::chrono::milliseconds warmup)
{
	if (!guard.initialized || lifecycle_changed(guard.observer_key, observer_key)
		|| lifecycle_changed(guard.target_key, target_key) || !observer_stable || !target_stable)
	{
		guard.fail_open_until = now + warmup;
		guard.baseline_sequence = 0;
		guard.baseline_opened = false;
	}
	guard.observer_key = observer_key;
	guard.target_key = target_key;
	guard.initialized = true;
}

inline void pair_note_open(pair_guard &guard, std::chrono::steady_clock::time_point now, uint64_t sequence)
{
	if (guard.initialized && now >= guard.fail_open_until && !guard.baseline_opened)
	{
		guard.baseline_sequence = sequence;
		guard.baseline_opened = true;
	}
}

inline bool pair_allows_hiding(const pair_guard &guard, std::chrono::steady_clock::time_point now, uint64_t sequence)
{
	return guard.initialized && now >= guard.fail_open_until && guard.baseline_opened && guard.baseline_sequence != sequence;
}

template <typename handle_type, size_t max_count>
inline void hidden_group_clear(hidden_entity_group<handle_type, max_count> &group)
{
	group = {};
}

template <typename handle_type, size_t max_count>
inline void hidden_group_store(hidden_entity_group<handle_type, max_count> &group,
	const handle_type &source, const std::array<handle_type, max_count> &handles, size_t count,
	std::chrono::steady_clock::time_point now, std::chrono::milliseconds quarantine)
{
	group.source = source;
	group.handles = handles;
	group.count = count > max_count ? max_count : count;
	group.quarantine_until = now + quarantine;
}

template <typename handle_type, size_t max_count>
inline bool hidden_group_quarantined(hidden_entity_group<handle_type, max_count> &group, std::chrono::steady_clock::time_point now)
{
	if (group.count == 0)
	{
		return false;
	}
	if (now >= group.quarantine_until)
	{
		hidden_group_clear(group);
		return false;
	}
	return true;
}

template <typename handle_type, size_t max_count, typename predicate_type>
inline bool hidden_group_all_of(const hidden_entity_group<handle_type, max_count> &group, predicate_type predicate)
{
	if (group.count == 0)
	{
		return false;
	}
	for (size_t index = 0; index < group.count; ++index)
	{
		if (!predicate(group.handles[index]))
		{
			return false;
		}
	}
	return true;
}

} // namespace cs2fow
