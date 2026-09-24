#include "executor.hpp"
#include "builtins.hpp"
#include "common.hpp"
#include "fd_guard.hpp"
#include <fcntl.h>

namespace aegissh {

static bool apply_redirections(const std::vector<Redirection>& redirs) {
    for (const auto& r : redirs) {
        int fd = -1;
        int target_fd = -1;

        switch (r.type) {
            case Redirection::Type::INPUT:
                fd = open(r.filename.c_str(), O_RDONLY);
                target_fd = STDIN_FILENO;
                break;
            case Redirection::Type::OUTPUT_TRUNC:
                fd = open(r.filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                target_fd = STDOUT_FILENO;
                break;
            case Redirection::Type::OUTPUT_APPEND:
                fd = open(r.filename.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
                target_fd = STDOUT_FILENO;
                break;
            case Redirection::Type::ERR_TRUNC:
                fd = open(r.filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                target_fd = STDERR_FILENO;
                break;
            case Redirection::Type::ERR_APPEND:
                fd = open(r.filename.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
                target_fd = STDERR_FILENO;
                break;
        }

        if (fd < 0) {
            print_sys_error(r.filename);
            return false;
        }

        if (dup2(fd, target_fd) < 0) {
            print_sys_error("dup2");
            close(fd);
            return false;
        }
        close(fd);
    }
    return true;
}

int Executor::execute(const Command& cmd, int last_status, bool& should_exit) {
    if (cmd.empty()) {
        return 0;
    }

    // 1. Builtin execution (runs directly in shell process)
    if (!cmd.args.empty() && Builtins::is_builtin(cmd.args[0])) {
        if (cmd.redirections.empty()) {
            return Builtins::execute(cmd.args, last_status, should_exit);
        }

        // Save original descriptors before applying redirections
        int s_in = dup(STDIN_FILENO);
        int s_out = dup(STDOUT_FILENO);
        int s_err = dup(STDERR_FILENO);
        FdGuard guard_in(s_in), guard_out(s_out), guard_err(s_err);

        if (!apply_redirections(cmd.redirections)) {
            // Restore immediately
            if (guard_in.valid()) dup2(guard_in.get(), STDIN_FILENO);
            if (guard_out.valid()) dup2(guard_out.get(), STDOUT_FILENO);
            if (guard_err.valid()) dup2(guard_err.get(), STDERR_FILENO);
            return 1;
        }

        int rc = Builtins::execute(cmd.args, last_status, should_exit);
        std::cout << std::flush;
        std::cerr << std::flush;
        fflush(stdout);
        fflush(stderr);

        // Restore descriptors
        if (guard_in.valid()) dup2(guard_in.get(), STDIN_FILENO);
        if (guard_out.valid()) dup2(guard_out.get(), STDOUT_FILENO);
        if (guard_err.valid()) dup2(guard_err.get(), STDERR_FILENO);

        return rc;
    }

    // 2. External command execution via fork/exec/waitpid
    pid_t pid = fork();
    if (pid < 0) {
        print_sys_error("fork");
        return 1;
    }

    if (pid == 0) {
        // --- CHILD PROCESS ---
        if (!apply_redirections(cmd.redirections)) {
            _exit(1);
        }

        if (cmd.args.empty()) {
            _exit(0);
        }

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
                continue;
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
