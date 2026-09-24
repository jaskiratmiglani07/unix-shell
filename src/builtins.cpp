#include "builtins.hpp"
#include "common.hpp"
#include <unistd.h>
#include <climits>
#include <cstdlib>

namespace aegissh {

bool Builtins::is_builtin(const std::string& name) {
    return name == "pwd" || name == "exit";
}

int Builtins::execute(const std::vector<std::string>& args, int last_status, bool& should_exit) {
    if (args.empty()) return 0;
    const std::string& cmd = args[0];

    if (cmd == "pwd") {
        return builtin_pwd(args);
    } else if (cmd == "exit") {
        return builtin_exit(args, last_status, should_exit);
    }

    return 1;
}

int Builtins::builtin_pwd(const std::vector<std::string>& /*args*/) {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        std::cout << cwd << "\n";
        return 0;
    } else {
        print_sys_error("pwd");
        return 1;
    }
}

int Builtins::builtin_exit(const std::vector<std::string>& args, int last_status, bool& should_exit) {
    should_exit = true;
    if (args.size() == 1) {
        return last_status;
    }

    // Parse optional status
    char* endptr = nullptr;
    long val = std::strtol(args[1].c_str(), &endptr, 10);
    if (*endptr != '\0') {
        print_error("exit", "numeric argument required: " + args[1]);
        return 2;
    }

    return static_cast<int>(val & 0xFF);
}

} // namespace aegissh
