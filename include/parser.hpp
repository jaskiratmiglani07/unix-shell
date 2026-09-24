#pragma once

#include <string>
#include <vector>

namespace aegissh {

struct Command {
    std::vector<std::string> args;
    std::string in_file;
    std::string out_file;
    bool out_append{false};
    std::string err_file;
    bool err_append{false};

    bool empty() const {
        return args.empty();
    }
};

class Parser {
public:
    // Parses a single command line for Phase 1.
    // Splits by whitespace while preserving leading/trailing cleanup.
    // (Will be extended in Phase 4 for pipes and Phase 7 for quotes/expansions)
    static Command parse_line(const std::string& line);
};

} // namespace aegissh
