#pragma once

#include "BaseDAO.h"
#include "../entity/Speed.h"
#include <vector>
#include <optional>

namespace dao {

    class SpeedDAO : public BaseDAO {
    public:
        // 插入速度记录，返回自增ID
        int insert(const entity::Speed& speed) {
            std::string sql = "INSERT INTO SPEED (value, date, flag) VALUES ("
                + ftos(speed.value) + ", "
                + esc(speed.date) + ", "
                + itos(speed.flag) + ")";
            LOG_INFO("SpeedDAO", "Insert speed record");
            return static_cast<int>(db().insertAndGetId(sql));
        }

        // 根据ID查询
        std::optional<entity::Speed> findById(int id) {
            std::string sql = "SELECT * FROM SPEED WHERE id = " + itos(id);
            auto rows = db().query(sql);
            if (rows.empty()) return std::nullopt;
            return mapRow(rows[0]);
        }

        // 查询所有记录
        std::vector<entity::Speed> findAll() {
            auto rows = db().query("SELECT * FROM SPEED ORDER BY id DESC");
            return mapRows(rows);
        }

        // 按日期查询
        std::vector<entity::Speed> findByDate(const std::string& date) {
            std::string sql = "SELECT * FROM SPEED WHERE date = " + esc(date) + " ORDER BY id DESC";
            return mapRows(db().query(sql));
        }

        // 查询未使用的记录
        std::vector<entity::Speed> findUnused() {
            return mapRows(db().query("SELECT * FROM SPEED WHERE flag = 0 ORDER BY id DESC"));
        }

        // 更新速度值
        int updateValue(int id, float value) {
            std::string sql = "UPDATE SPEED SET value = " + ftos(value) + " WHERE id = " + itos(id);
            LOG_INFO("SpeedDAO", "Update speed id=" + itos(id));
            return db().execute(sql);
        }

        // 标记为已使用
        int markUsed(int id) {
            std::string sql = "UPDATE SPEED SET flag = 1 WHERE id = " + itos(id);
            LOG_INFO("SpeedDAO", "Mark speed used id=" + itos(id));
            return db().execute(sql);
        }

        // 删除记录
        int deleteById(int id) {
            std::string sql = "DELETE FROM SPEED WHERE id = " + itos(id);
            LOG_INFO("SpeedDAO", "Delete speed id=" + itos(id));
            return db().execute(sql);
        }

        // 获取记录总数
        int count() {
            auto rows = db().query("SELECT COUNT(*) AS cnt FROM SPEED");
            return rows.empty() ? 0 : getInt(rows[0], "cnt");
        }

    private:
        entity::Speed mapRow(const db::Row& row) {
            entity::Speed s;
            s.id = getInt(row, "id");
            s.value = getFloat(row, "value");
            s.date = getVal(row, "date");
            s.flag = getInt(row, "flag");
            return s;
        }

        std::vector<entity::Speed> mapRows(const db::ResultSet& rows) {
            std::vector<entity::Speed> result;
            result.reserve(rows.size());
            for (auto& row : rows) {
                result.push_back(mapRow(row));
            }
            return result;
        }
    };

} // namespace dao
