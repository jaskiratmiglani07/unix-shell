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
    static int builtin_cd(const std::vector<std::string>& args);
    static int builtin_echo(const std::vector<std::string>& args);
    static int builtin_env(const std::vector<std::string>& args);
    static int builtin_export(const std::vector<std::string>& args);
    static int builtin_unset(const std::vector<std::string>& args);

    static bool is_valid_identifier(const std::string& name);
};

} // namespace aegissh
