#pragma once

#include "BaseDAO.h"
#include "../entity/History.h"
#include <vector>
#include <optional>

namespace dao {

    class HistoryDAO : public BaseDAO {
    public:
        long long insert(const entity::History& h) {
            std::string sql = "INSERT INTO HISTORY (category, level, url, camera, size, date) VALUES ("
                + esc(h.category) + ", "
                + itos(h.level) + ", "
                + esc(h.url) + ", "
                + itos(h.camera) + ", "
                + esc(h.size) + ", "
                + esc(h.date) + ")";
            LOG_INFO("HistoryDAO", "Insert history record, category=" + h.category);
            return db().insertAndGetId(sql);
        }

        std::optional<entity::History> findById(long long id) {
            std::string sql = "SELECT * FROM HISTORY WHERE id = " + lltos(id);
            auto rows = db().query(sql);
            if (rows.empty()) return std::nullopt;
            return mapRow(rows[0]);
        }

        std::vector<entity::History> findAll() {
            return mapRows(db().query("SELECT * FROM HISTORY ORDER BY id DESC"));
        }

        // 按损伤类型查询
        std::vector<entity::History> findByCategory(const std::string& category) {
            std::string sql = "SELECT * FROM HISTORY WHERE category = " + esc(category) + " ORDER BY id DESC";
            return mapRows(db().query(sql));
        }

        // 按日期查询
        std::vector<entity::History> findByDate(const std::string& date) {
            std::string sql = "SELECT * FROM HISTORY WHERE date = " + esc(date) + " ORDER BY id DESC";
            return mapRows(db().query(sql));
        }

        // 按摄像头编号查询
        std::vector<entity::History> findByCamera(int camera) {
            std::string sql = "SELECT * FROM HISTORY WHERE camera = " + itos(camera) + " ORDER BY id DESC";
            return mapRows(db().query(sql));
        }

        // 按日期范围查询
        std::vector<entity::History> findByDateRange(const std::string& startDate, const std::string& endDate) {
            std::string sql = "SELECT * FROM HISTORY WHERE date >= " + esc(startDate)
                + " AND date <= " + esc(endDate) + " ORDER BY id DESC";
            return mapRows(db().query(sql));
        }

        int deleteById(long long id) {
            std::string sql = "DELETE FROM HISTORY WHERE id = " + lltos(id);
            LOG_INFO("HistoryDAO", "Delete history id=" + lltos(id));
            return db().execute(sql);
        }

        int count() {
            auto rows = db().query("SELECT COUNT(*) AS cnt FROM HISTORY");
            return rows.empty() ? 0 : getInt(rows[0], "cnt");
        }

    private:
        entity::History mapRow(const db::Row& row) {
            entity::History h;
            h.id = getLong(row, "id");
            h.category = getVal(row, "category");
            h.level = getInt(row, "level");
            h.url = getVal(row, "url");
            h.camera = getInt(row, "camera");
            h.size = getVal(row, "size");
            h.date = getVal(row, "date");
            return h;
        }

        std::vector<entity::History> mapRows(const db::ResultSet& rows) {
            std::vector<entity::History> result;
            result.reserve(rows.size());
            for (auto& r : rows) result.push_back(mapRow(r));
            return result;
        }
    };

} // namespace dao
