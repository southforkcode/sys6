#pragma once

#include "system/terminal_io.h"

#include <deque>

class FakeTerminalIO : public TerminalIO {
public:
    void push(uint8_t byte) { m_queue.push_back(byte); }

    std::optional<uint8_t> tryReadByte() override {
        if (m_queue.empty()) {
            return std::nullopt;
        }
        uint8_t b = m_queue.front();
        m_queue.pop_front();
        return b;
    }

private:
    std::deque<uint8_t> m_queue;
};
