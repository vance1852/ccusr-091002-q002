#pragma once
#include <string>

namespace entity {

    struct Splice {
        int id = 0;
        float location = 0.0f;    // 当前位置
        float distance = 0.0f;    // 距离维修区距离
        std::string time;          // 倒计时时间（秒）
        std::string url;           // 保存路径
        int last = 0;             // 当前检测接头标志，1有效
        int flag = 0;             // 准备标志，不为0则准备停机
        int stop = 0;             // 停机标志，不为0则可以停机
    };

} // namespace entity
