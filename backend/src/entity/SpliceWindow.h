#pragma once
#include <string>
#include <stdexcept>

namespace entity {

    // 检测窗口状态机：OPEN -> PAUSED -> OPEN ... -> CLOSED（终态）
    enum class WindowState { OPEN, PAUSED, CLOSED };

    inline const char* toString(WindowState s) {
        switch (s) {
            case WindowState::OPEN:   return "OPEN";
            case WindowState::PAUSED: return "PAUSED";
            case WindowState::CLOSED: return "CLOSED";
        }
        return "UNKNOWN";
    }

    inline WindowState windowStateFrom(const std::string& s) {
        if (s == "OPEN")   return WindowState::OPEN;
        if (s == "PAUSED") return WindowState::PAUSED;
        if (s == "CLOSED") return WindowState::CLOSED;
        throw std::invalid_argument("unknown window state: " + s);
    }

    // 检测窗口：从开启到暂停、恢复、关闭的明确状态与起止时间
    struct SpliceWindow {
        int id = 0;
        std::string label;                 // 窗口标签（班次/产线）
        WindowState state = WindowState::OPEN;
        std::string openedAt;              // 开启时间
        std::string closedAt;              // 关闭时间，空串表示未关闭
        int version = 0;                   // 状态版本号，每次迁移加1
    };

    // 窗口状态迁移审计记录（交接复盘用）
    struct SpliceWindowEvent {
        long long id = 0;
        int windowId = 0;
        std::string action;                // OPEN / PAUSE / RESUME / CLOSE
        std::string fromState;             // 迁移前状态，OPEN 事件为 "-"
        std::string toState;               // 迁移后状态
        std::string actor;                 // 操作员
        std::string createdAt;             // 发生时间
    };

} // namespace entity
