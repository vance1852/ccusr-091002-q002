#pragma once

#include "BaseDAO.h"
#include "../entity/Splice.h"
#include <vector>
#include <optional>

namespace dao {

    class SpliceDAO : public BaseDAO {
    public:
        int insert(const entity::Splice& s) {
            std::string sql = "INSERT INTO SPLICE (location, distance, time, url, last, flag, stop, window_id) VALUES ("
                + ftos(s.location) + ", "
                + ftos(s.distance) + ", "
                + esc(s.time) + ", "
                + esc(s.url) + ", "
                + itos(s.last) + ", "
                + itos(s.flag) + ", "
                + itos(s.stop) + ", "
                + windowIdSql(s.windowId) + ")";
            LOG_INFO("SpliceDAO", "Insert splice record windowId=" + itos(s.windowId));
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

        // 查询某窗口内的全部接缝（含窗口关闭后的历史数据，供复盘）
        std::vector<entity::Splice> findByWindow(int windowId) {
            return mapRows(db().query(
                "SELECT * FROM SPLICE WHERE window_id = " + itos(windowId) + " ORDER BY id DESC"));
        }

        // 查询当前有效接头：
        // 仅返回所在窗口处于 OPEN 的接缝；PAUSED/CLOSED 窗口内的记录被排除。
        // window_id 为 NULL 的是未纳入窗口管理的历史数据，保持既有行为继续返回。
        std::vector<entity::Splice> findActive() {
            return mapRows(db().query(
                "SELECT s.* FROM SPLICE s "
                "LEFT JOIN SPLICE_WINDOW w ON s.window_id = w.id "
                "WHERE s.last = 1 AND (s.window_id IS NULL OR w.status = 'OPEN') "
                "ORDER BY s.id DESC"));
        }

        // 查询准备停机的接缝（窗口必须 OPEN）
        std::vector<entity::Splice> findReadyToStop() {
            return mapRows(db().query(
                "SELECT s.* FROM SPLICE s "
                "LEFT JOIN SPLICE_WINDOW w ON s.window_id = w.id "
                "WHERE s.flag != 0 AND (s.window_id IS NULL OR w.status = 'OPEN') "
                "ORDER BY s.id DESC"));
        }

        // 查询可停机的接缝（窗口必须 OPEN）
        std::vector<entity::Splice> findStoppable() {
            return mapRows(db().query(
                "SELECT s.* FROM SPLICE s "
                "LEFT JOIN SPLICE_WINDOW w ON s.window_id = w.id "
                "WHERE s.stop != 0 AND (s.window_id IS NULL OR w.status = 'OPEN') "
                "ORDER BY s.id DESC"));
        }

        int updateFlags(int id, int flag, int stop) {
            std::string sql = "UPDATE SPLICE SET flag = " + itos(flag)
                + ", stop = " + itos(stop)
                + " WHERE id = " + itos(id);
            LOG_INFO("SpliceDAO", "Update splice flags id=" + itos(id)
                + " flag=" + itos(flag) + " stop=" + itos(stop));
            return db().execute(sql);
        }

        int updateLast(int id, int last) {
            std::string sql = "UPDATE SPLICE SET last = " + itos(last) + " WHERE id = " + itos(id);
            LOG_INFO("SpliceDAO", "Update splice last id=" + itos(id));
            return db().execute(sql);
        }

        // 清除当前检测中的 last 标志。
        // 只作用于 OPEN 窗口及未纳入窗口管理的记录；PAUSED/CLOSED 窗口不被改写
        // （关闭窗口的状态已定格，批量清理也不能穿透窗口约束）。
        // 注意：分两步执行，UPDATE 语句本身不引用 SPLICE_WINDOW，
        // 否则快照触发器更新 SPLICE_WINDOW 会触发 MySQL 1442 限制。
        int clearAllLast() {
            LOG_INFO("SpliceDAO", "Clear last flags in OPEN windows");
            auto rows = db().query("SELECT id FROM SPLICE_WINDOW WHERE status = 'OPEN'");
            std::string sql = "UPDATE SPLICE SET last = 0 WHERE last = 1 AND (window_id IS NULL";
            if (!rows.empty()) {
                sql += " OR window_id IN (";
                for (size_t i = 0; i < rows.size(); ++i) {
                    if (i) sql += ",";
                    sql += getVal(rows[i], "id");
                }
                sql += ")";
            }
            sql += ")";
            return db().execute(sql);
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
        static std::string windowIdSql(int windowId) {
            return windowId > 0 ? itos(windowId) : "NULL";
        }

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
            s.windowId = getInt(row, "window_id");
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
