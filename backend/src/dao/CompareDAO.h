#pragma once

#include "BaseDAO.h"
#include "../entity/Compare.h"
#include <vector>
#include <optional>

namespace dao {

    class CompareDAO : public BaseDAO {
    public:
        long long insert(const entity::Compare& c) {
            std::string sql = "INSERT INTO COMPARE (new_url, old_url, value, category, level, old_size) VALUES ("
                + esc(c.newUrl) + ", "
                + esc(c.oldUrl) + ", "
                + ftos(c.value) + ", "
                + itos(c.category) + ", "
                + itos(c.level) + ", "
                + esc(c.oldSize) + ")";
            LOG_INFO("CompareDAO", "Insert compare record");
            return db().insertAndGetId(sql);
        }

        std::optional<entity::Compare> findById(long long id) {
            std::string sql = "SELECT * FROM COMPARE WHERE id = " + lltos(id);
            auto rows = db().query(sql);
            if (rows.empty()) return std::nullopt;
            return mapRow(rows[0]);
        }

        std::vector<entity::Compare> findAll() {
            return mapRows(db().query("SELECT * FROM COMPARE ORDER BY id DESC"));
        }

        // 按类型查询
        std::vector<entity::Compare> findByCategory(int category) {
            std::string sql = "SELECT * FROM COMPARE WHERE category = " + itos(category) + " ORDER BY id DESC";
            return mapRows(db().query(sql));
        }

        // 按级别查询
        std::vector<entity::Compare> findByLevel(int level) {
            std::string sql = "SELECT * FROM COMPARE WHERE level = " + itos(level) + " ORDER BY id DESC";
            return mapRows(db().query(sql));
        }

        int update(const entity::Compare& c) {
            std::string sql = "UPDATE COMPARE SET new_url = " + esc(c.newUrl)
                + ", old_url = " + esc(c.oldUrl)
                + ", value = " + ftos(c.value)
                + ", category = " + itos(c.category)
                + ", level = " + itos(c.level)
                + ", old_size = " + esc(c.oldSize)
                + " WHERE id = " + lltos(c.id);
            LOG_INFO("CompareDAO", "Update compare id=" + lltos(c.id));
            return db().execute(sql);
        }

        int deleteById(long long id) {
            std::string sql = "DELETE FROM COMPARE WHERE id = " + lltos(id);
            LOG_INFO("CompareDAO", "Delete compare id=" + lltos(id));
            return db().execute(sql);
        }

        int count() {
            auto rows = db().query("SELECT COUNT(*) AS cnt FROM COMPARE");
            return rows.empty() ? 0 : getInt(rows[0], "cnt");
        }

    private:
        entity::Compare mapRow(const db::Row& row) {
            entity::Compare c;
            c.id = getLong(row, "id");
            c.newUrl = getVal(row, "new_url");
            c.oldUrl = getVal(row, "old_url");
            c.value = getFloat(row, "value");
            c.category = getInt(row, "category");
            c.level = getInt(row, "level");
            c.oldSize = getVal(row, "old_size");
            return c;
        }

        std::vector<entity::Compare> mapRows(const db::ResultSet& rows) {
            std::vector<entity::Compare> result;
            result.reserve(rows.size());
            for (auto& r : rows) result.push_back(mapRow(r));
            return result;
        }
    };

} // namespace dao
