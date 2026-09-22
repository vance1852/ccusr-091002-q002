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

    class DatabaseManager;

    // 独立数据库会话（独立连接），用于跨连接并发场景（如两名操作员同时关窗）。
    // 与 DatabaseManager 单例提供相同的执行接口。
    class Session {
    public:
        Session() = default;
        explicit Session(const config::DatabaseConfig& cfg) { open(cfg); }
        ~Session() { close(); }

        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;

        void open(const config::DatabaseConfig& cfg);
        void close();

        bool isConnected() const { return connected_ && conn_ != nullptr; }

        // 执行非查询 SQL (INSERT/UPDATE/DELETE)，返回受影响行数
        int execute(const std::string& sql);

        // 执行查询 SQL，返回结果集
        ResultSet query(const std::string& sql);

        // 执行 INSERT 并返回自增 ID
        long long insertAndGetId(const std::string& sql);

        // 转义字符串防 SQL 注入
        std::string escape(const std::string& str);

        // 事务控制
        void begin()   { execute("START TRANSACTION"); }
        void commit()  { execute("COMMIT"); }
        void rollback(){ execute("ROLLBACK"); }

    private:
        MYSQL* conn_ = nullptr;
        bool connected_ = false;
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

        // 基于当前配置打开一个独立会话（独立连接）
        std::shared_ptr<Session> openSession();

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
