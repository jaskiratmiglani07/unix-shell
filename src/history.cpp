#include "history.hpp"
#include "common.hpp"
#include <fstream>
#include <cstdlib>
#include <unistd.h>
#include <iomanip>
#include <sstream>

namespace aegissh {

History& History::instance() {
    static History hist;
    return hist;
}

void History::add(const std::string& command) {
    if (command.empty()) return;
    history_.push_back(command);
    if (max_size_ > 0 && history_.size() > max_size_) {
        history_.pop_front();
    }
}

const std::string* History::get(size_t index) const {
    // 1-based index like bash
    if (index == 0 || index > history_.size()) return nullptr;
    // history_ is 0-based, but we want 1-based from oldest
    return &history_[index - 1];
}

const char* History::default_history_file() {
    static std::string path;
    if (path.empty()) {
        // Check HISTFILE environment variable first
        const char* histfile = std::getenv("HISTFILE");
        if (histfile && *histfile) {
            path = histfile;
        } else {
            const char* home = std::getenv("HOME");
            if (home) {
                path = std::string(home) + "/.aegissh_history";
            } else {
                path = "/tmp/.aegissh_history";
            }
        }
    }
    return path.c_str();
}

void History::save_to_file(const std::string& filepath) const {
    std::ofstream ofs(filepath);
    if (!ofs) return;
    for (const auto& cmd : history_) {
        ofs << cmd << "\n";
    }
}

void History::load_from_file(const std::string& filepath) {
    std::ifstream ifs(filepath);
    if (!ifs) return;
    std::string line;
    while (std::getline(ifs, line)) {
        if (!line.empty()) {
            history_.push_back(line);
        }
    }
    if (max_size_ > 0 && history_.size() > max_size_) {
        history_.erase(history_.begin(), history_.begin() + (history_.size() - max_size_));
    }
}

} // namespace aegissh