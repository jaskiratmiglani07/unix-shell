# AegisShell - A Unix-like Shell in C++

A fully-featured Unix-like shell implemented from scratch in C++17, demonstrating deep understanding of operating system concepts including processes, signals, pipes, job control, and terminal management.

## Features

### Phase 1: Core REPL & Process Execution
- Interactive shell prompt with colored CWD display
- Command parsing with comment support (`#`)
- External command execution via `fork()`/`execvp()`/`waitpid()`
- Builtin commands: `pwd`, `exit`

### Phase 2: Shell Builtins & Environment
- `cd` - Change directory (supports `~`, `-` for OLDPWD)
- `echo` - Print arguments (supports `-n` flag)
- `env` - Display environment variables
- `export` - Set/export environment variables
- `unset` - Remove environment variables
- PATH resolution for external commands

### Phase 3: I/O Redirection
- `>`  - Output redirection (truncate)
- `>>` - Output redirection (append)
- `<`  - Input redirection
- `2>` - Stderr redirection (truncate)
- `2>>` - Stderr redirection (append)
- Works with both builtins and external commands

### Phase 4: Pipelines
- Multi-stage pipelines (`cmd1 | cmd2 | cmd3`)
- Proper pipe lifecycle management
- Pipeline exit status reflects last command
- Combined with redirection support

### Phase 5: Background Processes
- `&` suffix for background execution
- Job ID assignment and tracking
- Zombie process prevention via `waitpid(WNOHANG)`
- Completion notifications

### Phase 6: Signal Handling
- SIGINT (Ctrl+C) - Survives shell, terminates foreground child
- SIGQUIT - Ignored by shell
- SIGTSTP (Ctrl+Z) - Stops foreground job
- SIGCHLD - Reaps background children
- SIGPIPE - Ignored (handled by pipeline stages)
- Async-signal-safe handlers

### Phase 7: Quoting & Variable Expansion
- Single quotes (`'...'`) - Literal, no expansion
- Double quotes (`"..."`) - Expansion with selective escaping
- Unquoted - Expansion with word splitting
- Variable expansion: `$VAR`, `${VAR}`, `$?`
- Escape sequences: `\ `, `\$`, `\"`, `\\`

### Phase 8: Command History
- In-memory history with configurable size (default 1000)
- Persistent history file (`~/.aegissh_history` or `$HISTFILE`)
- `history` builtin with numbered output
- Loads on startup, saves on exit

### Phase 9: Job Control
- Process group management for pipelines
- Terminal control (`tcsetpgrp`/`tcgetpgrp`)
- `fg` - Bring job to foreground
- `bg` - Continue stopped job in background
- `jobs` - List all jobs with status
- Job states: Running, Stopped, Done
- Terminal state save/restore

## Building

```bash
# Standard build
make

# Debug build with symbols
make debug

# With Undefined Behavior Sanitizer
make ubsan

# Run all tests
make test

# Clean
make clean
```

## Running

```bash
./bin/aegissh
```

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                         main.cpp                            │
│                    (entry point)                            │
└─────────────────────────┬───────────────────────────────────┘
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                      Shell Class                            │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐ │
│  │  REPL Loop  │  │  Prompt     │  │  History Manager    │ │
│  │  Signal Init│  │  CWD Format │  │  (load/save)        │ │
│  └──────┬──────┘  └─────────────┘  └─────────────────────┘ │
└─────────┼───────────────────────────────────────────────────┘
          ▼
┌─────────────────────────────────────────────────────────────┐
│                      Parser                                 │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐ │
│  │  Pipeline   │  │  Command    │  │  Expander           │ │
│  │  Split (|)  │  │  Split      │  │  ($VAR, quotes, \)  │ │
│  └─────────────┘  └─────────────┘  └─────────────────────┘ │
└─────────┼───────────────────────────────────────────────────┘
          ▼
┌─────────────────────────────────────────────────────────────┐
│                     Executor                                │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐ │
│  │  Builtin    │  │  Single     │  │  Pipeline           │ │
│  │  Dispatch   │  │  Command    │  │  Execution          │ │
│  └─────────────┘  └─────────────┘  └─────────────────────┘ │
└─────────┼───────────────────────────────────────────────────┘
          ▼
┌─────────────────────────────────────────────────────────────┐
│                   Job Manager                               │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐ │
│  │  Job List   │  │  Terminal   │  │  fg/bg/jobs         │ │
│  │  (pid, pgid)│  │  Control    │  │  Builtins           │ │
│  └─────────────┘  └─────────────┘  └─────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

## System Calls Used

| Category | System Calls |
|----------|--------------|
| Process | `fork()`, `execvp()`, `waitpid()`, `getpid()`, `getppid()` |
| Process Groups | `setpgid()`, `getpgrp()`, `tcsetpgrp()`, `tcgetpgrp()` |
| File I/O | `open()`, `close()`, `dup2()`, `pipe()`, `fcntl()` |
| Signals | `sigaction()`, `signal()`, `kill()`, `sigemptyset()` |
| Terminal | `tcgetattr()`, `tcsetattr()`, `isatty()`, `ttyname()` |
| Directory | `chdir()`, `getcwd()` |
| Environment | `getenv()`, `setenv()`, `unsetenv()` |

## Key Design Decisions

### Signal Safety
- Signal handlers only set `volatile sig_atomic_t` flags
- No async-unsafe functions in handlers
- SA_RESTART used appropriately for SIGCHLD
- SIGINT interrupts `getline()` for responsive Ctrl+C

### Process Group Management
- Each pipeline gets its own process group
- Shell maintains control of terminal
- Foreground job gets terminal via `tcsetpgrp()`
- Background jobs don't own terminal

### Resource Management
- RAII for file descriptors (`FdGuard` class)
- Proper cleanup in all error paths
- No file descriptor leaks in pipelines
- Zombie prevention via `WNOHANG` reaping

### Portability
- POSIX-compliant system calls
- Works on macOS and Linux
- Handles macOS-specific `getline()` EOF behavior on signals

## Limitations

- No command substitution (`$(...)` or backticks)
- No arithmetic expansion (`$((...))`)
- No brace expansion (`{a,b}`)
- No programmable completion
- No aliases or functions
- Job control requires interactive terminal (not FIFO/pipe)

## Testing

Comprehensive test suite covering all phases:
- 95+ individual test cases
- Automated via `make test`
- Tests for success and error cases
- Edge cases: empty input, syntax errors, signals

## License

MIT License - See LICENSE file for details.

## Educational Value

This project demonstrates:
- **Process creation & management**: fork/exec/wait lifecycle
- **Inter-process communication**: pipes, signals
- **File descriptor manipulation**: dup2, redirection
- **Terminal control**: process groups, tcsetpgrp
- **Signal handling**: async-safe patterns
- **Memory safety**: RAII, sanitizers
- **Software engineering**: modular design, testing, git history