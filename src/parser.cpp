#include "parser.hpp"
#include "common.hpp"
#include <cctype>

namespace aegissh {

Command Parser::parse_command(const std::string& cmd_str) {
    Command cmd;
    std::vector<std::string> raw_tokens;
    std::string current;
    bool in_token = false;

    for (size_t i = 0; i < cmd_str.length(); ++i) {
        char ch = cmd_str[i];

        if (ch == '#' && !in_token) {
            break;
        }

        if (!in_token || std::isspace(static_cast<unsigned char>(ch)) ||
            ch == '<' || ch == '>' || (ch == '2' && i + 1 < cmd_str.length() && cmd_str[i + 1] == '>')) {

            if (ch == '2' && i + 1 < cmd_str.length() && cmd_str[i + 1] == '>') {
                if (in_token) {
                    raw_tokens.push_back(current);
                    current.clear();
                    in_token = false;
                }
                if (i + 2 < cmd_str.length() && cmd_str[i + 2] == '>') {
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
                if (i + 1 < cmd_str.length() && cmd_str[i + 1] == '>') {
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

Pipeline Parser::parse_line(const std::string& line) {
    Pipeline pipeline;
    std::string cleaned = line;

    // Strip comments first
    size_t comment_pos = cleaned.find('#');
    if (comment_pos != std::string::npos) {
        cleaned = cleaned.substr(0, comment_pos);
    }

    // Trim trailing/leading whitespace
    size_t first = cleaned.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return pipeline; // empty
    }
    // Check for trailing '&' background operator
    if (!cleaned.empty() && cleaned.back() == '&') {
        pipeline.background = true;
        cleaned.pop_back();
        // Trim again after removing '&'
        size_t last = cleaned.find_last_not_of(" \t\r\n");
        if (last == std::string::npos) {
            return pipeline;
        }
        cleaned = cleaned.substr(0, last + 1);
    }

    // Split by pipe character '|'
    std::vector<std::string> stage_strings;
    std::string current_stage;

    for (size_t i = 0; i < cleaned.length(); ++i) {
        char ch = cleaned[i];
        if (ch == '|') {
            if (current_stage.find_first_not_of(" \t\r\n") == std::string::npos) {
                print_error("syntax error", "near unexpected token '|'");
                return Pipeline{};
            }
            stage_strings.push_back(current_stage);
            current_stage.clear();
        } else {
            current_stage.push_back(ch);
        }
    }

    if (current_stage.find_first_not_of(" \t\r\n") == std::string::npos) {
        if (!stage_strings.empty()) {
            print_error("syntax error", "near unexpected token '|'");
            return Pipeline{};
        }
    } else {
        stage_strings.push_back(current_stage);
    }

    for (const auto& stage_str : stage_strings) {
        Command cmd = parse_command(stage_str);
        if (cmd.empty()) {
            return Pipeline{};
        }
        pipeline.commands.push_back(std::move(cmd));
    }

    return pipeline;
}

} // namespace aegissh
