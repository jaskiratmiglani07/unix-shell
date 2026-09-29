#pragma once

#include <string>

namespace aegissh {

class Shell {
public:
    Shell();
    ~Shell();
    int run();

private:
    void print_prompt();
    std::string get_formatted_cwd();
    bool is_history_command(const std::string& line);

    bool interactive_{false};
    bool running_{true};
    int last_status_{0};
};

} // namespace aegissh