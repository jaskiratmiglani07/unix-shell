#include "parser.hpp"
#include "common.hpp"
#include <cctype>

namespace aegissh {

Command Parser::parse_line(const std::string& line) {
    Command cmd;
    std::vector<std::string> raw_tokens;
    std::string current;
    bool in_token = false;

    // Phase 3 Lexer: Extract tokens, separating redirection operators
    for (size_t i = 0; i < line.length(); ++i) {
        char ch = line[i];

        // Strip comments
        if (ch == '#' && !in_token) {
            break;
        }

        // Check for redirection operators: 2>>, 2>, >>, >, <
        if (!in_token || std::isspace(static_cast<unsigned char>(ch)) ||
            ch == '<' || ch == '>' || (ch == '2' && i + 1 < line.length() && line[i + 1] == '>')) {

            if (ch == '2' && i + 1 < line.length() && line[i + 1] == '>') {
                if (in_token) {
                    raw_tokens.push_back(current);
                    current.clear();
                    in_token = false;
                }
                if (i + 2 < line.length() && line[i + 2] == '>') {
                    raw_tokens.push_back("2>>");
                    i += 2;
                } else {
                    raw_tokens.push_back("2>");
                    i += 1;
                }
                continue;
            } else if (ch == '>') {
                if (in_token) {
                    raw_tokens.push_back(current);
                    current.clear();
                    in_token = false;
                }
                if (i + 1 < line.length() && line[i + 1] == '>') {
                    raw_tokens.push_back(">>");
                    i += 1;
                } else {
                    raw_tokens.push_back(">");
                }
                continue;
            } else if (ch == '<') {
                if (in_token) {
                    raw_tokens.push_back(current);
                    current.clear();
                    in_token = false;
                }
                raw_tokens.push_back("<");
                continue;
            }
        }

        if (std::isspace(static_cast<unsigned char>(ch))) {
            if (in_token) {
                raw_tokens.push_back(current);
                current.clear();
                in_token = false;
            }
        } else {
            current.push_back(ch);
            in_token = true;
        }
    }

    if (in_token) {
        raw_tokens.push_back(current);
    }

    // Process raw tokens to extract arguments and redirections
    for (size_t i = 0; i < raw_tokens.size(); ++i) {
        const std::string& tok = raw_tokens[i];

        if (tok == "<" || tok == ">" || tok == ">>" || tok == "2>" || tok == "2>>") {
            if (i + 1 >= raw_tokens.size()) {
                print_error("syntax error", "near unexpected token 'newline'");
                return Command{};
            }
            const std::string& target = raw_tokens[++i];

            if (tok == "<") {
                cmd.redirections.push_back({Redirection::Type::INPUT, target});
            } else if (tok == ">") {
                cmd.redirections.push_back({Redirection::Type::OUTPUT_TRUNC, target});
            } else if (tok == ">>") {
                cmd.redirections.push_back({Redirection::Type::OUTPUT_APPEND, target});
            } else if (tok == "2>") {
                cmd.redirections.push_back({Redirection::Type::ERR_TRUNC, target});
            } else if (tok == "2>>") {
                cmd.redirections.push_back({Redirection::Type::ERR_APPEND, target});
            }
        } else {
            cmd.args.push_back(tok);
        }
    }

    return cmd;
}

} // namespace aegissh
