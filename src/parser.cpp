#include "parser.hpp"
#include <sstream>
#include <cctype>

namespace aegissh {

Command Parser::parse_line(const std::string& line) {
    Command cmd;
    std::string current;
    bool in_token = false;

    for (size_t i = 0; i < line.length(); ++i) {
        char ch = line[i];

        // Handle basic comment stripping
        if (ch == '#' && !in_token) {
            break; // Rest of line is comment
        }

        if (std::isspace(static_cast<unsigned char>(ch))) {
            if (in_token) {
                cmd.args.push_back(current);
                current.clear();
                in_token = false;
            }
        } else {
            current.push_back(ch);
            in_token = true;
        }
    }

    if (in_token) {
        cmd.args.push_back(current);
    }

    return cmd;
}

} // namespace aegissh
