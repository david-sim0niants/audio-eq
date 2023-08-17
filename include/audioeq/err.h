#pragma once

#include <stdexcept>
#include <cstring>

namespace aeq {

struct AudioEqErr : std::runtime_error {
	AudioEqErr() : std::runtime_error("Unknown error.") {}
	AudioEqErr(const std::string& errmsg) : std::runtime_error(errmsg) {}
	AudioEqErr(const std::string& errmsg, int errnum)
		: std::runtime_error(errmsg + ": " + strerror(errnum)) {}
};

}
