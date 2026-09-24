#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <memory>
#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

namespace aegissh {

inline void print_error(const std::string& prefix, const std::string& msg) {
    std::cerr << "aegissh: " << prefix << (prefix.empty() ? "" : ": ") << msg << std::endl;
}

inline void print_sys_error(const std::string& prefix) {
    print_error(prefix, std::strerror(errno));
}

} // namespace aegissh
