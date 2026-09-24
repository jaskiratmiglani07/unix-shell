#include "jobs.hpp"
#include <iostream>
#include <sys/wait.h>
#include <algorithm>

namespace aegissh {

JobManager& JobManager::instance() {
    static JobManager mgr;
    return mgr;
}

int JobManager::add_job(pid_t pgid, const std::vector<pid_t>& pids, const std::string& cmdline, JobState state) {
    Job job;
    job.id = next_job_id_++;
    job.pgid = pgid;
    job.pids = pids;
    job.cmdline = cmdline;
    job.state = state;
    job.notified = false;

    jobs_.push_back(job);
    return job.id;
}

Job* JobManager::find_by_id(int id) {
    for (auto& job : jobs_) {
        if (job.id == id) return &job;
    }
    return nullptr;
}

Job* JobManager::find_by_pgid(pid_t pgid) {
    for (auto& job : jobs_) {
        if (job.pgid == pgid) return &job;
    }
    return nullptr;
}

Job* JobManager::get_current_job() {
    if (jobs_.empty()) return nullptr;
    return &jobs_.back();
}

void JobManager::remove_job(int id) {
    jobs_.erase(
        std::remove_if(jobs_.begin(), jobs_.end(), [id](const Job& j) { return j.id == id; }),
        jobs_.end()
    );
}

void JobManager::clear() {
    jobs_.clear();
    next_job_id_ = 1;
}

void JobManager::reap_background_jobs() {
    int status = 0;
    while (true) {
        pid_t pid = waitpid(-1, &status, WNOHANG | WUNTRACED);
        if (pid <= 0) {
            break;
        }

        for (auto& job : jobs_) {
            auto it = std::find(job.pids.begin(), job.pids.end(), pid);
            if (it != job.pids.end()) {
                if (WIFSTOPPED(status)) {
                    job.state = JobState::STOPPED;
                } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
                    job.pids.erase(it);
                    if (job.pids.empty()) {
                        job.state = JobState::DONE;
                        job.exit_status = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
                    }
                }
                break;
            }
        }
    }
}

void JobManager::print_completed_jobs() {
    auto it = jobs_.begin();
    while (it != jobs_.end()) {
        if (it->state == JobState::DONE) {
            std::cout << "[" << it->id << "]+  Done                    " << it->cmdline << "\n";
            it = jobs_.erase(it);
        } else {
            if (it->state == JobState::STOPPED && !it->notified) {
                std::cout << "[" << it->id << "]+  Stopped                 " << it->cmdline << "\n";
                it->notified = true;
            }
            ++it;
        }
    }
    std::cout << std::flush;
}

} // namespace aegissh
