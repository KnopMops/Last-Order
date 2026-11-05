#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace knopmops::core {

enum class LogLevel {
    Info,
    Warning,
    Error,
    Success
};

struct LogEntry {
    LogLevel level = LogLevel::Info;
    std::string message;
    std::string timeString;
};

class Log {
public:
    static Log& Instance() {
        static Log s_instance;
        return s_instance;
    }

    static void Info(const std::string& msg) { Instance().Add(LogLevel::Info, msg); }
    static void Warn(const std::string& msg) { Instance().Add(LogLevel::Warning, msg); }
    static void Error(const std::string& msg) { Instance().Add(LogLevel::Error, msg); }
    static void Success(const std::string& msg) { Instance().Add(LogLevel::Success, msg); }

    const std::vector<LogEntry>& GetEntries() const { return m_entries; }
    void Clear() { m_entries.clear(); }

private:
    void Add(LogLevel level, const std::string& msg) {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%H:%M:%S");

        m_entries.push_back(LogEntry{level, msg, ss.str()});
        if (m_entries.size() > 1000) {
            m_entries.erase(m_entries.begin(), m_entries.begin() + 100);
        }
    }

    std::vector<LogEntry> m_entries;
};

}

