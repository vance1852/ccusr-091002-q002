#pragma once

#include "BaseDAO.h"
#include "../entity/Remove.h"
#include <vector>
#include <optional>

namespace dao {

    class RemoveDAO : public BaseDAO {
    public:
        long long insert() {
            std::string sql = "INSERT INTO REMOVE VALUES ()";
            LOG_INFO("RemoveDAO", "Insert remove record");
            return db().insertAndGetId(sql);
        }

        // 插入指定ID的移除记录（用于关联损伤/接缝ID）
        int insertWithId(long long id) {
            std::string sql = "INSERT INTO REMOVE (id) VALUES (" + lltos(id) + ")";
            LOG_INFO("RemoveDAO", "Insert remove record id=" + lltos(id));
            return db().execute(sql);
        }

        std::optional<entity::Remove> findById(long long id) {
            std::string sql = "SELECT * FROM REMOVE WHERE id = " + lltos(id);
            auto rows = db().query(sql);
            if (rows.empty()) return std::nullopt;
            entity::Remove r;
            r.id = getLong(rows[0], "id");
            return r;
        }

        std::vector<entity::Remove> findAll() {
            auto rows = db().query("SELECT * FROM REMOVE ORDER BY id DESC");
            std::vector<entity::Remove> result;
            result.reserve(rows.size());
            for (auto& row : rows) {
                entity::Remove r;
                r.id = getLong(row, "id");
                result.push_back(r);
            }
            return result;
        }

        // 检查ID是否存在于移除表
        bool exists(long long id) {
            std::string sql = "SELECT COUNT(*) AS cnt FROM REMOVE WHERE id = " + lltos(id);
            auto rows = db().query(sql);
            return !rows.empty() && getInt(rows[0], "cnt") > 0;
        }

        int deleteById(long long id) {
            std::string sql = "DELETE FROM REMOVE WHERE id = " + lltos(id);
            LOG_INFO("RemoveDAO", "Delete remove id=" + lltos(id));
            return db().execute(sql);
        }

        int count() {
            auto rows = db().query("SELECT COUNT(*) AS cnt FROM REMOVE");
            return rows.empty() ? 0 : getInt(rows[0], "cnt");
        }
    };

} // namespace dao
