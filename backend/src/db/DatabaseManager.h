#pragma once

#ifdef _WIN32
#include <winsock2.h>
#endif
#include <mysql.h>

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <stdexcept>
#include <functional>
#include "../config/AppConfig.h"
#include "../utils/Logger.h"

namespace db {

    // 查询结果行: 列名 -> 值
    using Row = std::map<std::string, std::string>;
    using ResultSet = std::vector<Row>;

    // 数据库异常
    class DatabaseException : public std::runtime_error {
    public:
        explicit DatabaseException(const std::string& msg) : std::runtime_error(msg) {}
    };

    class DatabaseManager {
    public:
        static DatabaseManager& instance() {
            static DatabaseManager inst;
            return inst;
        }

        // 初始化连接
        void init(const config::DatabaseConfig& cfg);

        // 关闭连接
        void close();

        // 执行非查询 SQL (INSERT/UPDATE/DELETE)，返回受影响行数
        int execute(const std::string& sql);

        // 执行查询 SQL，返回结果集
        ResultSet query(const std::string& sql);

        // 执行 INSERT 并返回自增 ID
        long long insertAndGetId(const std::string& sql);

        // 转义字符串防 SQL 注入
        std::string escape(const std::string& str);

        // 检查连接是否存活
        bool isConnected() const;

    private:
        DatabaseManager() = default;
        ~DatabaseManager();

        DatabaseManager(const DatabaseManager&) = delete;
        DatabaseManager& operator=(const DatabaseManager&) = delete;

        MYSQL* conn_ = nullptr;
        config::DatabaseConfig config_;
        bool connected_ = false;

        void ensureConnected();
        void reconnect();
    };

} // namespace db
