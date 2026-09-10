#pragma once
#include <string>

namespace entity {

    struct Flaw {
        long long id = 0;
        std::string category;      // 损伤类型
        int level = 0;             // 损伤级别
        std::string url;           // 损伤记录保存路径
        int camera = 0;           // 监控摄像头编号
        float location = 0.0f;    // 当前位置
        float distance = 0.0f;    // 距离维修区距离
        std::string size;          // 损伤尺寸
        std::string coordinate;    // 损伤坐标
        std::string date;          // 记录日期
        float time = 0.0f;        // 倒计时时间（秒）
        int flag = 0;             // 准备标志
        int stop = 0;             // 停机标志
        int epoch = 0;            // 追踪当前缺陷的圈数
    };

} // namespace entity
