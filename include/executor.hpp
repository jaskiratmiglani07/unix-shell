#pragma once

#include "parser.hpp"

namespace aegissh {

class Executor {
public:
    // Executes a single command. Returns the exit status.
    // Sets should_exit to true if an exit builtin was executed.
    static int execute(const Command& cmd, int last_status, bool& should_exit);
};

} // namespace aegissh
