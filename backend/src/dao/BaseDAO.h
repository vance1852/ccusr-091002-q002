#pragma once

#include "../db/DatabaseManager.h"
#include <string>
#include <sstream>

namespace dao {

    // DAO 基类，提供通用工具方法
    class BaseDAO {
    protected:
        db::DatabaseManager& db() {
            return db::DatabaseManager::instance();
        }

        // 安全转义字符串
        std::string esc(const std::string& val) {
            return "'" + db().escape(val) + "'";
        }

        // 浮点数转字符串
        static std::string ftos(float val) {
            std::ostringstream oss;
            oss << val;
            return oss.str();
        }

        // 整数转字符串
        static std::string itos(int val) {
            return std::to_string(val);
        }

        // long long 转字符串
        static std::string lltos(long long val) {
            return std::to_string(val);
        }

        // 从 Row 安全取值
        static std::string getVal(const db::Row& row, const std::string& key) {
            auto it = row.find(key);
            return (it != row.end()) ? it->second : "";
        }

        static int getInt(const db::Row& row, const std::string& key) {
            std::string v = getVal(row, key);
            return v.empty() ? 0 : std::stoi(v);
        }

        static long long getLong(const db::Row& row, const std::string& key) {
            std::string v = getVal(row, key);
            return v.empty() ? 0LL : std::stoll(v);
        }

        static float getFloat(const db::Row& row, const std::string& key) {
            std::string v = getVal(row, key);
            return v.empty() ? 0.0f : std::stof(v);
        }
    };

} // namespace dao
