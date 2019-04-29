#ifndef OS_FINDER_H
#define OS_FINDER_H

#include <set>
#include <vector>
#include <string>
#include <stdexcept>
#include <cerrno>
#include <cstring>
#include <filesystem>

#include <sys/types.h>

struct os_finder
{
	struct finder_exception : std::runtime_error
	{
		using std::runtime_error::runtime_error;
	};

	struct size_filter
	{
		enum class cmp
		{
			less,
			greater,
			eq
		};

		size_filter(size_t val, cmp cmp_mode);

		bool check(size_t sz) const noexcept;

	private:
		size_t val_;
		cmp cmp_mode_;
	};

	std::vector<std::filesystem::path> visit(const std::filesystem::path& path);
	void filter_inum(ino64_t inum);
	void filter_name(std::string name);
	void filter_size(size_filter size);
	void filter_nlinks(nlink_t nlinks);

private:
	void visit_rec(std::vector<std::filesystem::path>& found, const std::filesystem::path& path);

	std::set<ino64_t> inums_{};
	std::set<std::string> names_{};
	std::vector<size_filter> sizes_{};
	std::set<nlink_t> nlinks_{};
};

#endif // OS_FINDER_H