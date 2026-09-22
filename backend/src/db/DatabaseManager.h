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

        // 创建独立连接（调用方独占，用于多操作员并发等多连接场景）
        static std::shared_ptr<DatabaseManager> openConnection(const config::DatabaseConfig& cfg) {
            auto mgr = std::shared_ptr<DatabaseManager>(new DatabaseManager());
            mgr->init(cfg);
            return mgr;
        }

        // 关闭连接
        void close();

        // 事务控制（单连接内串行使用）
        void beginTransaction() { execute("START TRANSACTION"); }
        void commit() { execute("COMMIT"); }
        void rollback() { execute("ROLLBACK"); }

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

        ~DatabaseManager();

    private:
        DatabaseManager() = default;

        DatabaseManager(const DatabaseManager&) = delete;
        DatabaseManager& operator=(const DatabaseManager&) = delete;

        MYSQL* conn_ = nullptr;
        config::DatabaseConfig config_;
        bool connected_ = false;

        void ensureConnected();
        void reconnect();
    };

} // namespace db
