#include "common/logger.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>

namespace rbft {

std::mutex Logger::mutex_;
std::string Logger::file_path_;

void Logger::SetLogFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    file_path_ = path;
}

static const char* LevelName(LogLevel level) {
    switch (level) {
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARN: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::DEBUG: return "DEBUG";
    }
    return "INFO";
}

void Logger::Log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    char ts[32]{};
    std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    std::string line = std::string(ts) + " [" + LevelName(level) + "] " + message + "\n";
    std::cerr << line;
    if (!file_path_.empty()) {
        std::ofstream out(file_path_, std::ios::app);
        out << line;
    }
}

void Logger::Info(const std::string& message) { Log(LogLevel::INFO, message); }
void Logger::Warn(const std::string& message) { Log(LogLevel::WARN, message); }
void Logger::Error(const std::string& message) { Log(LogLevel::ERROR, message); }
void Logger::Debug(const std::string& message) { Log(LogLevel::DEBUG, message); }

} // namespace rbft
