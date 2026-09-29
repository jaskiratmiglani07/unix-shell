#include "expander.hpp"
#include <cstdlib>
#include <cctype>

namespace aegissh {

static std::string get_env(const std::string& name, int last_status) {
    if (name == "?") {
        return std::to_string(last_status);
    }
    const char* val = std::getenv(name.c_str());
    return val ? val : "";
}

static bool is_var_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

// Expand variables in a string.
// in_single_quote[i] = true means character i is inside single quotes (no expansion, no backslash processing)
// is_escaped[i] = true means character i resulted from an escape sequence (no expansion for $)
static std::string expand_with_tracking(const std::string& token, int last_status, 
                                         const std::vector<bool>& in_single_quote,
                                         const std::vector<bool>& is_escaped) {
    std::string result;
    result.reserve(token.size() * 2);

    for (size_t i = 0; i < token.size(); ++i) {
        char c = token[i];
        bool in_sq = in_single_quote[i];
        bool escaped = is_escaped[i];
        
        // Skip backslash processing - already handled during tokenization
        // Only handle variable expansion here
        
        if (c == '$' && !in_sq && !escaped) {
            if (i + 1 < token.size()) {
                char next = token[i + 1];
                // Check if next char is also escaped (part of same escape sequence)
                bool next_escaped = is_escaped[i + 1];
                bool next_in_sq = in_single_quote[i + 1];
                
                if (next == '?' && !next_escaped && !next_in_sq) {
                    result += get_env("?", last_status);
                    ++i;
                } else if (next == '{' && !next_escaped && !next_in_sq) {
                    size_t brace_end = token.find('}', i + 2);
                    if (brace_end != std::string::npos) {
                        std::string var_name = token.substr(i + 2, brace_end - i - 2);
                        result += get_env(var_name, last_status);
                        i = brace_end;
                    } else {
                        result.push_back(c);
                    }
                } else if (is_var_char(next) && !next_escaped && !next_in_sq) {
                    // $VAR syntax - parse variable name, stopping at quote/escape boundaries
                    size_t j = i + 1;
                    bool initial_quote_state = in_single_quote[j];
                    bool initial_escaped_state = is_escaped[j];
                    while (j < token.size() && is_var_char(token[j]) 
                           && in_single_quote[j] == initial_quote_state
                           && is_escaped[j] == initial_escaped_state) {
                        ++j;
                    }
                    std::string var_name = token.substr(i + 1, j - i - 1);
                    result += get_env(var_name, last_status);
                    i = j - 1;
                } else {
                    result.push_back(c);
                }
            } else {
                result.push_back(c);
            }
        } else {
            result.push_back(c);
        }
    }
    return result;
}

std::string Expander::expand_vars(const std::string& token, int last_status) {
    // For redirection filenames - simple expansion without quote tracking
    std::vector<bool> dummy_sq(token.size(), false);
    std::vector<bool> dummy_esc(token.size(), false);
    return expand_with_tracking(token, last_status, dummy_sq, dummy_esc);
}

std::vector<std::string> Expander::expand_and_split(const std::string& input, int last_status) {
    std::vector<std::string> result;
    std::string current_token;
    std::vector<bool> current_quote_state; // true = in single quote for each char
    std::vector<bool> current_escaped_state; // true = char resulted from escape
    bool in_single_quote = false;
    bool in_double_quote = false;
    
    for (size_t i = 0; i < input.size(); ++i) {
        char c = input[i];
        
        if (!in_double_quote && c == '\'') {
            in_single_quote = !in_single_quote;
            continue; // Don't include quote chars in token
        }
        
        if (!in_single_quote && c == '"') {
            in_double_quote = !in_double_quote;
            continue; // Don't include quote chars in token
        }
        
        // Handle backslash escapes - ONLY outside single quotes
        if (c == '\\' && i + 1 < input.size()) {
            char next = input[i + 1];
            if (in_single_quote) {
                // Inside single quotes: backslash is LITERAL, include both chars
                current_token.push_back(c);
                current_quote_state.push_back(true);
                current_escaped_state.push_back(false);
                current_token.push_back(next);
                current_quote_state.push_back(true);
                current_escaped_state.push_back(false);
            } else if (in_double_quote) {
                // Inside double quotes: backslash escapes only $, `, ", \, newline
                if (next == '$' || next == '`' || next == '"' || next == '\\' || next == '\n') {
                    // Escape is consumed - only the escaped char goes to token, marked as escaped
                    current_token.push_back(next);
                    current_quote_state.push_back(false);
                    current_escaped_state.push_back(true); // This char was escaped
                } else {
                    // Not an escapable char in double quotes - keep both literal
                    current_token.push_back(c);
                    current_quote_state.push_back(false);
                    current_escaped_state.push_back(false);
                    current_token.push_back(next);
                    current_quote_state.push_back(false);
                    current_escaped_state.push_back(false);
                }
            } else {
                // Outside quotes: backslash escapes next character
                current_token.push_back(next);
                current_quote_state.push_back(false);
                current_escaped_state.push_back(true); // Escaped char
            }
            ++i;
            continue;
        }
        
        if (std::isspace(static_cast<unsigned char>(c)) && !in_single_quote && !in_double_quote) {
            // Word boundary - finalize current token
            if (!current_token.empty()) {
                std::string expanded = expand_with_tracking(current_token, last_status, 
                                                            current_quote_state, current_escaped_state);
                result.push_back(expanded);
                current_token.clear();
                current_quote_state.clear();
                current_escaped_state.clear();
            }
        } else {
            current_token.push_back(c);
            current_quote_state.push_back(in_single_quote);
            current_escaped_state.push_back(false);
        }
    }
    
    // Final token
    if (!current_token.empty()) {
        std::string expanded = expand_with_tracking(current_token, last_status, 
                                                    current_quote_state, current_escaped_state);
        result.push_back(expanded);
    }
    
    // If we have empty result but were in quotes (e.g., "" or ''), return empty string
    if (result.empty() && (in_single_quote || in_double_quote)) {
        result.push_back("");
    }
    
    return result;
}

} // namespace aegissh