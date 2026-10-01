#include "signals.hpp"
#include <cstring>
#include <unistd.h>

namespace aegissh {

static volatile sig_atomic_t g_sigint_received  = 0;
static volatile sig_atomic_t g_sigchld_received = 0;

// SIGINT handler: record that a signal arrived.
// We do NOT write to stdout here to avoid interfering with std::cin on macOS.
// Interactive prompt clearing is handled in the REPL after getline returns.
// SA_RESTART is NOT set so that blocking reads (getline) wake up on SIGINT.
static void handle_sigint(int /*sig*/) {
    g_sigint_received = 1;
}

static void handle_sigchld(int /*sig*/) {
    g_sigchld_received = 1;
}

void SignalHandler::init_shell_signals() {
    struct sigaction sa_int;
    std::memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = handle_sigint;
    // No SA_RESTART: we want blocking reads to be interrupted so the REPL
    // can re-check the loop condition and print a fresh prompt on Ctrl+C.
    sigemptyset(&sa_int.sa_mask);
    sigaction(SIGINT, &sa_int, nullptr);

    struct sigaction sa_chld;
    std::memset(&sa_chld, 0, sizeof(sa_chld));
    sa_chld.sa_handler = handle_sigchld;
    // SA_RESTART so that waitpid/read calls inside executors are not
    // interrupted by SIGCHLD (background job completions).
    sa_chld.sa_flags   = SA_RESTART | SA_NOCLDSTOP;
    sigemptyset(&sa_chld.sa_mask);
    sigaction(SIGCHLD, &sa_chld, nullptr);

    // Shell ignores SIGQUIT, SIGTTIN, SIGTTOU (terminal job-control
    // signals that would otherwise stop or core-dump the shell itself).
    // SIGTSTP is NOT ignored - we let it stop the foreground job.
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    // Also ignore SIGPIPE so that writing to a broken pipe in a pipeline does
    // not kill the shell (individual pipeline stages handle SIGPIPE via DFL).
    signal(SIGPIPE, SIG_IGN);
}

void SignalHandler::reset_child_signals() {
    // Before execvp(), restore all signals to their default dispositions so
    // child processes behave normally (e.g. Ctrl+C kills them).
    signal(SIGINT,  SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
    signal(SIGPIPE, SIG_DFL);
}

bool SignalHandler::check_and_clear_sigint() {
    if (g_sigint_received) {
        g_sigint_received = 0;
        return true;
    }
    return false;
}

bool SignalHandler::check_and_clear_sigchld() {
    if (g_sigchld_received) {
        g_sigchld_received = 0;
        return true;
    }
    return false;
}

} // namespace aegissh
