#include <cstdio>
#include <iostream>
#include <regex>
#include <string>
#include <cstring>
#include <cerrno>
#include <charconv>
#include <filesystem>

#include <boost/program_options.hpp>

#include <unistd.h>

#include "os_finder.h"

void validate(boost::any& v, const std::vector<std::string>& values, os_finder::size_filter* target_type, int)
{
	static std::regex r("[-+=]\\d+");

	using namespace boost::program_options;

	validators::check_first_occurrence(v);
	const std::string& s = validators::get_single_string(values);

	std::smatch match;
	if (std::regex_match(s, match, r)) {
		std::string arg = match[0];
		os_finder::size_filter::cmp cmp_mode;
		switch (arg[0])
		{
		case '-':
			cmp_mode = os_finder::size_filter::cmp::less;
			break;
		case '+':
			cmp_mode = os_finder::size_filter::cmp::greater;
			break;
		case '=':
			cmp_mode = os_finder::size_filter::cmp::eq;
			break;
		}
		size_t val;
		std::from_chars(arg.data() + 1, arg.data() + arg.size(), val);
		v = boost::any(os_finder::size_filter(val, cmp_mode));
	}
	else {
		throw validation_error(validation_error::invalid_option_value);
	}
}

int main(int argc, char* argv[])
{
	namespace po = boost::program_options;
	auto desc = po::options_description("Allowed options");
	desc.add_options()
		("help", "help message")
		("path", po::value<std::filesystem::path>(), "path")
		("inum", po::value<std::vector<ino64_t>>(), "inode number")
		("name", po::value<std::vector<std::string>>(), "file name")
		("size", po::value<std::vector<os_finder::size_filter>>(), "size filter")
		("nlinks", po::value<std::vector<nlink_t>>(), "file hard links count")
		("exec", po::value<std::vector<std::filesystem::path>>(), "file to execute")
	;
	po::positional_options_description p;
	p.add("path", -1);
	po::variables_map vm;
	try {
		po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);
	} catch (const po::error& ex)
	{
		std::cout << ex.what() << std::endl;
		return EXIT_FAILURE;
	}
	po::notify(vm);

	if (vm.count("help") || !vm.count("path")) {
		std::cout << desc << std::endl;
		return EXIT_SUCCESS;
	}

	auto finder = os_finder();
	if (vm.count("inum"))
	{
		for (const auto& inum : vm["inum"].as<std::vector<ino64_t>>())
		{
			finder.filter_inum(inum);
		}
	}
	if (vm.count("name"))
	{
		for (const auto& name : vm["name"].as<std::vector<std::string>>())
		{
			finder.filter_name(name);
		}
	}
	if (vm.count("size"))
	{
		for (const auto& size_filter : vm["size"].as<std::vector<os_finder::size_filter>>())
		{
			finder.filter_size(size_filter);
		}
	}
	if (vm.count("nlinks"))
	{
		for (const auto& nlinks : vm["nlinks"].as<std::vector<nlink_t>>())
		{
			finder.filter_nlinks(nlinks);
		}
	}

	std::vector<std::filesystem::path> found;
	try
	{
		found = finder.visit(vm["path"].as<std::filesystem::path>());
	} catch (const os_finder::finder_exception& ex)
	{
		std::cout << ex.what() << std::endl;
		return EXIT_FAILURE;
	}

	if (vm.count("exec"))
	{
		for (const auto& exec : vm["exec"].as<std::vector<std::filesystem::path>>())
		{
			std::vector<char*> found_c_strs;
			found_c_strs.reserve(found.size() + 2);
			found_c_strs.push_back(const_cast<char*>(exec.c_str()));
			for (auto const& path : found) {
				found_c_strs.push_back(const_cast<char*>(path.c_str()));
			}
			found_c_strs.push_back(nullptr);

			if (execv(found_c_strs[0], found_c_strs.data()) == -1) {
				std::cout << "could not execute " << exec << ": " << std::strerror(errno) << std::endl;
				return EXIT_FAILURE;
			}
		}
	} else
	{
		for (const auto& path : found)
		{
			std::cout << path << std::endl;
		}
	}

	return EXIT_SUCCESS;
}
