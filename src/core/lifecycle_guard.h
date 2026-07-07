#pragma once

#include <chrono>
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

} // namespace cs2fow
