#include "DatabaseManager.h"
#include <cstring>

namespace db {

    static const char* TAG = "DatabaseManager";

    void DatabaseManager::init(const config::DatabaseConfig& cfg) {
        config_ = cfg;
        conn_ = mysql_init(nullptr);
        if (!conn_) {
            throw DatabaseException("mysql_init() failed: out of memory");
        }

        // 设置连接选项
        unsigned int timeout = cfg.connectTimeout;
        mysql_options(conn_, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);

        bool reconnectOpt = cfg.autoReconnect;
        mysql_options(conn_, MYSQL_OPT_RECONNECT, &reconnectOpt);

        mysql_options(conn_, MYSQL_SET_CHARSET_NAME, cfg.charset.c_str());

        // 建立连接
        if (!mysql_real_connect(conn_,
                cfg.host.c_str(),
                cfg.user.c_str(),
                cfg.password.c_str(),
                cfg.database.c_str(),
                cfg.port, nullptr, 0)) {
            std::string err = "Connection failed: ";
            err += mysql_error(conn_);
            mysql_close(conn_);
            conn_ = nullptr;
            throw DatabaseException(err);
        }

        connected_ = true;
        LOG_INFO(TAG, "Database connected to " + cfg.host + ":" + std::to_string(cfg.port) + "/" + cfg.database);
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
        LOG_DEBUG(TAG, "Execute: " + sql);

        if (mysql_query(conn_, sql.c_str()) != 0) {
            std::string err = "Execute failed: ";
            err += mysql_error(conn_);
            LOG_ERROR(TAG, err);
            throw DatabaseException(err);
        }

        int affected = static_cast<int>(mysql_affected_rows(conn_));
        LOG_DEBUG(TAG, "Affected rows: " + std::to_string(affected));
        return affected;
    }

    ResultSet DatabaseManager::query(const std::string& sql) {
        ensureConnected();
        LOG_DEBUG(TAG, "Query: " + sql);

        if (mysql_query(conn_, sql.c_str()) != 0) {
            std::string err = "Query failed: ";
            err += mysql_error(conn_);
            LOG_ERROR(TAG, err);
            throw DatabaseException(err);
        }

        MYSQL_RES* result = mysql_store_result(conn_);
        if (!result) {
            if (mysql_field_count(conn_) == 0) {
                return {}; // 非 SELECT 语句
            }
            std::string err = "Store result failed: ";
            err += mysql_error(conn_);
            throw DatabaseException(err);
        }

        ResultSet rows;
        int numFields = mysql_num_fields(result);
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
        LOG_DEBUG(TAG, "Query returned " + std::to_string(rows.size()) + " rows");
        return rows;
    }

    long long DatabaseManager::insertAndGetId(const std::string& sql) {
        ensureConnected();
        LOG_DEBUG(TAG, "InsertAndGetId: " + sql);

        if (mysql_query(conn_, sql.c_str()) != 0) {
            std::string err = "Insert failed: ";
            err += mysql_error(conn_);
            LOG_ERROR(TAG, err);
            throw DatabaseException(err);
        }

        long long id = static_cast<long long>(mysql_insert_id(conn_));
        LOG_DEBUG(TAG, "Inserted ID: " + std::to_string(id));
        return id;
    }

    std::string DatabaseManager::escape(const std::string& str) {
        ensureConnected();
        std::vector<char> buf(str.size() * 2 + 1);
        mysql_real_escape_string(conn_, buf.data(), str.c_str(), static_cast<unsigned long>(str.size()));
        return std::string(buf.data());
    }

    bool DatabaseManager::isConnected() const {
        return connected_ && conn_ != nullptr;
    }

} // namespace db
