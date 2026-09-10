#pragma once
#include <string>

namespace entity {

    struct History {
        long long id = 0;          // 长ID为损伤，短ID为接缝
        std::string category;      // 损伤类型
        int level = 0;            // 损伤级别
        std::string url;           // 损伤记录保存路径
        int camera = 0;           // 监控摄像头编号
        std::string size;          // 损伤尺寸
        std::string date;          // 记录日期
    };

} // namespace entity
