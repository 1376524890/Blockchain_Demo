#pragma once

#include <mutex>
#include <string>

namespace rbft {

enum class LogLevel { INFO, WARN, ERROR, DEBUG };

class Logger {
public:
    static void SetLogFile(const std::string& path);
    static void Log(LogLevel level, const std::string& message);
    static void Info(const std::string& message);
    static void Warn(const std::string& message);
    static void Error(const std::string& message);
    static void Debug(const std::string& message);

private:
    static std::mutex mutex_;
    static std::string file_path_;
};

} // namespace rbft
