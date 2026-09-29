#pragma once

#include <string>
#include <vector>
#include <deque>

namespace aegissh {

class History {
public:
    static History& instance();
    
    void add(const std::string& command);
    const std::deque<std::string>& get_all() const { return history_; }
    size_t size() const { return history_.size(); }
    void clear() { history_.clear(); }
    
    // Get history entry by index (1-based, like bash)
    const std::string* get(size_t index) const;
    
    // Save/load from file
    void save_to_file(const std::string& filepath) const;
    void load_from_file(const std::string& filepath);
    
    // Set maximum history size (0 = unlimited)
    void set_max_size(size_t max) { max_size_ = max; }
    size_t max_size() const { return max_size_; }
    
    // Get default history file path
    static const char* default_history_file();

private:
    History() = default;
    std::deque<std::string> history_;
    size_t max_size_ = 1000;
};

} // namespace aegissh