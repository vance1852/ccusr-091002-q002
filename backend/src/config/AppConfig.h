#pragma once

#include <string>

namespace config {

    // 数据库连接配置 - 可通过环境变量或配置文件覆盖
    struct DatabaseConfig {
        std::string host = "127.0.0.1";
        unsigned int port = 3306;
        std::string user = "root";
        std::string password = "root";
        std::string database = "industrial_inspection";
        std::string charset = "utf8mb4";
        unsigned int connectTimeout = 10;  // 秒
        bool autoReconnect = true;

        // 从环境变量加载
        void loadFromEnv() {
            if (const char* v = std::getenv("DB_HOST"))     host = v;
            if (const char* v = std::getenv("DB_PORT"))     port = static_cast<unsigned int>(std::stoul(v));
            if (const char* v = std::getenv("DB_USER"))     user = v;
            if (const char* v = std::getenv("DB_PASSWORD")) password = v;
            if (const char* v = std::getenv("DB_NAME"))     database = v;
        }
    };

} // namespace config
