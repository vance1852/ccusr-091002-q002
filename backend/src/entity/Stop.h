#pragma once

namespace entity {

    struct Stop {
        long long id = 0;         // 长ID为损伤，短ID为接缝
        int category = 0;         // 损伤类型
        float distance = 0.0f;    // 距离维修区距离
        int flag = 0;             // 停机标志，1为允许停机
        int command = 0;          // 停机命令标志，1为下发
    };

} // namespace entity
