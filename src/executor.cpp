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

int Executor::execute_command(const Command& cmd, int last_status, bool& should_exit) {
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

        if (guard_in.valid()) dup2(guard_in.get(), STDIN_FILENO);
        if (guard_out.valid()) dup2(guard_out.get(), STDOUT_FILENO);
        if (guard_err.valid()) dup2(guard_err.get(), STDERR_FILENO);

        return rc;
    }

    // 2. External single command execution via fork/exec/waitpid
    pid_t pid = fork();
    if (pid < 0) {
        print_sys_error("fork");
        return 1;
    }

    if (pid == 0) {
        // Child
        if (!apply_redirections(cmd.redirections)) {
            _exit(1);
        }

        if (cmd.args.empty()) {
            _exit(0);
        }

        std::vector<char*> c_argv;
        c_argv.reserve(cmd.args.size() + 1);
        for (const auto& arg : cmd.args) {
            c_argv.push_back(const_cast<char*>(arg.c_str()));
        }
        c_argv.push_back(nullptr);

        execvp(c_argv[0], c_argv.data());

        if (errno == ENOENT) {
            print_error(cmd.args[0], "command not found");
            _exit(127);
        } else {
            print_sys_error(cmd.args[0]);
            _exit(126);
        }
    }

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

int Executor::execute_pipeline(const Pipeline& pipeline, int last_status, bool& should_exit) {
    should_exit = false;
    if (pipeline.empty()) {
        return 0;
    }

    if (pipeline.commands.size() == 1) {
        return execute_command(pipeline.commands[0], last_status, should_exit);
    }

    // Multi-stage pipeline execution
    size_t nstages = pipeline.commands.size();
    std::vector<pid_t> pids(nstages, -1);
    int in_fd = STDIN_FILENO;
    int pipefds[2];

    for (size_t i = 0; i < nstages; ++i) {
        const Command& cmd = pipeline.commands[i];
        int out_fd = STDOUT_FILENO;

        if (i + 1 < nstages) {
            if (pipe(pipefds) < 0) {
                print_sys_error("pipe");
                if (in_fd != STDIN_FILENO) close(in_fd);
                return 1;
            }
            out_fd = pipefds[1];
        }

        pid_t pid = fork();
        if (pid < 0) {
            print_sys_error("fork");
            if (i + 1 < nstages) {
                close(pipefds[0]);
                close(pipefds[1]);
            }
            if (in_fd != STDIN_FILENO) close(in_fd);
            return 1;
        }

        if (pid == 0) {
            // Child stage
            if (i + 1 < nstages) {
                close(pipefds[0]); // Unused read end of next pipe
            }

            if (in_fd != STDIN_FILENO) {
                if (dup2(in_fd, STDIN_FILENO) < 0) _exit(1);
                close(in_fd);
            }

            if (out_fd != STDOUT_FILENO) {
                if (dup2(out_fd, STDOUT_FILENO) < 0) _exit(1);
                close(out_fd);
            }

            if (!apply_redirections(cmd.redirections)) {
                _exit(1);
            }

            if (cmd.args.empty()) {
                _exit(0);
            }

            // Builtins inside pipelines run in subshell
            if (Builtins::is_builtin(cmd.args[0])) {
                bool sub_exit = false;
                int rc = Builtins::execute(cmd.args, last_status, sub_exit);
                std::cout << std::flush;
                std::cerr << std::flush;
                fflush(stdout);
                fflush(stderr);
                _exit(rc & 0xFF);
            }

            std::vector<char*> c_argv;
            c_argv.reserve(cmd.args.size() + 1);
            for (const auto& arg : cmd.args) {
                c_argv.push_back(const_cast<char*>(arg.c_str()));
            }
            c_argv.push_back(nullptr);

            execvp(c_argv[0], c_argv.data());

            if (errno == ENOENT) {
                print_error(cmd.args[0], "command not found");
                _exit(127);
            } else {
                print_sys_error(cmd.args[0]);
                _exit(126);
            }
        }

        // Parent stage cleanup
        pids[i] = pid;

        if (in_fd != STDIN_FILENO) {
            close(in_fd);
        }

        if (i + 1 < nstages) {
            close(pipefds[1]); // Close write end so reader receives EOF
            in_fd = pipefds[0];
        }
    }

    // Wait for all stages to prevent zombies and retrieve exit status of last stage
    int last_stage_status = 0;
    for (size_t i = 0; i < nstages; ++i) {
        int status = 0;
        while (true) {
            pid_t wpid = waitpid(pids[i], &status, 0);
            if (wpid < 0) {
                if (errno == EINTR) continue;
                break;
            }
            break;
        }

        if (i == nstages - 1) {
            if (WIFEXITED(status)) {
                last_stage_status = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                last_stage_status = 128 + WTERMSIG(status);
            }
        }
    }

    return last_stage_status;
}

} // namespace aegissh
