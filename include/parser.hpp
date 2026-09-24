#pragma once

#include <string>
#include <vector>

namespace aegissh {

struct Redirection {
    enum class Type {
        INPUT,          // <
        OUTPUT_TRUNC,   // >
        OUTPUT_APPEND,  // >>
        ERR_TRUNC,      // 2>
        ERR_APPEND      // 2>>
    };
    Type type;
    std::string filename;
};

struct Command {
    std::vector<std::string> args;
    std::vector<Redirection> redirections;

    bool empty() const {
        return args.empty() && redirections.empty();
    }
};

class Parser {
public:
    static Command parse_line(const std::string& line);
};

} // namespace aegissh
