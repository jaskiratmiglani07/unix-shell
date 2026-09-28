#pragma once

#include <signal.h>

namespace aegissh {

class SignalHandler {
public:
    // Configures signal dispositions for the interactive shell process.
    static void init_shell_signals();

    // Restores default signal dispositions in child processes prior to execvp.
    static void reset_child_signals();

    // Queries and clears pending signal notifications
    static bool check_and_clear_sigint();
    static bool check_and_clear_sigchld();
};

} // namespace aegissh
