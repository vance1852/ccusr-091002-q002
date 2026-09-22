#pragma once

#include "BaseDAO.h"
#include "../entity/Splice.h"
#include "../entity/SpliceWindow.h"
#include <vector>
#include <optional>
#include <stdexcept>

namespace dao {

    // 窗口状态约束被违反时抛出：非法迁移、窗口未开启时的写入、窗口不存在等
    class WindowStateException : public std::runtime_error {
    public:
        explicit WindowStateException(const std::string& msg) : std::runtime_error(msg) {}
    };

    class SpliceDAO : public BaseDAO {
    public:
        SpliceDAO() = default;
        // 绑定独立连接的构造（两名操作员并发操作等场景）
        explicit SpliceDAO(db::DatabaseManager* conn) { useConnection(conn); }

        // ============================================
        // 既有接缝读取接口（签名与语义保持不变）
        // findById / findAll 不过滤窗口，已关闭窗口的
        // 最后有效状态仍可读取，供交接复盘
        // ============================================

        int insert(const entity::Splice& s) {
            // 窗口校验：仅 OPEN 窗口可写；windowId 为 0 时自动归属当前 OPEN 窗口
            int windowId = resolveWritableWindow(s.windowId);
            std::string sql = "INSERT INTO SPLICE (window_id, location, distance, time, url, last, flag, stop) VALUES ("
                + itos(windowId) + ", "
                + ftos(s.location) + ", "
                + ftos(s.distance) + ", "
                + esc(s.time) + ", "
                + esc(s.url) + ", "
                + itos(s.last) + ", "
                + itos(s.flag) + ", "
                + itos(s.stop) + ")";
            LOG_INFO("SpliceDAO", "Insert splice record into window " + itos(windowId));
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

        // 查询当前有效接头（仅返回 OPEN 窗口内的数据）
        std::vector<entity::Splice> findActive() {
            return mapRows(db().query(openWindowScope() + "WHERE S.last = 1 ORDER BY S.id DESC"));
        }

        // 查询准备停机的接缝（仅返回 OPEN 窗口内的数据）
        std::vector<entity::Splice> findReadyToStop() {
            return mapRows(db().query(openWindowScope() + "WHERE S.flag != 0 ORDER BY S.id DESC"));
        }

        // 查询可停机的接缝（仅返回 OPEN 窗口内的数据）
        std::vector<entity::Splice> findStoppable() {
            return mapRows(db().query(openWindowScope() + "WHERE S.stop != 0 ORDER BY S.id DESC"));
        }

        int updateFlags(int id, int flag, int stop) {
            std::string sql = "UPDATE SPLICE S JOIN SPLICE_WINDOW W ON W.id = S.window_id"
                " SET S.flag = " + itos(flag) + ", S.stop = " + itos(stop)
                + " WHERE S.id = " + itos(id) + " AND W.state = 'OPEN'";
            LOG_INFO("SpliceDAO", "Update splice flags id=" + itos(id));
            return guardedUpdate(sql, id);
        }

        int updateLast(int id, int last) {
            std::string sql = "UPDATE SPLICE S JOIN SPLICE_WINDOW W ON W.id = S.window_id"
                " SET S.last = " + itos(last)
                + " WHERE S.id = " + itos(id) + " AND W.state = 'OPEN'";
            LOG_INFO("SpliceDAO", "Update splice last id=" + itos(id));
            return guardedUpdate(sql, id);
        }

        // 清除所有 last 标志（仅 OPEN 窗口内的记录可写）
        int clearAllLast() {
            LOG_INFO("SpliceDAO", "Clear all last flags");
            return db().execute("UPDATE SPLICE S JOIN SPLICE_WINDOW W ON W.id = S.window_id"
                " SET S.last = 0 WHERE S.last = 1 AND W.state = 'OPEN'");
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

        // ============================================
        // 检测窗口生命周期：开启 -> 暂停 -> 恢复 -> 关闭
        // ============================================

        // 开启窗口。同一时刻只允许一个 OPEN 窗口（数据库唯一索引兜底）
        int openWindow(const std::string& label, const std::string& actor) {
            auto cur = findCurrentWindow();
            if (cur.has_value()) {
                throw WindowStateException("open rejected: window " + itos(cur->id)
                    + " (" + cur->label + ") is still OPEN");
            }
            db().beginTransaction();
            try {
                long long id = db().insertAndGetId(
                    "INSERT INTO SPLICE_WINDOW (label) VALUES (" + esc(label) + ")");
                insertEvent(static_cast<int>(id), "OPEN", "-", "OPEN", actor);
                db().commit();
                LOG_INFO("SpliceDAO", "Open window " + std::to_string(id) + " label=" + label);
                return static_cast<int>(id);
            } catch (const db::DatabaseException& e) {
                db().rollback();
                // 并发开窗撞唯一索引：翻译为窗口状态异常
                if (std::string(e.what()).find("uq_window_open_singleton") != std::string::npos) {
                    throw WindowStateException(std::string("open rejected: another window is OPEN (") + e.what() + ")");
                }
                throw;
            } catch (...) {
                db().rollback();
                throw;
            }
        }

        // 暂停窗口：仅 OPEN -> PAUSED 合法
        bool pauseWindow(int windowId, const std::string& actor) {
            return transition(windowId, "PAUSE", entity::WindowState::OPEN, entity::WindowState::PAUSED, actor);
        }

        // 恢复窗口：仅 PAUSED -> OPEN 合法
        bool resumeWindow(int windowId, const std::string& actor) {
            return transition(windowId, "RESUME", entity::WindowState::PAUSED, entity::WindowState::OPEN, actor);
        }

        // 关闭窗口：幂等。首次关闭写入 CLOSED 与 closed_at；
        // 重复关闭（含并发关闭）返回同一结果——相同的 closed_at 与 version
        entity::SpliceWindow closeWindow(int windowId, const std::string& actor) {
            db().beginTransaction();
            try {
                auto rows = db().query("SELECT state FROM SPLICE_WINDOW WHERE id = "
                    + itos(windowId) + " FOR UPDATE");
                if (rows.empty()) {
                    throw WindowStateException("close rejected: window " + itos(windowId) + " not found");
                }
                std::string from = getVal(rows[0], "state");
                if (from != "CLOSED") {
                    db().execute("UPDATE SPLICE_WINDOW SET state = 'CLOSED',"
                        " closed_at = NOW(3), version = version + 1 WHERE id = " + itos(windowId));
                    insertEvent(windowId, "CLOSE", from, "CLOSED", actor);
                    LOG_INFO("SpliceDAO", "Close window " + itos(windowId) + " by " + actor);
                } else {
                    LOG_INFO("SpliceDAO", "Window " + itos(windowId) + " already CLOSED, return same result");
                }
                db().commit();
            } catch (...) {
                db().rollback();
                throw;
            }
            auto w = findWindow(windowId);
            return *w;  // 上面已确认存在
        }

        std::optional<entity::SpliceWindow> findWindow(int windowId) {
            auto rows = db().query("SELECT id, label, state, opened_at, closed_at, version"
                " FROM SPLICE_WINDOW WHERE id = " + itos(windowId));
            if (rows.empty()) return std::nullopt;
            return mapWindow(rows[0]);
        }

        // 当前 OPEN 窗口（数据库保证至多一个）
        std::optional<entity::SpliceWindow> findCurrentWindow() {
            auto rows = db().query("SELECT id, label, state, opened_at, closed_at, version"
                " FROM SPLICE_WINDOW WHERE state = 'OPEN' LIMIT 1");
            if (rows.empty()) return std::nullopt;
            return mapWindow(rows[0]);
        }

        // 窗口状态迁移审计（开启/暂停/恢复/关闭的时间与操作员）
        std::vector<entity::SpliceWindowEvent> windowEvents(int windowId) {
            auto rows = db().query("SELECT * FROM SPLICE_WINDOW_EVENT WHERE window_id = "
                + itos(windowId) + " ORDER BY id ASC");
            std::vector<entity::SpliceWindowEvent> result;
            result.reserve(rows.size());
            for (auto& r : rows) {
                entity::SpliceWindowEvent ev;
                ev.id = getLong(r, "id");
                ev.windowId = getInt(r, "window_id");
                ev.action = getVal(r, "action");
                ev.fromState = getVal(r, "from_state");
                ev.toState = getVal(r, "to_state");
                ev.actor = getVal(r, "actor");
                ev.createdAt = getVal(r, "created_at");
                result.push_back(std::move(ev));
            }
            return result;
        }

        // 复盘视图：全部接缝及其所在窗口状态，
        // 接班人可直接看出记录为何出现或被运行查询排除
        struct SpliceWindowView {
            entity::Splice splice;
            int windowId = 0;
            std::string windowLabel;
            entity::WindowState windowState = entity::WindowState::OPEN;
            // 是否会被当前接头/准备停机/可停机查询返回
            bool visibleInOpenQueries() const { return windowState == entity::WindowState::OPEN; }
        };

        std::vector<SpliceWindowView> findAllWithWindow() {
            auto rows = db().query(
                "SELECT S.*, W.label AS window_label, W.state AS window_state"
                " FROM SPLICE S JOIN SPLICE_WINDOW W ON W.id = S.window_id"
                " ORDER BY S.id DESC");
            std::vector<SpliceWindowView> result;
            result.reserve(rows.size());
            for (auto& r : rows) {
                SpliceWindowView v;
                v.splice = mapRow(r);
                v.windowId = getInt(r, "window_id");
                v.windowLabel = getVal(r, "window_label");
                v.windowState = entity::windowStateFrom(getVal(r, "window_state"));
                result.push_back(std::move(v));
            }
            return result;
        }

    private:
        // 运行查询的窗口作用域：只返回 OPEN 窗口允许出现的数据
        static std::string openWindowScope() {
            return "SELECT S.* FROM SPLICE S JOIN SPLICE_WINDOW W"
                " ON W.id = S.window_id AND W.state = 'OPEN' ";
        }

        // 解析可写窗口：显式 windowId 必须为 OPEN；为 0 时归属当前 OPEN 窗口
        int resolveWritableWindow(int windowId) {
            if (windowId <= 0) {
                auto cur = findCurrentWindow();
                if (!cur.has_value()) {
                    throw WindowStateException("insert rejected: no OPEN window");
                }
                return cur->id;
            }
            auto w = findWindow(windowId);
            if (!w.has_value()) {
                throw WindowStateException("insert rejected: window " + itos(windowId) + " not found");
            }
            if (w->state != entity::WindowState::OPEN) {
                throw WindowStateException("insert rejected: window " + itos(windowId)
                    + " is " + entity::toString(w->state));
            }
            return windowId;
        }

        // 受窗口守卫的更新：记录存在但窗口非 OPEN 时拒绝（迟到写入）
        int guardedUpdate(const std::string& sql, int id) {
            int affected = db().execute(sql);
            if (affected == 0) {
                auto s = findById(id);
                if (s.has_value()) {
                    auto w = findWindow(s->windowId);
                    if (w.has_value() && w->state != entity::WindowState::OPEN) {
                        throw WindowStateException("write rejected: window " + itos(s->windowId)
                            + " of splice " + itos(id) + " is " + entity::toString(w->state));
                    }
                }
            }
            return affected;
        }

        // 窗口状态迁移：行锁内校验前置状态，非法迁移抛异常，合法迁移落审计
        bool transition(int windowId, const char* action,
                        entity::WindowState from, entity::WindowState to,
                        const std::string& actor) {
            db().beginTransaction();
            try {
                auto rows = db().query("SELECT state FROM SPLICE_WINDOW WHERE id = "
                    + itos(windowId) + " FOR UPDATE");
                if (rows.empty()) {
                    throw WindowStateException(std::string(action) + " rejected: window "
                        + itos(windowId) + " not found");
                }
                std::string cur = getVal(rows[0], "state");
                if (cur != entity::toString(from)) {
                    throw WindowStateException(std::string("illegal transition ") + action
                        + ": window " + itos(windowId) + " is " + cur
                        + ", requires " + entity::toString(from));
                }
                db().execute("UPDATE SPLICE_WINDOW SET state = " + esc(entity::toString(to))
                    + ", version = version + 1 WHERE id = " + itos(windowId));
                insertEvent(windowId, action, cur, entity::toString(to), actor);
                db().commit();
                LOG_INFO("SpliceDAO", std::string(action) + " window " + itos(windowId) + " by " + actor);
                return true;
            } catch (...) {
                db().rollback();
                throw;
            }
        }

        void insertEvent(int windowId, const std::string& action,
                         const std::string& from, const std::string& to,
                         const std::string& actor) {
            db().execute("INSERT INTO SPLICE_WINDOW_EVENT (window_id, action, from_state, to_state, actor) VALUES ("
                + itos(windowId) + ", " + esc(action) + ", " + esc(from) + ", " + esc(to) + ", " + esc(actor) + ")");
        }

        entity::SpliceWindow mapWindow(const db::Row& row) {
            entity::SpliceWindow w;
            w.id = getInt(row, "id");
            w.label = getVal(row, "label");
            w.state = entity::windowStateFrom(getVal(row, "state"));
            w.openedAt = getVal(row, "opened_at");
            w.closedAt = getVal(row, "closed_at");  // 未关闭时为空串
            w.version = getInt(row, "version");
            return w;
        }

        entity::Splice mapRow(const db::Row& row) {
            entity::Splice s;
            s.id = getInt(row, "id");
            s.windowId = getInt(row, "window_id");
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
