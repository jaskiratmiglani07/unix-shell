#pragma once

#include "parser.hpp"

namespace aegissh {

class Executor {
public:
    static int execute_pipeline(const Pipeline& pipeline, int last_status, bool& should_exit);
    static int execute_command(const Command& cmd, int last_status, bool& should_exit);
};

} // namespace aegissh
