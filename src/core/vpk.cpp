#include "vpk.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>

namespace cs2fow
{
namespace
{

constexpr uint32_t k_vpk_signature = 0x55aa1234u;
constexpr uint16_t k_entry_terminator = 0xffffu;

template <typename type>
bool read_value(std::ifstream &stream, type &value)
{
	stream.read(reinterpret_cast<char *>(&value), sizeof(value));
	return stream.good();
}

bool read_string(std::ifstream &stream, std::streampos tree_end, std::string &value)
{
	value.clear();
	while (stream.tellg() < tree_end)
	{
		char character = '\0';
		if (!stream.get(character))
		{
			return false;
		}
		if (character == '\0')
		{
			return true;
		}
		if (value.size() >= 4096)
		{
			return false;
		}
		value.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
	}
	return false;
}

std::string normalized(std::string value)
{
	std::replace(value.begin(), value.end(), '\\', '/');
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	while (!value.empty() && value.front() == '/')
	{
		value.erase(value.begin());
	}
	return value;
}

} // namespace

bool find_vpk_entry(const std::filesystem::path &vpk_path, const std::string &entry_path, vpk_entry &entry, std::string &error)
{
	std::ifstream stream(vpk_path, std::ios::binary | std::ios::ate);
	if (!stream)
	{
		error = "could not open VPK";
		return false;
	}
	const uint64_t file_size = static_cast<uint64_t>(stream.tellg());
	stream.seekg(0);
	uint32_t signature = 0;
	uint32_t version = 0;
	uint32_t tree_size = 0;
	if (!read_value(stream, signature) || !read_value(stream, version) || !read_value(stream, tree_size) || signature != k_vpk_signature || (version != 1 && version != 2))
	{
		error = "invalid or unsupported VPK header";
		return false;
	}
	const uint32_t header_size = version == 1 ? 12u : 28u;
	if (version == 2)
	{
		uint32_t ignored[4] {};
		for (uint32_t &value : ignored)
		{
			if (!read_value(stream, value))
			{
				error = "truncated VPK v2 header";
				return false;
			}
		}
	}
	if (tree_size == 0 || static_cast<uint64_t>(header_size) + tree_size > file_size)
	{
		error = "VPK directory tree is invalid";
		return false;
	}
	const std::streampos tree_end = static_cast<std::streamoff>(header_size + tree_size);
	const std::string wanted = normalized(entry_path);

	for (;;)
	{
		std::string extension;
		if (!read_string(stream, tree_end, extension))
		{
			error = "malformed VPK extension tree";
			return false;
		}
		if (extension.empty())
		{
			break;
		}
		for (;;)
		{
			std::string directory;
			if (!read_string(stream, tree_end, directory))
			{
				error = "malformed VPK path tree";
				return false;
			}
			if (directory.empty())
			{
				break;
			}
			if (directory == " ")
			{
				directory.clear();
			}
			for (;;)
			{
				std::string name;
				if (!read_string(stream, tree_end, name))
				{
					error = "malformed VPK filename tree";
					return false;
				}
				if (name.empty())
				{
					break;
				}
				uint32_t crc = 0;
				uint16_t preload_size = 0;
				uint16_t archive_index = 0;
				uint32_t archive_offset = 0;
				uint32_t archive_length = 0;
				uint16_t terminator = 0;
				if (!read_value(stream, crc) || !read_value(stream, preload_size) || !read_value(stream, archive_index)
					|| !read_value(stream, archive_offset) || !read_value(stream, archive_length) || !read_value(stream, terminator)
					|| terminator != k_entry_terminator || stream.tellg() + static_cast<std::streamoff>(preload_size) > tree_end)
				{
					error = "malformed VPK entry";
					return false;
				}
				const std::string full_path = (directory.empty() ? name : directory + "/" + name) + "." + extension;
				if (full_path == wanted)
				{
					entry = {crc, static_cast<uint64_t>(preload_size) + archive_length, archive_index, archive_offset};
					return true;
				}
				stream.seekg(preload_size, std::ios::cur);
			}
		}
	}
	error = "VPK entry not found: " + wanted;
	return false;
}

} // namespace cs2fow

