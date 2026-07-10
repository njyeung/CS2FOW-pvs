#include "subprocess.h"

// Runs one external program and captures its output on a non-game thread or in
// the command-line baker. Platform handles are always closed, and timeout or
// cancellation terminates the child and returns an ordinary result/error.

#include <thread>

#if defined(_WIN32)
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <cstring>
#include <spawn.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif

namespace cs2fow
{
namespace
{

#if defined(_WIN32)
std::wstring widen(const std::string &value)
{
	if (value.empty())
	{
		return {};
	}
	const int size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
	std::wstring result(static_cast<size_t>(size), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size);
	return result;
}

std::wstring quote_argument(const std::wstring &value)
{
	if (value.find_first_of(L" \t\"") == std::wstring::npos)
	{
		return value;
	}
	std::wstring result = L"\"";
	size_t backslashes = 0;
	for (const wchar_t character : value)
	{
		if (character == L'\\')
		{
			++backslashes;
			continue;
		}
		if (character == L'\"')
		{
			result.append(backslashes * 2u + 1u, L'\\');
			result.push_back(character);
		}
		else
		{
			result.append(backslashes, L'\\');
			result.push_back(character);
		}
		backslashes = 0;
	}
	result.append(backslashes * 2u, L'\\');
	result.push_back(L'\"');
	return result;
}
#endif

} // namespace

bool lower_process_priority(std::string &error)
{
#if defined(_WIN32)
	if (!SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS))
	{
		error = "could not lower process priority";
		return false;
	}
#else
	if (setpriority(PRIO_PROCESS, 0, 10) != 0)
	{
		error = std::string("could not lower process priority: ") + std::strerror(errno);
		return false;
	}
#endif
	return true;
}

bool run_process(const std::filesystem::path &executable, const std::vector<std::string> &arguments,
	std::chrono::milliseconds timeout, const std::atomic_bool *cancel, bool low_priority,
	process_result &result, std::string &error)
{
	result = {};
	if (!std::filesystem::exists(executable))
	{
		error = "executable not found: " + executable.string();
		return false;
	}
	const auto started = std::chrono::steady_clock::now();
#if defined(_WIN32)
	const std::wstring executable_wide = executable.wstring();
	std::wstring command = quote_argument(executable_wide);
	for (const std::string &argument : arguments)
	{
		command.push_back(L' ');
		command += quote_argument(widen(argument));
	}
	std::vector<wchar_t> mutable_command(command.begin(), command.end());
	mutable_command.push_back(L'\0');
	STARTUPINFOW startup {};
	startup.cb = sizeof(startup);
	PROCESS_INFORMATION process {};
	HANDLE job = CreateJobObjectW(nullptr, nullptr);
	if (job == nullptr)
	{
		error = "could not create process job";
		return false;
	}
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits {};
	limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
	{
		CloseHandle(job);
		error = "could not configure process job";
		return false;
	}
	const DWORD flags = CREATE_SUSPENDED | CREATE_NO_WINDOW | (low_priority ? BELOW_NORMAL_PRIORITY_CLASS : 0);
	if (!CreateProcessW(executable_wide.c_str(), mutable_command.data(), nullptr, nullptr, FALSE, flags, nullptr, nullptr, &startup, &process))
	{
		CloseHandle(job);
		error = "could not start process";
		return false;
	}
	if (!AssignProcessToJobObject(job, process.hProcess))
	{
		TerminateProcess(process.hProcess, 1);
		CloseHandle(process.hThread);
		CloseHandle(process.hProcess);
		CloseHandle(job);
		error = "could not assign process job";
		return false;
	}
	ResumeThread(process.hThread);
	for (;;)
	{
		if (WaitForSingleObject(process.hProcess, 10) == WAIT_OBJECT_0)
		{
			break;
		}
		if (cancel != nullptr && cancel->load())
		{
			result.cancelled = true;
			TerminateJobObject(job, 1);
			WaitForSingleObject(process.hProcess, INFINITE);
			break;
		}
		if (timeout.count() > 0 && std::chrono::steady_clock::now() - started >= timeout)
		{
			result.timed_out = true;
			TerminateJobObject(job, 1);
			WaitForSingleObject(process.hProcess, INFINITE);
			break;
		}
	}
	DWORD exit_code = 1;
	GetExitCodeProcess(process.hProcess, &exit_code);
	result.exit_code = static_cast<int>(exit_code);
	CloseHandle(process.hThread);
	CloseHandle(process.hProcess);
	CloseHandle(job);
#else
	std::string executable_string = executable.string();
	std::vector<std::string> values;
	values.reserve(arguments.size() + 1u);
	values.push_back(executable_string);
	values.insert(values.end(), arguments.begin(), arguments.end());
	std::vector<char *> argv;
	argv.reserve(values.size() + 1u);
	for (std::string &value : values)
	{
		argv.push_back(value.data());
	}
	argv.push_back(nullptr);
	posix_spawnattr_t attributes;
	posix_spawnattr_init(&attributes);
	posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
	posix_spawnattr_setpgroup(&attributes, 0);
	pid_t pid = 0;
	const int spawn_error = posix_spawn(&pid, executable_string.c_str(), nullptr, &attributes, argv.data(), environ);
	posix_spawnattr_destroy(&attributes);
	if (spawn_error != 0)
	{
		error = std::string("could not start process: ") + std::strerror(spawn_error);
		return false;
	}
	int status = 0;
	for (;;)
	{
		const pid_t waited = waitpid(pid, &status, WNOHANG);
		if (waited == pid)
		{
			break;
		}
		if (waited < 0 && errno != EINTR)
		{
			error = std::string("could not wait for process: ") + std::strerror(errno);
			return false;
		}
		if (cancel != nullptr && cancel->load())
		{
			result.cancelled = true;
			kill(-pid, SIGKILL);
			waitpid(pid, &status, 0);
			break;
		}
		if (timeout.count() > 0 && std::chrono::steady_clock::now() - started >= timeout)
		{
			result.timed_out = true;
			kill(-pid, SIGKILL);
			waitpid(pid, &status, 0);
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
	(void)low_priority;
#endif
	return true;
}

} // namespace cs2fow
