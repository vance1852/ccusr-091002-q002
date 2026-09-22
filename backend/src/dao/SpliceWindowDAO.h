#pragma once

#include "BaseDAO.h"
#include "../entity/SpliceWindow.h"
#include <vector>
#include <optional>

namespace dao {

    // 关闭窗口的结果。重复关闭/并发落败时 closedNow=false，
    // 但仍返回与第一次关闭完全相同的 closedBy / closedAt（幂等）。
    struct WindowCloseResult {
        int windowId = 0;
        bool closedNow = false;       // true=本次调用真正完成了关闭（先关先得）
        std::string status;           // 调用后窗口状态
        std::string closedBy;         // 实际关窗人（重复关闭时仍是首个关窗人）
        std::string closedAt;         // 实际关窗时间
    };

    class SpliceWindowDAO : public BaseDAO {
    public:
        // 开启一个检测窗口（初始状态 OPEN，记录 OPENED 事件由数据库触发器完成）
        int openWindow(const std::string& shiftCode, const std::string& oper) {
            std::string sql = "INSERT INTO SPLICE_WINDOW (shift_code, operator, status) VALUES ("
                + esc(shiftCode) + ", " + esc(oper) + ", 'OPEN')";
            LOG_INFO("SpliceWindowDAO", "Open window shift=" + shiftCode + " operator=" + oper);
            return static_cast<int>(db().insertAndGetId(sql));
        }

        std::optional<entity::SpliceWindow> findById(int id) {
            auto rows = db().query("SELECT * FROM SPLICE_WINDOW WHERE id = " + itos(id));
            if (rows.empty()) return std::nullopt;
            return mapRow(rows[0]);
        }

        std::vector<entity::SpliceWindow> findAll() {
            return mapRows(db().query("SELECT * FROM SPLICE_WINDOW ORDER BY id DESC"));
        }

        // 按班次查询窗口（跨班次交接时使用）
        std::vector<entity::SpliceWindow> findByShift(const std::string& shiftCode) {
            return mapRows(db().query(
                "SELECT * FROM SPLICE_WINDOW WHERE shift_code = " + esc(shiftCode) + " ORDER BY id DESC"));
        }

        // 查询当前唯一允许写入/检测的窗口（OPEN）。暂停窗口不参与当前检测。
        std::vector<entity::SpliceWindow> findOpen() {
            return mapRows(db().query("SELECT * FROM SPLICE_WINDOW WHERE status = 'OPEN' ORDER BY id DESC"));
        }

        // 暂停：仅 OPEN -> PAUSED 合法；其余状态由数据库触发器拒绝，这里给出可读错误
        int pauseWindow(int id, const std::string& oper) {
            int affected = db().execute(
                "UPDATE SPLICE_WINDOW SET status = 'PAUSED', actor = " + esc(oper)
                + " WHERE id = " + itos(id) + " AND status = 'OPEN'");
            if (affected == 0) rejectIfNotExpected(id, "OPEN", "pause (OPEN -> PAUSED)");
            LOG_INFO("SpliceWindowDAO", "Pause window id=" + itos(id) + " by " + oper);
            return affected;
        }

        // 恢复：仅 PAUSED -> OPEN 合法
        int resumeWindow(int id, const std::string& oper) {
            int affected = db().execute(
                "UPDATE SPLICE_WINDOW SET status = 'OPEN', actor = " + esc(oper)
                + " WHERE id = " + itos(id) + " AND status = 'PAUSED'");
            if (affected == 0) rejectIfNotExpected(id, "PAUSED", "resume (PAUSED -> OPEN)");
            LOG_INFO("SpliceWindowDAO", "Resume window id=" + itos(id) + " by " + oper);
            return affected;
        }

        // 关闭窗口（默认连接）。幂等：窗口已关闭时返回首次关闭的同一结果。
        WindowCloseResult closeWindow(int id, const std::string& oper) {
            return closeWindowOn(db(), id, oper);
        }

