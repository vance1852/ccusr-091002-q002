#pragma once
#include <string>

namespace entity {

    struct Speed {
        int id = 0;
        float value = 0.0f;
        std::string date;
        int flag = 0;  // 使用标志，1为已使用（废弃）
    };

} // namespace entity
