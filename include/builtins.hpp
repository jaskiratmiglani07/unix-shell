#pragma once

#include <string>
#include <vector>

namespace aegissh {

class Builtins {
public:
    static bool is_builtin(const std::string& name);
    static int execute(const std::vector<std::string>& args, int last_status, bool& should_exit);

private:
    static int builtin_pwd(const std::vector<std::string>& args);
    static int builtin_exit(const std::vector<std::string>& args, int last_status, bool& should_exit);
};

} // namespace aegissh
