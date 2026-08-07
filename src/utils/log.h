#pragma once

#include <iostream>
#include <string>

class Logger {
public:
    enum log_level_t { INFO, WARNING, ERROR, DEBUG, TRACE };

public:
    Logger(std::ostream &os) : m_os(os), m_defaultLogLevel(INFO) {}
    Logger(std::ostream &os, log_level_t defaultLogLevel)
        : m_os(os), m_defaultLogLevel(defaultLogLevel) {}
    void log(const std::string &message);
    void log(log_level_t level, const std::string &message);
    void info(const std::string &message);
    void warn(const std::string &message);
    void error(const std::string &message);
    void debug(const std::string &message);
    void trace(const std::string &message);
    void setDefaultLogLevel(log_level_t level);

protected:
    std::ostream &m_os;
    log_level_t m_defaultLogLevel;
};
