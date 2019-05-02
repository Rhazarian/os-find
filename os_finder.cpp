#include "os_finder.h"

#include <array>
#include <algorithm>

#include <sys/syscall.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#include <iostream>

namespace
{
	struct linux_dirent64 {
		ino64_t d_ino;
		off64_t d_off;
		unsigned short d_reclen;
		unsigned char d_type;
		char d_name[];
	};

	struct fd_guard
	{
		fd_guard(const char* pathname, const int flags)
		{
			fd_ = open(pathname, flags);
		}

		~fd_guard()
		{
			if (fd_ != -1)
			{
				close(fd_);
			}
		}

		int get_fd() const
		{
			return fd_;
		}

		fd_guard(const fd_guard& other) = delete;
		fd_guard& operator=(const fd_guard& other) = delete;

	private:
		int fd_;
	};
}

os_finder::size_filter::size_filter(size_t val, cmp cmp_mode) : val_(val), cmp_mode_(cmp_mode) { }

bool os_finder::size_filter::check(size_t sz) const noexcept
{
	switch (cmp_mode_)
	{
	case cmp::less:
		return sz < val_;
	case cmp::greater:
		return sz > val_;
	case cmp::eq:
		return sz == val_;
	}
	return false;
}

void os_finder::filter_inum(ino64_t inum)
{
	inums_.insert(inum);
}

void os_finder::filter_name(std::string name)
{
	names_.insert(std::move(name));
}

void os_finder::filter_size(size_filter size)
{
	sizes_.push_back(std::move(size));
}

void os_finder::filter_nlinks(nlink_t nlinks)
{
	nlinks_.insert(nlinks);
}

namespace fs = std::filesystem;

bool os_finder::visit_rec(std::vector<fs::path>& found, const fs::path& path, const callback_t& on_visit_file_failed)
{
	try {
		const auto fd = fd_guard(path.c_str(), O_RDONLY | O_DIRECTORY);
		if (fd.get_fd() == -1)
		{
			throw finder_exception(std::string("Could not open dir: ") + std::strerror(errno));
		}
		std::array<char, 4096> buf;
		for (auto nread = syscall(SYS_getdents64, fd.get_fd(), buf.data(), buf.size()); nread != 0; nread = syscall(SYS_getdents64, fd.get_fd(), buf.data(), buf.size())) {
			if (nread == -1)
			{
				throw finder_exception(std::string("Could not read dir: ") + std::strerror(errno));
			}

			for (auto ptr = buf.data(); ptr < buf.data() + nread; ptr += reinterpret_cast<linux_dirent64*>(ptr)->d_reclen) {
				const auto entry = reinterpret_cast<linux_dirent64*>(ptr);
				const auto name = std::string(entry->d_name);
				if (name == "." || name == "..") {
					continue;
				}
				auto cur = path / name;

				if (entry->d_type == DT_REG) {
					if (!inums_.empty() && inums_.find(entry->d_ino) == inums_.end())
					{
						continue;
					}
					if (!names_.empty() && names_.find(name) == names_.end())
					{
						continue;
					}
					if (!sizes_.empty() || !nlinks_.empty()) {
						struct stat stats {};
						if (fstat(fd.get_fd(), &stats) == -1) {
							throw finder_exception(std::string("Could not stat file: ") + std::strerror(errno));
						}
						if (!std::all_of(sizes_.begin(), sizes_.end(), [sz = stats.st_size](const auto& filter)
						{
							return filter.check(sz);
						}))
						{
							continue;
						};
						if (!nlinks_.empty() && nlinks_.find(stats.st_nlink) == nlinks_.end())
						{
							continue;
						}
					}

					found.push_back(std::move(cur));
				}
				else if (entry->d_type == DT_DIR) {
					if (!visit_rec(found, cur, on_visit_file_failed))
					{
						return false;
					}
				}
			}
		}
	} catch (const std::exception& ex)
	{
		return on_visit_file_failed(path, ex);
	}
	return true;
}

std::vector<fs::path> os_finder::visit(const fs::path& path, const callback_t& on_visit_file_failed)
{
	std::vector<fs::path> found;
	visit_rec(found, path, on_visit_file_failed);
	return found;
}