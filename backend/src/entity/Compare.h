#pragma once
#include <string>

namespace entity {

    struct Compare {
        long long id = 0;          // 长ID为损伤，短ID为接缝
        std::string newUrl;        // 较新对比记录
        std::string oldUrl;        // 较旧对比记录
        float value = 0.0f;       // 对比结果
        int category = 0;         // 类型
        int level = 0;            // 结果级别
        std::string oldSize;       // 较旧记录尺寸（只对损伤有效）
    };

} // namespace entity
