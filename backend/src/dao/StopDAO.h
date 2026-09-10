#pragma once

#include "BaseDAO.h"
#include "../entity/Stop.h"
#include <vector>
#include <optional>

namespace dao {

    class StopDAO : public BaseDAO {
    public:
        long long insert(const entity::Stop& s) {
            std::string sql = "INSERT INTO STOP (category, distance, flag, command) VALUES ("
                + itos(s.category) + ", "
                + ftos(s.distance) + ", "
                + itos(s.flag) + ", "
                + itos(s.command) + ")";
            LOG_INFO("StopDAO", "Insert stop record");
            return db().insertAndGetId(sql);
        }

        std::optional<entity::Stop> findById(long long id) {
            std::string sql = "SELECT * FROM STOP WHERE id = " + lltos(id);
            auto rows = db().query(sql);
            if (rows.empty()) return std::nullopt;
            return mapRow(rows[0]);
        }

        std::vector<entity::Stop> findAll() {
            return mapRows(db().query("SELECT * FROM STOP ORDER BY id DESC"));
        }

        // 查询允许停机的记录
        std::vector<entity::Stop> findAllowed() {
            return mapRows(db().query("SELECT * FROM STOP WHERE flag = 1 ORDER BY id DESC"));
        }

        // 查询已下发停机命令的记录
        std::vector<entity::Stop> findCommanded() {
            return mapRows(db().query("SELECT * FROM STOP WHERE command = 1 ORDER BY id DESC"));
        }

        // 下发停机命令
        int issueCommand(long long id) {
            std::string sql = "UPDATE STOP SET command = 1 WHERE id = " + lltos(id);
            LOG_INFO("StopDAO", "Issue stop command id=" + lltos(id));
            return db().execute(sql);
        }

        // 更新停机标志
        int updateFlag(long long id, int flag) {
            std::string sql = "UPDATE STOP SET flag = " + itos(flag) + " WHERE id = " + lltos(id);
            LOG_INFO("StopDAO", "Update stop flag id=" + lltos(id));
            return db().execute(sql);
        }

        int deleteById(long long id) {
            std::string sql = "DELETE FROM STOP WHERE id = " + lltos(id);
            LOG_INFO("StopDAO", "Delete stop id=" + lltos(id));
            return db().execute(sql);
        }

        int count() {
            auto rows = db().query("SELECT COUNT(*) AS cnt FROM STOP");
            return rows.empty() ? 0 : getInt(rows[0], "cnt");
        }

    private:
        entity::Stop mapRow(const db::Row& row) {
            entity::Stop s;
            s.id = getLong(row, "id");
            s.category = getInt(row, "category");
            s.distance = getFloat(row, "distance");
            s.flag = getInt(row, "flag");
            s.command = getInt(row, "command");
            return s;
        }

        std::vector<entity::Stop> mapRows(const db::ResultSet& rows) {
            std::vector<entity::Stop> result;
            result.reserve(rows.size());
            for (auto& r : rows) result.push_back(mapRow(r));
            return result;
        }
    };

} // namespace dao
