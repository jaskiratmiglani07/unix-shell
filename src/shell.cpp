#include "shell.hpp"
#include "parser.hpp"
#include "executor.hpp"
#include "common.hpp"
#include "jobs.hpp"
#include "history.hpp"

#include <iostream>
#include <unistd.h>
#include <climits>
#include <cstdlib>

#include "signals.hpp"

namespace aegissh {

Shell::Shell() {
    interactive_ = (isatty(STDIN_FILENO) != 0);
    SignalHandler::init_shell_signals();
    
    // Load history from file
    History::instance().load_from_file(History::instance().default_history_file());
}

Shell::~Shell() {
    // Save history on exit
    History::instance().save_to_file(History::instance().default_history_file());
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
        // Reap any background jobs that finished and print status notifications
        JobManager::instance().reap_background_jobs();
        JobManager::instance().print_completed_jobs();

        print_prompt();

        if (!std::getline(std::cin, line)) {
            // Check for SIGINT first - on macOS, signal interruption during getline
            // on a FIFO can set eofbit instead of failbit.
            bool sigint_received = SignalHandler::check_and_clear_sigint();
            
            if (std::cin.eof() && !sigint_received) {
                // Ctrl+D / real EOF (and not a spurious EOF from signal interruption)
                if (interactive_) {
                    std::cout << "\n";
                }
                break;
            }
            // getline was interrupted by a signal, or spurious EOF from signal.
            if (sigint_received && interactive_) {
                std::cout << "\n" << std::flush;
            }
            // Clear error state and restart the read loop.
            std::cin.clear();
            continue;
        }

        Pipeline pipeline = Parser::parse_line(line, last_status_);
        if (pipeline.empty()) {
            continue;
        }

        // Add to history (only non-empty, non-history commands)
        if (!line.empty() && !is_history_command(line)) {
            History::instance().add(line);
        }

        bool should_exit = false;
        last_status_ = Executor::execute_pipeline(pipeline, last_status_, should_exit);

        if (should_exit) {
            running_ = false;
            break;
        }
    }

    return last_status_;
}

bool Shell::is_history_command(const std::string& line) {
    // Check if the command is a history builtin (after parsing)
    // Simple check: trim and see if it starts with "history"
    size_t first = line.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return false;
    return line.compare(first, 7, "history") == 0 && 
           (first + 7 >= line.size() || std::isspace(static_cast<unsigned char>(line[first + 7])));
}

} // namespace aegissh