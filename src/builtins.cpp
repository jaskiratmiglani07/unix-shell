#include "builtins.hpp"
#include "history.hpp"
#include "jobs.hpp"
#include "common.hpp"
#include <unistd.h>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <iomanip>

extern char **environ;

namespace aegissh {

bool Builtins::is_valid_identifier(const std::string& name) {
    if (name.empty()) return false;
    if (!std::isalpha(static_cast<unsigned char>(name[0])) && name[0] != '_') {
        return false;
    }
    for (size_t i = 1; i < name.length(); ++i) {
        if (!std::isalnum(static_cast<unsigned char>(name[i])) && name[i] != '_') {
            return false;
        }
    }
    return true;
}

bool Builtins::is_builtin(const std::string& name) {
    return name == "pwd" || name == "exit" || name == "cd" ||
           name == "echo" || name == "env" || name == "export" ||
           name == "unset" || name == "history" ||
           name == "fg" || name == "bg" || name == "jobs";
}

int Builtins::execute(const std::vector<std::string>& args, int last_status, bool& should_exit) {
    if (args.empty()) return 0;
    const std::string& cmd = args[0];

    if (cmd == "pwd") {
        return builtin_pwd(args);
    } else if (cmd == "exit") {
        return builtin_exit(args, last_status, should_exit);
    } else if (cmd == "cd") {
        return builtin_cd(args);
    } else if (cmd == "echo") {
        return builtin_echo(args);
    } else if (cmd == "env") {
        return builtin_env(args);
    } else if (cmd == "export") {
        return builtin_export(args);
    } else if (cmd == "unset") {
        return builtin_unset(args);
    } else if (cmd == "history") {
        return builtin_history(args);
    } else if (cmd == "fg") {
        return builtin_fg(args);
    } else if (cmd == "bg") {
        return builtin_bg(args);
    } else if (cmd == "jobs") {
        return builtin_jobs(args);
    }

    return 1;
}

int Builtins::builtin_pwd(const std::vector<std::string>& /*args*/) {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        std::cout << cwd << "\n";
        return 0;
    } else {
        print_sys_error("pwd");
        return 1;
    }
}

int Builtins::builtin_exit(const std::vector<std::string>& args, int last_status, bool& should_exit) {
    should_exit = true;
    if (args.size() == 1) {
        return last_status;
    }

    // Parse optional status
    char* endptr = nullptr;
    long val = std::strtol(args[1].c_str(), &endptr, 10);
    if (*endptr != '\0') {
        print_error("exit", "numeric argument required: " + args[1]);
        return 2;
    }

    return static_cast<int>(val & 0xFF);
}

int Builtins::builtin_cd(const std::vector<std::string>& args) {
    std::string target;
    bool print_after_cd = false;

    if (args.size() == 1) {
        const char* home = std::getenv("HOME");
        if (!home) {
            print_error("cd", "HOME not set");
            return 1;
        }
        target = home;
    } else if (args[1] == "-") {
        const char* oldpwd = std::getenv("OLDPWD");
        if (!oldpwd) {
            print_error("cd", "OLDPWD not set");
            return 1;
        }
        target = oldpwd;
        print_after_cd = true;
    } else {
        target = args[1];
    }

    char current_cwd[PATH_MAX];
    bool have_prev_cwd = (getcwd(current_cwd, sizeof(current_cwd)) != nullptr);

    if (chdir(target.c_str()) != 0) {
        print_sys_error("cd: " + target);
        return 1;
    }

    char new_cwd[PATH_MAX];
    if (getcwd(new_cwd, sizeof(new_cwd)) != nullptr) {
        if (have_prev_cwd) {
            setenv("OLDPWD", current_cwd, 1);
        }
        setenv("PWD", new_cwd, 1);
        if (print_after_cd) {
            std::cout << new_cwd << "\n";
        }
    }

    return 0;
}

int Builtins::builtin_echo(const std::vector<std::string>& args) {
    bool no_newline = false;
    size_t start_idx = 1;

    // Check for -n option
    if (args.size() > 1 && args[1] == "-n") {
        no_newline = true;
        start_idx = 2;
    }

    for (size_t i = start_idx; i < args.size(); ++i) {
        std::cout << args[i];
        if (i + 1 < args.size()) {
            std::cout << " ";
        }
    }

    if (!no_newline) {
        std::cout << "\n";
    }
    std::cout << std::flush;
    return 0;
}

