#include "jobs.hpp"
#include "signals.hpp"
#include "common.hpp"
#include <iostream>
#include <sys/wait.h>
#include <algorithm>
#include <unistd.h>
#include <termios.h>
#include <signal.h>

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

// Terminal management
void JobManager::save_shell_terminal() {
    shell_pgid_ = getpgrp();
    if (tcgetattr(STDIN_FILENO, &shell_termios_) == 0) {
        shell_termios_saved_ = true;
    }
}

void JobManager::restore_shell_terminal() {
    if (shell_termios_saved_) {
        tcsetattr(STDIN_FILENO, TCSADRAIN, &shell_termios_);
    }
    if (shell_pgid_ > 0) {
        tcsetpgrp(STDIN_FILENO, shell_pgid_);
    }
}

void JobManager::set_foreground_job(Job* job) {
    if (!job) return;
    
    current_fg_job_ = job;
    
    // Save job's terminal state if not already saved
    if (!job->termios_saved) {
        if (tcgetattr(STDIN_FILENO, &job->termios_save) == 0) {
            job->termios_saved = true;
        }
    }
    
    // Put job in foreground
    tcsetpgrp(STDIN_FILENO, job->pgid);
    
    // Restore job's terminal settings
    if (job->termios_saved) {
        tcsetattr(STDIN_FILENO, TCSADRAIN, &job->termios_save);
    }
}

void JobManager::release_foreground_job() {
    if (current_fg_job_) {
        // Save current terminal state for the job
        if (tcgetattr(STDIN_FILENO, &current_fg_job_->termios_save) == 0) {
            current_fg_job_->termios_saved = true;
        }
        current_fg_job_ = nullptr;
    }
    // Restore shell's terminal
    restore_shell_terminal();
}

// fg builtin - bring job to foreground
int JobManager::fg_job(int job_id) {
    Job* job = find_by_id(job_id);
    if (!job) {
        print_error("fg", "job not found: " + std::to_string(job_id));
        return 1;
    }
    
    if (job->state == JobState::DONE) {
        print_error("fg", "job " + std::to_string(job_id) + " already terminated");
        return 1;
    }
    
    std::cout << job->cmdline << "\n" << std::flush;
    
    // Send SIGCONT if stopped
    if (job->state == JobState::STOPPED) {
        kill(-job->pgid, SIGCONT);
        job->state = JobState::RUNNING;
    }
    
    // Put job in foreground
    set_foreground_job(job);
    
    // Wait for job to complete or stop
    int status = 0;
    while (true) {
        pid_t pid = waitpid(-job->pgid, &status, WUNTRACED);
        if (pid <= 0) {
            if (errno == EINTR) continue;
            break;
        }
        
        // Update job state
        if (WIFSTOPPED(status)) {
            job->state = JobState::STOPPED;
            job->exit_status = 128 + WSTOPSIG(status);
            std::cout << "\n[" << job->id << "]+  Stopped                 " << job->cmdline << "\n" << std::flush;
            break;
        } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
            // Remove completed process from job
            auto it = std::find(job->pids.begin(), job->pids.end(), pid);
            if (it != job->pids.end()) {
                job->pids.erase(it);
            }
            if (job->pids.empty()) {
                job->state = JobState::DONE;
                job->exit_status = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
            }
            if (!job->pids.empty()) {
                continue; // Still have processes running
            }
            break;
        }
    }
    
    // Release foreground, restore shell
    release_foreground_job();
    
    return job->exit_status;
}

// bg builtin - continue job in background
int JobManager::bg_job(int job_id) {
    Job* job = find_by_id(job_id);
    if (!job) {
        print_error("bg", "job not found: " + std::to_string(job_id));
        return 1;
    }
    
    if (job->state != JobState::STOPPED) {
        print_error("bg", "job " + std::to_string(job_id) + " not stopped");
        return 1;
    }
    
    job->state = JobState::RUNNING;
    job->notified = false;
    
    // Send SIGCONT to continue
    if (kill(-job->pgid, SIGCONT) < 0) {
        print_sys_error("bg");
        return 1;
    }
    
    std::cout << "[" << job->id << "]+  " << job->cmdline << " &\n" << std::flush;
    return 0;
}

} // namespace aegissh