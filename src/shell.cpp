#include "shell.hpp"
#include "parser.hpp"
#include "executor.hpp"
#include "common.hpp"

#include <iostream>
#include <unistd.h>
#include <climits>
#include <cstdlib>

namespace aegissh {

Shell::Shell() {
    interactive_ = (isatty(STDIN_FILENO) != 0);
}

std::string Shell::get_formatted_cwd() {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == nullptr) {
        return "?";
    }

    std::string s_cwd(cwd);
    const char* home = std::getenv("HOME");
    if (home != nullptr && *home != '\0') {
        std::string s_home(home);
        if (s_cwd == s_home) {
            return "~";
        } else if (s_cwd.rfind(s_home + "/", 0) == 0) {
            return "~" + s_cwd.substr(s_home.length());
        }
    }
    return s_cwd;
}

void Shell::print_prompt() {
    if (!interactive_) return;
    std::cout << "\033[1;36maegissh\033[0m:\033[1;34m" << get_formatted_cwd() << "\033[0m$ " << std::flush;
}

int Shell::run() {
    std::string line;

    while (running_) {
        print_prompt();

        if (!std::getline(std::cin, line)) {
            // EOF encountered (Ctrl+D)
            if (interactive_) {
                std::cout << "\n";
            }
            break;
        }

        Command cmd = Parser::parse_line(line);
        if (cmd.empty()) {
            continue;
        }

        bool should_exit = false;
        last_status_ = Executor::execute(cmd, last_status_, should_exit);

        if (should_exit) {
            running_ = false;
            break;
        }
    }

    return last_status_;
}

} // namespace aegissh
