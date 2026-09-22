#include "DatabaseManager.h"
#include <cstring>

namespace db {

    static const char* TAG = "DatabaseManager";

    namespace {

        MYSQL* connectRaw(const config::DatabaseConfig& cfg) {
            MYSQL* conn = mysql_init(nullptr);
            if (!conn) {
                throw DatabaseException("mysql_init() failed: out of memory");
            }

            unsigned int timeout = cfg.connectTimeout;
            mysql_options(conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);

            bool reconnectOpt = cfg.autoReconnect;
            mysql_options(conn, MYSQL_OPT_RECONNECT, &reconnectOpt);

            mysql_options(conn, MYSQL_SET_CHARSET_NAME, cfg.charset.c_str());

            if (!mysql_real_connect(conn,
                    cfg.host.c_str(),
                    cfg.user.c_str(),
                    cfg.password.c_str(),
                    cfg.database.c_str(),
                    cfg.port, nullptr, 0)) {
                std::string err = "Connection failed: ";
                err += mysql_error(conn);
                mysql_close(conn);
                throw DatabaseException(err);
            }
            return conn;
        }

        int executeImpl(MYSQL* conn, const std::string& sql, const char* tag) {
            LOG_DEBUG(tag, "Execute: " + sql);
            if (mysql_query(conn, sql.c_str()) != 0) {
                std::string err = "Execute failed: ";
                err += mysql_error(conn);
                LOG_ERROR(tag, err);
                throw DatabaseException(err);
            }
            int affected = static_cast<int>(mysql_affected_rows(conn));
            LOG_DEBUG(tag, "Affected rows: " + std::to_string(affected));
            return affected;
        }

        ResultSet queryImpl(MYSQL* conn, const std::string& sql, const char* tag) {
            LOG_DEBUG(tag, "Query: " + sql);
            if (mysql_query(conn, sql.c_str()) != 0) {
                std::string err = "Query failed: ";
                err += mysql_error(conn);
                LOG_ERROR(tag, err);
                throw DatabaseException(err);
            }

            MYSQL_RES* result = mysql_store_result(conn);
            if (!result) {
                if (mysql_field_count(conn) == 0) {
                    return {}; // 非 SELECT 语句
                }
                std::string err = "Store result failed: ";
                err += mysql_error(conn);
                throw DatabaseException(err);
            }

            ResultSet rows;
            int numFields = static_cast<int>(mysql_num_fields(result));
            MYSQL_FIELD* fields = mysql_fetch_fields(result);

            MYSQL_ROW row;
            while ((row = mysql_fetch_row(result))) {
                unsigned long* lengths = mysql_fetch_lengths(result);
                Row r;
                for (int i = 0; i < numFields; ++i) {
                    std::string colName = fields[i].name;
                    std::string colVal = row[i] ? std::string(row[i], lengths[i]) : "";
                    r[colName] = colVal;
                }
                rows.push_back(std::move(r));
            }

            mysql_free_result(result);
            LOG_DEBUG(tag, "Query returned " + std::to_string(rows.size()) + " rows");
            return rows;
        }

        long long insertImpl(MYSQL* conn, const std::string& sql, const char* tag) {
            LOG_DEBUG(tag, "InsertAndGetId: " + sql);
            if (mysql_query(conn, sql.c_str()) != 0) {
                std::string err = "Insert failed: ";
                err += mysql_error(conn);
                LOG_ERROR(tag, err);
                throw DatabaseException(err);
            }
            long long id = static_cast<long long>(mysql_insert_id(conn));
            LOG_DEBUG(tag, "Inserted ID: " + std::to_string(id));
            return id;
        }

    } // anonymous namespace

    // ============================================
    // Session (独立连接)
    // ============================================
    void Session::open(const config::DatabaseConfig& cfg) {
        close();
        conn_ = connectRaw(cfg);
        connected_ = true;
        LOG_INFO("Session", "Database session opened to " + cfg.host + ":" +
                             std::to_string(cfg.port) + "/" + cfg.database);
    }

    void Session::close() {
        if (conn_) {
            mysql_close(conn_);
            conn_ = nullptr;
            connected_ = false;
        }
    }

    int Session::execute(const std::string& sql) {
        if (!conn_) throw DatabaseException("Session not open. Call open() first.");
        return executeImpl(conn_, sql, "Session");
    }

    ResultSet Session::query(const std::string& sql) {
        if (!conn_) throw DatabaseException("Session not open. Call open() first.");
        return queryImpl(conn_, sql, "Session");
    }

    long long Session::insertAndGetId(const std::string& sql) {
        if (!conn_) throw DatabaseException("Session not open. Call open() first.");
        return insertImpl(conn_, sql, "Session");
    }

    std::string Session::escape(const std::string& str) {
        if (!conn_) throw DatabaseException("Session not open. Call open() first.");
        std::vector<char> buf(str.size() * 2 + 1);
        mysql_real_escape_string(conn_, buf.data(), str.c_str(),
                                 static_cast<unsigned long>(str.size()));
        return std::string(buf.data());
    }

    // ============================================
    // DatabaseManager 单例
    // ============================================
    void DatabaseManager::init(const config::DatabaseConfig& cfg) {
        config_ = cfg;
        close();
        conn_ = connectRaw(cfg);
        connected_ = true;
        LOG_INFO(TAG, "Database connected to " + cfg.host + ":" +
                      std::to_string(cfg.port) + "/" + cfg.database);
    }

    void DatabaseManager::close() {
        if (conn_) {
            mysql_close(conn_);
            conn_ = nullptr;
            connected_ = false;
            LOG_INFO(TAG, "Database connection closed");
        }
    }

    DatabaseManager::~DatabaseManager() {
        close();
    }

    void DatabaseManager::ensureConnected() {
        if (!conn_ || !connected_) {
            throw DatabaseException("Database not connected. Call init() first.");
        }
        if (mysql_ping(conn_) != 0) {
            LOG_WARN(TAG, "Connection lost, attempting reconnect...");
            reconnect();
        }
    }

    void DatabaseManager::reconnect() {
        close();
        init(config_);
    }

    int DatabaseManager::execute(const std::string& sql) {
        ensureConnected();
        return executeImpl(conn_, sql, TAG);
    }

    ResultSet DatabaseManager::query(const std::string& sql) {
        ensureConnected();
        return queryImpl(conn_, sql, TAG);
    }

    long long DatabaseManager::insertAndGetId(const std::string& sql) {
        ensureConnected();
        return insertImpl(conn_, sql, TAG);
    }

    std::string DatabaseManager::escape(const std::string& str) {
        ensureConnected();
        std::vector<char> buf(str.size() * 2 + 1);
        mysql_real_escape_string(conn_, buf.data(), str.c_str(),
                                 static_cast<unsigned long>(str.size()));
        return std::string(buf.data());
    }

    bool DatabaseManager::isConnected() const {
        return connected_ && conn_ != nullptr;
    }

    std::shared_ptr<Session> DatabaseManager::openSession() {
        if (!connected_) {
            throw DatabaseException("Database not initialized. Call init() first.");
        }
        auto s = std::make_shared<Session>();
        s->open(config_);
        return s;
    }

} // namespace db
