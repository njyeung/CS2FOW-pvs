#pragma once


#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace cs2fow
{

struct process_result
{
	int exit_code {-1};
	bool cancelled {};
	bool timed_out {};
};

bool run_process(const std::filesystem::path &executable, const std::vector<std::string> &arguments,
	std::chrono::milliseconds timeout, const std::atomic_bool *cancel, bool low_priority,
	process_result &result, std::string &error);
bool lower_process_priority(std::string &error);

} // namespace cs2fow