int Builtins::builtin_env(const std::vector<std::string>& /*args*/) {
    if (!environ) return 0;
    for (char** env = environ; *env != nullptr; ++env) {
        std::cout << *env << "\n";
    }
    return 0;
}

int Builtins::builtin_export(const std::vector<std::string>& args) {
    if (args.size() == 1) {
        // List exported variables
        if (!environ) return 0;
        for (char** env = environ; *env != nullptr; ++env) {
            std::cout << "export " << *env << "\n";
        }
        return 0;
    }

    int rc = 0;
    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& arg = args[i];
        size_t eq_pos = arg.find('=');

        if (eq_pos != std::string::npos) {
            std::string key = arg.substr(0, eq_pos);
            std::string val = arg.substr(eq_pos + 1);

            if (!is_valid_identifier(key)) {
                print_error("export", "'" + arg + "': not a valid identifier");
                rc = 1;
                continue;
            }

            if (setenv(key.c_str(), val.c_str(), 1) != 0) {
                print_sys_error("export");
                rc = 1;
            }
        } else {
            if (!is_valid_identifier(arg)) {
                print_error("export", "'" + arg + "': not a valid identifier");
                rc = 1;
            }
        }
    }

    return rc;
}

int Builtins::builtin_unset(const std::vector<std::string>& args) {
    int rc = 0;
    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (!is_valid_identifier(arg)) {
            print_error("unset", "'" + arg + "': not a valid identifier");
            rc = 1;
            continue;
        }

        if (unsetenv(arg.c_str()) != 0) {
            print_sys_error("unset");
            rc = 1;
        }
    }
    return rc;
}

int Builtins::builtin_history(const std::vector<std::string>& args) {
    auto& hist = History::instance();
    const auto& all = hist.get_all();
    
    // Print history with right-aligned numbers (bash style)
    size_t total = all.size();
    size_t width = 1;
    size_t temp = total;
    while (temp >= 10) {
        width++;
        temp /= 10;
    }
    // Minimum width of 5 for alignment with bash
    if (width < 5) width = 5;
    
    size_t idx = 1;
    for (const auto& cmd : all) {
        std::cout << std::setw(width) << idx++ << "  " << cmd << "\n";
    }
    return 0;
}

int Builtins::builtin_jobs(const std::vector<std::string>& /*args*/) {
    auto& jm = JobManager::instance();
    const auto& jobs = jm.get_all_jobs();
    
    for (const auto& job : jobs) {
        std::string state_str;
        switch (job.state) {
            case JobState::RUNNING: state_str = "Running"; break;
            case JobState::STOPPED: state_str = "Stopped"; break;
            case JobState::DONE: state_str = "Done"; break;
        }
        std::cout << "[" << job.id << "]+  " << state_str << "                 " << job.cmdline << "\n";
    }
    return 0;
}

int Builtins::builtin_fg(const std::vector<std::string>& args) {
    auto& jm = JobManager::instance();
    
    int job_id = 0;
    if (args.size() > 1) {
        // Parse job ID from argument (e.g., %1 or 1)
        std::string arg = args[1];
        if (arg[0] == '%') arg = arg.substr(1);
        try {
            job_id = std::stoi(arg);
        } catch (...) {
            print_error("fg", "invalid job ID: " + args[1]);
            return 1;
        }
    } else {
        // Use current job (most recent)
        Job* job = jm.get_current_job();
        if (!job) {
            print_error("fg", "no current job");
            return 1;
        }
        job_id = job->id;
    }
    
    return jm.fg_job(job_id);
}

int Builtins::builtin_bg(const std::vector<std::string>& args) {
    auto& jm = JobManager::instance();
    
    int job_id = 0;
    if (args.size() > 1) {
        // Parse job ID from argument (e.g., %1 or 1)
        std::string arg = args[1];
        if (arg[0] == '%') arg = arg.substr(1);
        try {
            job_id = std::stoi(arg);
        } catch (...) {
            print_error("bg", "invalid job ID: " + args[1]);
            return 1;
        }
    } else {
        // Use current job (most recent)
        Job* job = jm.get_current_job();
        if (!job) {
            print_error("bg", "no current job");
            return 1;
        }
        job_id = job->id;
    }
    
    return jm.bg_job(job_id);
}

} // namespace aegissh