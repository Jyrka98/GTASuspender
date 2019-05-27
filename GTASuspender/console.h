#pragma once

#include <iostream>
#include <string>
#include <boost/format.hpp>
#include <termcolor/termcolor.hpp>

class console final {
public:
	template <typename... Arguments>
	static void print_info(const std::string& fmt, const Arguments& ... args) {
		const std::string formatted = boost::str((boost::format(fmt) % ... % args));
		std::cout << termcolor::white << formatted << std::endl;
	}

	template <typename... Arguments>
	static void print_warning(const std::string& fmt, const Arguments& ... args) {
		const std::string formatted = boost::str((boost::format(fmt) % ... % args));
		std::cout << termcolor::yellow << formatted << std::endl;
	}

	template <typename... Arguments>
	static void print_error(const std::string& fmt, const Arguments& ... args) {
		const std::string formatted = boost::str((boost::format(fmt) % ... % args));
		std::cout << termcolor::red << formatted << std::endl;
	}

	template <typename... Arguments>
	static void print_debug(const std::string& fmt, const Arguments& ... args) {
#if defined(_DEBUG)
		const std::string formatted = boost::str((boost::format(fmt) % ... % args));
		std::cout << termcolor::cyan << formatted << std::endl;
#endif
	}
};
