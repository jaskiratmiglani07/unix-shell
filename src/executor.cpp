#include "executor.hpp"
#include "builtins.hpp"
#include "common.hpp"

namespace aegissh {

int Executor::execute(const Command& cmd, int last_status, bool& should_exit) {
    if (cmd.empty()) {
        return 0;
    }

    // 1. Builtin execution (runs directly in shell process)
    if (Builtins::is_builtin(cmd.args[0])) {
        return Builtins::execute(cmd.args, last_status, should_exit);
    }

    // 2. External command execution via fork/exec/waitpid
    pid_t pid = fork();
    if (pid < 0) {
        print_sys_error("fork");
        return 1;
    }

    if (pid == 0) {
        // --- CHILD PROCESS ---
        // Prepare null-terminated argv array for execvp
        std::vector<char*> c_argv;
        c_argv.reserve(cmd.args.size() + 1);
        for (const auto& arg : cmd.args) {
            c_argv.push_back(const_cast<char*>(arg.c_str()));
        }
        c_argv.push_back(nullptr);

        execvp(c_argv[0], c_argv.data());

        // If execvp returns, it failed
        if (errno == ENOENT) {
            print_error(cmd.args[0], "command not found");
            _exit(127);
        } else {
            print_sys_error(cmd.args[0]);
            _exit(126);
        }
    }

    // --- PARENT PROCESS ---
    int status = 0;
    while (true) {
        pid_t wpid = waitpid(pid, &status, 0);
        if (wpid < 0) {
            if (errno == EINTR) {
                continue; // Interrupted by signal, retry
            }
            print_sys_error("waitpid");
            return 1;
        }
        break;
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }

    return 0;
}

} // namespace aegissh
