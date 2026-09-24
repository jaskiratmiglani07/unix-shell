#pragma once

#include <string>
#include <vector>
#include <memory>
#include <sys/types.h>

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

private:
    JobManager() = default;
    int next_job_id_{1};
    std::vector<Job> jobs_;
};

} // namespace aegissh
