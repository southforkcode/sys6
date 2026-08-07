#pragma once

#include "utils/log.h"

class CPU {
public:
    virtual void reset() = 0;
    virtual void executeInstruction() = 0;

    bool Debug() const { return m_debug; }
    void Debug(bool val) { m_debug = val; }
    bool Tracing() const { return m_tracing; }
    void Tracing(bool val) { m_tracing = val; }
    void setLogger(Logger* logger) { m_logger = logger; }

protected:
    bool m_debug;
    bool m_tracing;

    Logger* m_logger = nullptr;
};