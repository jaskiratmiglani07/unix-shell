#pragma once

#include <string>
#include <vector>

namespace aegissh {

// Expander performs word-splitting with:
//   - single-quote literals (no expansion inside '')
//   - double-quote grouping (variable expansion inside "")
//   - backslash escaping
//   - $VAR and ${VAR} expansion
//   - $? expansion
class Expander {
public:
    // Expand and split a raw token string into a list of words.
    // last_status is used for $? expansion.
    static std::vector<std::string> expand_and_split(const std::string& token,
                                                      int last_status);

    // Expand variable references inside a single already-parsed token
    // (no word splitting). Used for filenames in redirections.
    static std::string expand_vars(const std::string& token, int last_status);
};

} // namespace aegissh
