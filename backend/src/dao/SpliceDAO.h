#pragma once

#include "BaseDAO.h"
#include "../entity/Splice.h"
#include <vector>
#include <optional>

namespace dao {

    class SpliceDAO : public BaseDAO {
    public:
        int insert(const entity::Splice& s) {
            std::string sql = "INSERT INTO SPLICE (location, distance, time, url, last, flag, stop) VALUES ("
                + ftos(s.location) + ", "
                + ftos(s.distance) + ", "
                + esc(s.time) + ", "
                + esc(s.url) + ", "
                + itos(s.last) + ", "
                + itos(s.flag) + ", "
                + itos(s.stop) + ")";
            LOG_INFO("SpliceDAO", "Insert splice record");
            return static_cast<int>(db().insertAndGetId(sql));
        }

        std::optional<entity::Splice> findById(int id) {
            std::string sql = "SELECT * FROM SPLICE WHERE id = " + itos(id);
            auto rows = db().query(sql);
            if (rows.empty()) return std::nullopt;
            return mapRow(rows[0]);
        }

        std::vector<entity::Splice> findAll() {
            return mapRows(db().query("SELECT * FROM SPLICE ORDER BY id DESC"));
        }

        // 查询当前有效接头
        std::vector<entity::Splice> findActive() {
            return mapRows(db().query("SELECT * FROM SPLICE WHERE last = 1 ORDER BY id DESC"));
        }

        // 查询准备停机的接缝
        std::vector<entity::Splice> findReadyToStop() {
            return mapRows(db().query("SELECT * FROM SPLICE WHERE flag != 0 ORDER BY id DESC"));
        }

        // 查询可停机的接缝
        std::vector<entity::Splice> findStoppable() {
            return mapRows(db().query("SELECT * FROM SPLICE WHERE stop != 0 ORDER BY id DESC"));
        }

        int updateFlags(int id, int flag, int stop) {
            std::string sql = "UPDATE SPLICE SET flag = " + itos(flag)
                + ", stop = " + itos(stop)
                + " WHERE id = " + itos(id);
            LOG_INFO("SpliceDAO", "Update splice flags id=" + itos(id));
            return db().execute(sql);
        }

        int updateLast(int id, int last) {
            std::string sql = "UPDATE SPLICE SET last = " + itos(last) + " WHERE id = " + itos(id);
            LOG_INFO("SpliceDAO", "Update splice last id=" + itos(id));
            return db().execute(sql);
        }

        // 清除所有 last 标志
        int clearAllLast() {
            LOG_INFO("SpliceDAO", "Clear all last flags");
            return db().execute("UPDATE SPLICE SET last = 0 WHERE last = 1");
        }

        int deleteById(int id) {
            std::string sql = "DELETE FROM SPLICE WHERE id = " + itos(id);
            LOG_INFO("SpliceDAO", "Delete splice id=" + itos(id));
            return db().execute(sql);
        }

        int count() {
            auto rows = db().query("SELECT COUNT(*) AS cnt FROM SPLICE");
            return rows.empty() ? 0 : getInt(rows[0], "cnt");
        }

    private:
        entity::Splice mapRow(const db::Row& row) {
            entity::Splice s;
            s.id = getInt(row, "id");
            s.location = getFloat(row, "location");
            s.distance = getFloat(row, "distance");
            s.time = getVal(row, "time");
            s.url = getVal(row, "url");
            s.last = getInt(row, "last");
            s.flag = getInt(row, "flag");
            s.stop = getInt(row, "stop");
            return s;
        }

        std::vector<entity::Splice> mapRows(const db::ResultSet& rows) {
            std::vector<entity::Splice> result;
            result.reserve(rows.size());
            for (auto& r : rows) result.push_back(mapRow(r));
            return result;
        }
    };

} // namespace dao
