#include "parser.hpp"
#include "expander.hpp"
#include "common.hpp"
#include <cctype>

namespace aegissh {

Command Parser::parse_command(const std::string& cmd_str, int last_status) {
    Command cmd;
    
    // First tokenize respecting quotes and expanding variables using Expander
    std::vector<std::string> tokens = Expander::expand_and_split(cmd_str, last_status);
    
    // Then process redirections
    for (size_t i = 0; i < tokens.size(); ++i) {
        const std::string& tok = tokens[i];
        
        if (tok == "<" || tok == ">" || tok == ">>" || tok == "2>" || tok == "2>>") {
            if (i + 1 >= tokens.size()) {
                print_error("syntax error", "near unexpected token 'newline'");
                return Command{};
            }
            // Expand variables in redirection target filename (already expanded, but do it again for safety)
            std::string target = Expander::expand_vars(tokens[++i], last_status);
            
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

Pipeline Parser::parse_line(const std::string& line, int last_status) {
    Pipeline pipeline;
    std::string cleaned = line;

    // Strip comments first (but not inside quotes)
    std::string no_comments;
    bool in_single_quote = false;
    bool in_double_quote = false;
    for (size_t i = 0; i < cleaned.size(); ++i) {
        char c = cleaned[i];
        if (!in_double_quote && c == '\'') {
            in_single_quote = !in_single_quote;
        } else if (!in_single_quote && c == '"') {
            in_double_quote = !in_double_quote;
        } else if (c == '#' && !in_single_quote && !in_double_quote) {
            break;  // Rest of line is comment
        }
        no_comments.push_back(c);
    }
    cleaned = no_comments;

    // Trim trailing/leading whitespace
    size_t first = cleaned.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return pipeline; // empty
    }
    
    // Check for trailing '&' background operator (not inside quotes)
    in_single_quote = false;
    in_double_quote = false;
    for (size_t i = cleaned.size(); i-- > 0; ) {
        char c = cleaned[i];
        if (!in_double_quote && c == '\'') {
            in_single_quote = !in_single_quote;
        } else if (!in_single_quote && c == '"') {
            in_double_quote = !in_double_quote;
        } else if (std::isspace(static_cast<unsigned char>(c)) && !in_single_quote && !in_double_quote) {
            // whitespace, continue
        } else if (!in_single_quote && !in_double_quote && c == '&') {
            pipeline.background = true;
            cleaned = cleaned.substr(0, i);
            // Trim again after removing '&'
            size_t new_last = cleaned.find_last_not_of(" \t\r\n");
            if (new_last != std::string::npos) {
                cleaned = cleaned.substr(0, new_last + 1);
            } else {
                cleaned.clear();
            }
            break;
        } else {
            break;
        }
    }

    // Split by pipe character '|' (not inside quotes)
    std::vector<std::string> stage_strings;
    std::string current_stage;
    in_single_quote = false;
    in_double_quote = false;

    for (size_t i = 0; i < cleaned.size(); ++i) {
        char ch = cleaned[i];
        
        if (!in_double_quote && ch == '\'') {
            in_single_quote = !in_single_quote;
            current_stage.push_back(ch);
        } else if (!in_single_quote && ch == '"') {
            in_double_quote = !in_double_quote;
            current_stage.push_back(ch);
        } else if (ch == '|' && !in_single_quote && !in_double_quote) {
            // Check for empty stage
            bool only_ws = true;
            for (char c : current_stage) {
                if (!std::isspace(static_cast<unsigned char>(c))) {
                    only_ws = false;
                    break;
                }
            }
            if (only_ws) {
                print_error("syntax error", "near unexpected token '|'");
                return Pipeline{};
            }
            stage_strings.push_back(current_stage);
            current_stage.clear();
            in_single_quote = false;
            in_double_quote = false;
        } else {
            current_stage.push_back(ch);
        }
    }

    // Check final stage
    bool only_ws = true;
    for (char c : current_stage) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            only_ws = false;
            break;
        }
    }
    if (only_ws) {
        if (!stage_strings.empty()) {
            print_error("syntax error", "near unexpected token '|'");
            return Pipeline{};
        }
    } else {
        stage_strings.push_back(current_stage);
    }

    for (const auto& stage_str : stage_strings) {
        Command cmd = parse_command(stage_str, last_status);
        if (cmd.empty()) {
            return Pipeline{};
        }
        pipeline.commands.push_back(std::move(cmd));
    }

    return pipeline;
}

} // namespace aegissh