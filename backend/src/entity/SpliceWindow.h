#pragma once
#include <string>
#include <vector>

namespace entity {

    // 接缝检测窗口：OPEN(开启/检测中) -> PAUSED(暂停) -> OPEN(恢复) -> CLOSED(关闭,终态)
    struct SpliceWindow {
        int id = 0;
        std::string shiftCode;             // 开启窗口的班次
        std::string oper;                  // 开启窗口的操作员
        std::string status;                // OPEN / PAUSED / CLOSED
        std::string openedAt;              // 开启时间
        std::string pausedAt;              // 最近一次暂停时间
        std::string resumedAt;             // 最近一次恢复时间
        std::string closedAt;              // 关闭时间
        std::string closedBy;              // 实际执行关闭的操作员（先关先得）
        std::string actor;                 // 最近一次迁移的操作员
        std::string lastStatus;            // 窗口内最后一次有效状态（关闭后定格）
        std::string lastStatusAt;          // 最后一次有效状态时间
        int lastSpliceId = 0;              // 最后一次有效状态对应的接缝记录
    };

    // 窗口生命周期事件：OPENED / PAUSED / RESUMED / CLOSED
    struct SpliceWindowEvent {
        long long id = 0;
        int windowId = 0;
        std::string event;
        std::string eventAt;
        std::string oper;
    };

} // namespace entity
