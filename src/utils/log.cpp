#include "log.h"

void Logger::log(const std::string &message) { log(m_defaultLogLevel, message); }

void Logger::log(log_level_t level, const std::string &message) {
    switch (level) {
    case INFO:
        m_os << "[INFO] ";
        break;
    case WARNING:
        m_os << "[WARN] ";
        break;
    case ERROR:
        m_os << "[ERRO] ";
        break;
    case DEBUG:
        m_os << "[DBUG] ";
        break;
    case TRACE:
        m_os << "[TRAC] ";
        break;
    }
    m_os << message;
    m_os << "\n";
}

void Logger::info(const std::string &message) { log(INFO, message); }
void Logger::warn(const std::string &message) { log(WARNING, message); }
void Logger::error(const std::string &message) { log(ERROR, message); }
void Logger::debug(const std::string &message) { log(DEBUG, message); }
void Logger::trace(const std::string &message) { log(TRACE, message); }
void Logger::setDefaultLogLevel(log_level_t level) { m_defaultLogLevel = level; }
