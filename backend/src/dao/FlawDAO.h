#pragma once

#include "BaseDAO.h"
#include "../entity/Flaw.h"
#include <vector>
#include <optional>

namespace dao {

    class FlawDAO : public BaseDAO {
    public:
        long long insert(const entity::Flaw& f) {
            std::string sql = "INSERT INTO FLAW (category, level, url, camera, location, distance, "
                "size, coordinate, date, time, flag, stop, epoch) VALUES ("
                + esc(f.category) + ", "
                + itos(f.level) + ", "
                + esc(f.url) + ", "
                + itos(f.camera) + ", "
                + ftos(f.location) + ", "
                + ftos(f.distance) + ", "
                + esc(f.size) + ", "
                + esc(f.coordinate) + ", "
                + esc(f.date) + ", "
                + ftos(f.time) + ", "
                + itos(f.flag) + ", "
                + itos(f.stop) + ", "
                + itos(f.epoch) + ")";
            LOG_INFO("FlawDAO", "Insert flaw record, category=" + f.category);
            return db().insertAndGetId(sql);
        }

        std::optional<entity::Flaw> findById(long long id) {
            std::string sql = "SELECT * FROM FLAW WHERE id = " + lltos(id);
            auto rows = db().query(sql);
            if (rows.empty()) return std::nullopt;
            return mapRow(rows[0]);
        }

        std::vector<entity::Flaw> findAll() {
            return mapRows(db().query("SELECT * FROM FLAW ORDER BY id DESC"));
        }

        // 按损伤类型查询
        std::vector<entity::Flaw> findByCategory(const std::string& category) {
            std::string sql = "SELECT * FROM FLAW WHERE category = " + esc(category) + " ORDER BY id DESC";
            return mapRows(db().query(sql));
        }

        // 按损伤级别查询
        std::vector<entity::Flaw> findByLevel(int level) {
            std::string sql = "SELECT * FROM FLAW WHERE level = " + itos(level) + " ORDER BY id DESC";
            return mapRows(db().query(sql));
        }

        // 按摄像头编号查询
        std::vector<entity::Flaw> findByCamera(int camera) {
            std::string sql = "SELECT * FROM FLAW WHERE camera = " + itos(camera) + " ORDER BY id DESC";
            return mapRows(db().query(sql));
        }

        // 查询准备停机的损伤
        std::vector<entity::Flaw> findReadyToStop() {
            return mapRows(db().query("SELECT * FROM FLAW WHERE flag != 0 ORDER BY id DESC"));
        }

        // 查询可停机的损伤
        std::vector<entity::Flaw> findStoppable() {
            return mapRows(db().query("SELECT * FROM FLAW WHERE stop != 0 ORDER BY id DESC"));
        }

        // 按日期范围查询
        std::vector<entity::Flaw> findByDateRange(const std::string& startDate, const std::string& endDate) {
            std::string sql = "SELECT * FROM FLAW WHERE date >= " + esc(startDate)
                + " AND date <= " + esc(endDate) + " ORDER BY id DESC";
            return mapRows(db().query(sql));
        }

        int updateFlags(long long id, int flag, int stop) {
            std::string sql = "UPDATE FLAW SET flag = " + itos(flag)
                + ", stop = " + itos(stop)
                + " WHERE id = " + lltos(id);
            LOG_INFO("FlawDAO", "Update flaw flags id=" + lltos(id));
            return db().execute(sql);
        }

        int updateEpoch(long long id, int epoch) {
            std::string sql = "UPDATE FLAW SET epoch = " + itos(epoch) + " WHERE id = " + lltos(id);
            LOG_INFO("FlawDAO", "Update flaw epoch id=" + lltos(id) + " epoch=" + itos(epoch));
            return db().execute(sql);
        }

        int deleteById(long long id) {
            std::string sql = "DELETE FROM FLAW WHERE id = " + lltos(id);
            LOG_INFO("FlawDAO", "Delete flaw id=" + lltos(id));
            return db().execute(sql);
        }

        int count() {
            auto rows = db().query("SELECT COUNT(*) AS cnt FROM FLAW");
            return rows.empty() ? 0 : getInt(rows[0], "cnt");
        }

    private:
        entity::Flaw mapRow(const db::Row& row) {
            entity::Flaw f;
            f.id = getLong(row, "id");
            f.category = getVal(row, "category");
            f.level = getInt(row, "level");
            f.url = getVal(row, "url");
            f.camera = getInt(row, "camera");
            f.location = getFloat(row, "location");
            f.distance = getFloat(row, "distance");
            f.size = getVal(row, "size");
            f.coordinate = getVal(row, "coordinate");
            f.date = getVal(row, "date");
            f.time = getFloat(row, "time");
            f.flag = getInt(row, "flag");
            f.stop = getInt(row, "stop");
            f.epoch = getInt(row, "epoch");
            return f;
        }

        std::vector<entity::Flaw> mapRows(const db::ResultSet& rows) {
            std::vector<entity::Flaw> result;
            result.reserve(rows.size());
            for (auto& r : rows) result.push_back(mapRow(r));
            return result;
        }
    };

} // namespace dao
