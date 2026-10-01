#pragma once

#include <string>
#include <vector>
#include <memory>
#include <sys/types.h>
#include <termios.h>

namespace aegissh {

enum class JobState {
    RUNNING,
    STOPPED,
    DONE
};

struct Job {
    int id{0};
    pid_t pgid{0};
    std::vector<pid_t> pids;
    std::string cmdline;
    JobState state{JobState::RUNNING};
    int exit_status{0};
    bool notified{false};
    // Terminal state for job control
    struct termios termios_save;
    bool termios_saved{false};
};

class JobManager {
public:
    static JobManager& instance();

    int add_job(pid_t pgid, const std::vector<pid_t>& pids, const std::string& cmdline, JobState state = JobState::RUNNING);
    void reap_background_jobs();
    void print_completed_jobs();
    
    Job* find_by_id(int id);
    Job* find_by_pgid(pid_t pgid);
    Job* get_current_job();
    const std::vector<Job>& get_all_jobs() const { return jobs_; }
    void remove_job(int id);

    void clear();

    // Job control operations
    int fg_job(int job_id);
    int bg_job(int job_id);
    
    // Terminal management
    void save_shell_terminal();
    void restore_shell_terminal();
    void set_foreground_job(Job* job);
    void release_foreground_job();

private:
    JobManager() = default;
    int next_job_id_{1};
    std::vector<Job> jobs_;
    
    // Shell's terminal state
    struct termios shell_termios_;
    bool shell_termios_saved_{false};
    pid_t shell_pgid_{0};
    
    // Current foreground job
    Job* current_fg_job_{nullptr};
};

} // namespace aegissh