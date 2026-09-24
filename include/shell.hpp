#pragma once

#include <string>

namespace aegissh {

class Shell {
public:
    Shell();
    int run();

private:
    void print_prompt();
    std::string get_formatted_cwd();

    bool interactive_{false};
    bool running_{true};
    int last_status_{0};
};

} // namespace aegissh