        // 在指定连接上关闭窗口：两名操作员使用各自连接并发关窗时，
        // 依靠行锁(FOR UPDATE)串行化——先关先得，后来者拿到同一结果。
        template <typename Conn>
        WindowCloseResult closeWindowOn(Conn& conn, int id, const std::string& oper) {
            WindowCloseResult result;
            result.windowId = id;
            conn.execute("START TRANSACTION");
            try {
                auto rows = conn.query(
                    "SELECT status, closed_by, closed_at FROM SPLICE_WINDOW "
                    "WHERE id = " + itos(id) + " FOR UPDATE");
                if (rows.empty()) {
                    conn.execute("ROLLBACK");
                    throw db::DatabaseException("Close window rejected: window id=" + itos(id) + " not found");
                }
                result.status = rows[0].at("status");
                result.closedBy = getVal(rows[0], "closed_by");
                result.closedAt = getVal(rows[0], "closed_at");

                if (result.status == "CLOSED") {
                    // 重复关闭或并发落败：不产生新的状态写入，原样返回首次关闭结果
                    conn.execute("COMMIT");
                    result.closedNow = false;
                    LOG_INFO("SpliceWindowDAO",
                        "Close window id=" + itos(id) + " by " + oper + " -> already closed by "
                        + result.closedBy + " at " + result.closedAt);
                    return result;
                }

                // OPEN / PAUSED -> CLOSED，合法性由 BEFORE UPDATE 触发器兜底
                conn.execute(
                    "UPDATE SPLICE_WINDOW SET status = 'CLOSED', closed_by = " + esc(oper)
                    + ", actor = " + esc(oper) + " WHERE id = " + itos(id));
                conn.execute("COMMIT");

                // 在同一连接上回读真实的关闭时间与关窗人，避免并发时与其他连接混用
                auto closedRows = conn.query(
                    "SELECT closed_by, closed_at FROM SPLICE_WINDOW WHERE id = " + itos(id));
                result.status = "CLOSED";
                result.closedNow = true;
                if (!closedRows.empty()) {
                    result.closedBy = getVal(closedRows[0], "closed_by");
                    result.closedAt = getVal(closedRows[0], "closed_at");
                } else {
                    result.closedBy = oper;
                }
                LOG_INFO("SpliceWindowDAO",
                    "Close window id=" + itos(id) + " by " + oper + " at " + result.closedAt);
                return result;
            } catch (...) {
                conn.execute("ROLLBACK");
                throw;
            }
        }

        // 窗口生命周期轨迹（开启/暂停/恢复/关闭），供复盘
        std::vector<entity::SpliceWindowEvent> findEvents(int windowId) {
            auto rows = db().query(
                "SELECT * FROM SPLICE_WINDOW_EVENT WHERE window_id = " + itos(windowId)
                + " ORDER BY id ASC");
            std::vector<entity::SpliceWindowEvent> out;
            out.reserve(rows.size());
            for (auto& r : rows) {
                entity::SpliceWindowEvent e;
                e.id = getLong(r, "id");
                e.windowId = getInt(r, "window_id");
                e.event = getVal(r, "event");
                e.eventAt = getVal(r, "event_at");
                e.oper = getVal(r, "operator");
                out.push_back(std::move(e));
            }
            return out;
        }

        int count() {
            auto rows = db().query("SELECT COUNT(*) AS cnt FROM SPLICE_WINDOW");
            return rows.empty() ? 0 : getInt(rows[0], "cnt");
        }

    private:
        void rejectIfNotExpected(int id, const std::string& expected, const std::string& action) {
            auto w = findById(id);
            if (!w) {
                throw db::DatabaseException("Window id=" + itos(id) + " not found, cannot " + action);
            }
            throw db::DatabaseException(
                "Illegal window transition: cannot " + action + " when status is " + w->status
                + " (requires " + expected + "); window id=" + itos(id));
        }

        entity::SpliceWindow mapRow(const db::Row& row) {
            entity::SpliceWindow w;
            w.id = getInt(row, "id");
            w.shiftCode = getVal(row, "shift_code");
            w.oper = getVal(row, "operator");
            w.status = getVal(row, "status");
            w.openedAt = getVal(row, "opened_at");
            w.pausedAt = getVal(row, "paused_at");
            w.resumedAt = getVal(row, "resumed_at");
            w.closedAt = getVal(row, "closed_at");
            w.closedBy = getVal(row, "closed_by");
            w.actor = getVal(row, "actor");
            w.lastStatus = getVal(row, "last_status");
            w.lastStatusAt = getVal(row, "last_status_at");
            w.lastSpliceId = getInt(row, "last_splice_id");
            return w;
        }

        std::vector<entity::SpliceWindow> mapRows(const db::ResultSet& rows) {
            std::vector<entity::SpliceWindow> out;
            out.reserve(rows.size());
            for (auto& r : rows) out.push_back(mapRow(r));
            return out;
        }
    };

} // namespace dao
