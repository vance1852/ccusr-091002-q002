/**
 * 工业检测系统 - 完整测试套件
 * 覆盖: AppConfig, Logger, DatabaseManager, BaseDAO, 全部7个DAO
 * 
 * 每个测试用例在执行前清理相关表数据，确保测试隔离
 */

#ifdef _WIN32
#include <winsock2.h>
#endif

#include "TestFramework.h"
#include "../config/AppConfig.h"
#include "../utils/Logger.h"
#include "../db/DatabaseManager.h"
#include "../dao/SpeedDAO.h"
#include "../dao/SpliceDAO.h"
#include "../dao/SpliceWindowDAO.h"
#include "../dao/FlawDAO.h"
#include "../dao/StopDAO.h"
#include "../dao/CompareDAO.h"
#include "../dao/HistoryDAO.h"
#include "../dao/RemoveDAO.h"

#include <cstdlib>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>

// ============================================
// 辅助：清理所有表
// ============================================
static void cleanAllTables() {
    auto& dbm = db::DatabaseManager::instance();
    dbm.execute("DELETE FROM SPLICE");
    dbm.execute("DELETE FROM SPLICE_WINDOW_EVENT");
    dbm.execute("DELETE FROM SPLICE_WINDOW");
    dbm.execute("DELETE FROM SPEED");
    dbm.execute("DELETE FROM FLAW");
    dbm.execute("DELETE FROM STOP");
    dbm.execute("DELETE FROM COMPARE");
    dbm.execute("DELETE FROM HISTORY");
    dbm.execute("DELETE FROM REMOVE");
}

// ============================================
// 1. AppConfig 测试
// ============================================
TEST(AppConfig_DefaultValues) {
    config::DatabaseConfig cfg;
    ASSERT_STR_EQ(cfg.host, "127.0.0.1");
    ASSERT_EQ(cfg.port, 3306u);
    ASSERT_STR_EQ(cfg.user, "root");
    ASSERT_STR_EQ(cfg.password, "root");
    ASSERT_STR_EQ(cfg.database, "industrial_inspection");
    ASSERT_STR_EQ(cfg.charset, "utf8mb4");
    ASSERT_EQ(cfg.connectTimeout, 10u);
    ASSERT_TRUE(cfg.autoReconnect);
}

TEST(AppConfig_LoadFromEnv) {
    // 注意：测试环境中已设置了 DB_HOST 等环境变量
    config::DatabaseConfig cfg;
    cfg.loadFromEnv();
    // loadFromEnv 应该能正常执行不崩溃
    // 具体值取决于环境变量，这里只验证函数可调用
    ASSERT_FALSE(cfg.host.empty());
    ASSERT_GT(cfg.port, 0u);
}

// ============================================
// 2. Logger 测试
// ============================================
TEST(Logger_AllLevels) {
    auto& logger = utils::Logger::instance();
    // 设置为 DEBUG 级别，所有日志都应输出
    logger.setLevel(utils::LogLevel::DEBUG);
    logger.debug("Test", "debug message");
    logger.info("Test", "info message");
    logger.warn("Test", "warn message");
    logger.error("Test", "error message");
    // 不崩溃即通过
    ASSERT_TRUE(true);
}

TEST(Logger_LevelFilter) {
    auto& logger = utils::Logger::instance();
    // 设置为 ERROR 级别，低级别日志不应输出
    logger.setLevel(utils::LogLevel::ERROR);
    logger.debug("Test", "should not appear");
    logger.info("Test", "should not appear");
    logger.warn("Test", "should not appear");
    logger.error("Test", "should appear");
    // 恢复
    logger.setLevel(utils::LogLevel::INFO);
    ASSERT_TRUE(true);
}

// ============================================
// 3. DatabaseManager 测试
// ============================================
TEST(DatabaseManager_IsConnected) {
    ASSERT_TRUE(db::DatabaseManager::instance().isConnected());
}

TEST(DatabaseManager_Escape) {
    auto& dbm = db::DatabaseManager::instance();
    std::string escaped = dbm.escape("test'string\"with\\special");
    // 转义后不应包含未转义的单引号
    ASSERT_TRUE(escaped.find("'") == std::string::npos || escaped.find("\\'") != std::string::npos);
    ASSERT_FALSE(escaped.empty());
}

TEST(DatabaseManager_ExecuteAndQuery) {
    auto& dbm = db::DatabaseManager::instance();
    dbm.execute("DELETE FROM SPEED");
    dbm.execute("INSERT INTO SPEED (value, date, flag) VALUES (100.0, '2026-01-01', 0)");
    auto rows = dbm.query("SELECT * FROM SPEED WHERE date = '2026-01-01'");
    ASSERT_EQ(rows.size(), (size_t)1);
    ASSERT_STR_EQ(rows[0].at("date"), "2026-01-01");
    dbm.execute("DELETE FROM SPEED");
}

TEST(DatabaseManager_InsertAndGetId) {
    auto& dbm = db::DatabaseManager::instance();
    dbm.execute("DELETE FROM SPEED");
    long long id = dbm.insertAndGetId("INSERT INTO SPEED (value, date, flag) VALUES (50.0, '2026-02-01', 0)");
    ASSERT_GT(id, 0LL);
    dbm.execute("DELETE FROM SPEED");
}

TEST(DatabaseManager_QueryEmptyResult) {
    auto& dbm = db::DatabaseManager::instance();
    auto rows = dbm.query("SELECT * FROM SPEED WHERE id = -999");
    ASSERT_EQ(rows.size(), (size_t)0);
}

TEST(DatabaseManager_ExecuteInvalidSQL) {
    auto& dbm = db::DatabaseManager::instance();
    ASSERT_THROWS(dbm.execute("INVALID SQL STATEMENT HERE"), db::DatabaseException);
}

TEST(DatabaseManager_QueryInvalidSQL) {
    auto& dbm = db::DatabaseManager::instance();
    ASSERT_THROWS(dbm.query("SELECT * FROM NON_EXISTENT_TABLE_XYZ"), db::DatabaseException);
}

TEST(DatabaseManager_InsertInvalidSQL) {
    auto& dbm = db::DatabaseManager::instance();
    ASSERT_THROWS(dbm.insertAndGetId("INSERT INTO NON_EXISTENT_TABLE_XYZ VALUES (1)"), db::DatabaseException);
}

// ============================================
// 4. SpeedDAO 测试 (9 methods)
// ============================================
TEST(SpeedDAO_InsertAndFindById) {
    cleanAllTables();
    dao::SpeedDAO dao;
    entity::Speed s;
    s.value = 120.5f;
    s.date = "2026-02-24";
    s.flag = 0;
    int id = dao.insert(s);
    ASSERT_GT(id, 0);

    auto found = dao.findById(id);
    ASSERT_TRUE(found.has_value());
    ASSERT_EQ(found->id, id);
    ASSERT_FLOAT_EQ(found->value, 120.5f);
    ASSERT_STR_EQ(found->date, "2026-02-24");
    ASSERT_EQ(found->flag, 0);
}

TEST(SpeedDAO_FindByIdNotFound) {
    dao::SpeedDAO dao;
    auto found = dao.findById(-999);
    ASSERT_FALSE(found.has_value());
}

TEST(SpeedDAO_FindAll) {
    cleanAllTables();
    dao::SpeedDAO dao;
    entity::Speed s1; s1.value = 100.0f; s1.date = "2026-01-01"; s1.flag = 0;
    entity::Speed s2; s2.value = 200.0f; s2.date = "2026-01-02"; s2.flag = 0;
    dao.insert(s1);
    dao.insert(s2);
    auto all = dao.findAll();
    ASSERT_GE(all.size(), (size_t)2);
}

TEST(SpeedDAO_FindByDate) {
    cleanAllTables();
    dao::SpeedDAO dao;
    entity::Speed s; s.value = 150.0f; s.date = "2026-03-15"; s.flag = 0;
    dao.insert(s);
    auto results = dao.findByDate("2026-03-15");
    ASSERT_EQ(results.size(), (size_t)1);
    ASSERT_FLOAT_EQ(results[0].value, 150.0f);
}

TEST(SpeedDAO_FindUnused) {
    cleanAllTables();
    dao::SpeedDAO dao;
    entity::Speed s1; s1.value = 10.0f; s1.date = "2026-01-01"; s1.flag = 0;
    entity::Speed s2; s2.value = 20.0f; s2.date = "2026-01-02"; s2.flag = 1;
    dao.insert(s1);
    dao.insert(s2);
    auto unused = dao.findUnused();
    ASSERT_EQ(unused.size(), (size_t)1);
    ASSERT_FLOAT_EQ(unused[0].value, 10.0f);
}

TEST(SpeedDAO_UpdateValue) {
    cleanAllTables();
    dao::SpeedDAO dao;
    entity::Speed s; s.value = 100.0f; s.date = "2026-01-01"; s.flag = 0;
    int id = dao.insert(s);
    int affected = dao.updateValue(id, 999.9f);
    ASSERT_EQ(affected, 1);
    auto found = dao.findById(id);
    ASSERT_TRUE(found.has_value());
    ASSERT_FLOAT_EQ(found->value, 999.9f);
}

TEST(SpeedDAO_MarkUsed) {
    cleanAllTables();
    dao::SpeedDAO dao;
    entity::Speed s; s.value = 50.0f; s.date = "2026-01-01"; s.flag = 0;
    int id = dao.insert(s);
    int affected = dao.markUsed(id);
    ASSERT_EQ(affected, 1);
    auto found = dao.findById(id);
    ASSERT_TRUE(found.has_value());
    ASSERT_EQ(found->flag, 1);
}

TEST(SpeedDAO_DeleteById) {
    cleanAllTables();
    dao::SpeedDAO dao;
    entity::Speed s; s.value = 50.0f; s.date = "2026-01-01"; s.flag = 0;
    int id = dao.insert(s);
    int affected = dao.deleteById(id);
    ASSERT_EQ(affected, 1);
    auto found = dao.findById(id);
    ASSERT_FALSE(found.has_value());
}

TEST(SpeedDAO_Count) {
    cleanAllTables();
    dao::SpeedDAO dao;
    ASSERT_EQ(dao.count(), 0);
    entity::Speed s; s.value = 1.0f; s.date = "2026-01-01"; s.flag = 0;
    dao.insert(s);
    dao.insert(s);
    ASSERT_EQ(dao.count(), 2);
}

// ============================================
// 5. SpliceDAO 测试 (11 methods)
// ============================================
TEST(SpliceDAO_InsertAndFindById) {
    cleanAllTables();
    dao::SpliceDAO dao;
    entity::Splice s;
    s.location = 1500.0f; s.distance = 320.5f; s.time = "45";
    s.url = "/data/splice/001.jpg"; s.last = 1; s.flag = 0; s.stop = 0;
    int id = dao.insert(s);
    ASSERT_GT(id, 0);

    auto found = dao.findById(id);
    ASSERT_TRUE(found.has_value());
    ASSERT_FLOAT_EQ(found->location, 1500.0f);
    ASSERT_FLOAT_EQ(found->distance, 320.5f);
    ASSERT_STR_EQ(found->time, "45");
    ASSERT_STR_EQ(found->url, "/data/splice/001.jpg");
    ASSERT_EQ(found->last, 1);
}

TEST(SpliceDAO_FindByIdNotFound) {
    dao::SpliceDAO dao;
    auto found = dao.findById(-999);
    ASSERT_FALSE(found.has_value());
}

TEST(SpliceDAO_FindAll) {
    cleanAllTables();
    dao::SpliceDAO dao;
    entity::Splice s; s.location = 100.0f; s.distance = 50.0f; s.time = "10";
    s.url = "/test"; s.last = 0; s.flag = 0; s.stop = 0;
    dao.insert(s);
    auto all = dao.findAll();
    ASSERT_GE(all.size(), (size_t)1);
}

TEST(SpliceDAO_FindActive) {
    cleanAllTables();
    dao::SpliceDAO dao;
    entity::Splice s1; s1.location = 100.0f; s1.distance = 50.0f; s1.time = "10";
    s1.url = "/a"; s1.last = 1; s1.flag = 0; s1.stop = 0;
    entity::Splice s2; s2.location = 200.0f; s2.distance = 60.0f; s2.time = "20";
    s2.url = "/b"; s2.last = 0; s2.flag = 0; s2.stop = 0;
    dao.insert(s1);
    dao.insert(s2);
    auto active = dao.findActive();
    ASSERT_EQ(active.size(), (size_t)1);
    ASSERT_FLOAT_EQ(active[0].location, 100.0f);
}

TEST(SpliceDAO_FindReadyToStop) {
    cleanAllTables();
    dao::SpliceDAO dao;
    entity::Splice s; s.location = 100.0f; s.distance = 50.0f; s.time = "10";
    s.url = "/a"; s.last = 0; s.flag = 1; s.stop = 0;
    dao.insert(s);
    auto ready = dao.findReadyToStop();
    ASSERT_EQ(ready.size(), (size_t)1);
}

TEST(SpliceDAO_FindStoppable) {
    cleanAllTables();
    dao::SpliceDAO dao;
    entity::Splice s; s.location = 100.0f; s.distance = 50.0f; s.time = "10";
    s.url = "/a"; s.last = 0; s.flag = 0; s.stop = 1;
    dao.insert(s);
    auto stoppable = dao.findStoppable();
    ASSERT_EQ(stoppable.size(), (size_t)1);
}

TEST(SpliceDAO_UpdateFlags) {
    cleanAllTables();
    dao::SpliceDAO dao;
    entity::Splice s; s.location = 100.0f; s.distance = 50.0f; s.time = "10";
    s.url = "/a"; s.last = 0; s.flag = 0; s.stop = 0;
    int id = dao.insert(s);
    int affected = dao.updateFlags(id, 1, 1);
    ASSERT_EQ(affected, 1);
    auto found = dao.findById(id);
    ASSERT_EQ(found->flag, 1);
    ASSERT_EQ(found->stop, 1);
}

TEST(SpliceDAO_UpdateLast) {
    cleanAllTables();
    dao::SpliceDAO dao;
    entity::Splice s; s.location = 100.0f; s.distance = 50.0f; s.time = "10";
    s.url = "/a"; s.last = 0; s.flag = 0; s.stop = 0;
    int id = dao.insert(s);
    dao.updateLast(id, 1);
    auto found = dao.findById(id);
    ASSERT_EQ(found->last, 1);
}

TEST(SpliceDAO_ClearAllLast) {
    cleanAllTables();
    dao::SpliceDAO dao;
    entity::Splice s; s.location = 100.0f; s.distance = 50.0f; s.time = "10";
    s.url = "/a"; s.last = 1; s.flag = 0; s.stop = 0;
    dao.insert(s);
    dao.insert(s);
    int affected = dao.clearAllLast();
    ASSERT_EQ(affected, 2);
    auto active = dao.findActive();
    ASSERT_EQ(active.size(), (size_t)0);
}

TEST(SpliceDAO_DeleteById) {
    cleanAllTables();
    dao::SpliceDAO dao;
    entity::Splice s; s.location = 100.0f; s.distance = 50.0f; s.time = "10";
    s.url = "/a"; s.last = 0; s.flag = 0; s.stop = 0;
    int id = dao.insert(s);
    dao.deleteById(id);
    ASSERT_FALSE(dao.findById(id).has_value());
}

TEST(SpliceDAO_Count) {
    cleanAllTables();
    dao::SpliceDAO dao;
    ASSERT_EQ(dao.count(), 0);
    entity::Splice s; s.location = 100.0f; s.distance = 50.0f; s.time = "10";
    s.url = "/a"; s.last = 0; s.flag = 0; s.stop = 0;
    dao.insert(s);
    ASSERT_EQ(dao.count(), 1);
}

// ============================================
// 5.5 SpliceWindowDAO 窗口生命周期测试
// ============================================
static entity::Splice makeSpliceInWindow(int windowId, int last = 1, int flag = 0, int stop = 0) {
    entity::Splice s;
    s.location = 1000.0f; s.distance = 120.0f; s.time = "30";
    s.url = "/window/" + std::to_string(windowId) + ".jpg";
    s.last = last; s.flag = flag; s.stop = stop; s.windowId = windowId;
    return s;
}

static bool dbErrorContains(const std::function<void()>& fn, const std::string& part) {
    try {
        fn();
    } catch (const db::DatabaseException& e) {
        return std::string(e.what()).find(part) != std::string::npos;
    }
    return false;
}

TEST(SpliceWindow_OpenRecordsOpenedEvent) {
    cleanAllTables();
    dao::SpliceWindowDAO wdao;
    int id = wdao.openWindow("A班", "张工");
    ASSERT_GT(id, 0);

    auto w = wdao.findById(id);
    ASSERT_TRUE(w.has_value());
    ASSERT_STR_EQ(w->status, "OPEN");
    ASSERT_STR_EQ(w->shiftCode, "A班");
    ASSERT_STR_EQ(w->oper, "张工");
    ASSERT_FALSE(w->openedAt.empty());

    auto events = wdao.findEvents(id);
    ASSERT_EQ(events.size(), (size_t)1);
    ASSERT_STR_EQ(events[0].event, "OPENED");
    ASSERT_STR_EQ(events[0].oper, "张工");
}

TEST(SpliceWindow_PauseResumeTransitionsAndTimeline) {
    cleanAllTables();
    dao::SpliceWindowDAO wdao;
    int id = wdao.openWindow("A班", "张工");

    ASSERT_EQ(wdao.pauseWindow(id, "张工"), 1);
    auto paused = wdao.findById(id);
    ASSERT_STR_EQ(paused->status, "PAUSED");
    ASSERT_FALSE(paused->pausedAt.empty());

    ASSERT_EQ(wdao.resumeWindow(id, "李工"), 1);
    auto resumed = wdao.findById(id);
    ASSERT_STR_EQ(resumed->status, "OPEN");
    ASSERT_FALSE(resumed->resumedAt.empty());

    auto events = wdao.findEvents(id);
    ASSERT_EQ(events.size(), (size_t)3);
    ASSERT_STR_EQ(events[0].event, "OPENED");
    ASSERT_STR_EQ(events[1].event, "PAUSED");
    ASSERT_STR_EQ(events[2].event, "RESUMED");
}

TEST(SpliceWindow_IllegalTransitionsRejected) {
    cleanAllTables();
    dao::SpliceWindowDAO wdao;
    int id = wdao.openWindow("A班", "张工");

    // OPEN 状态不能 resume
    ASSERT_TRUE(dbErrorContains([&] { wdao.resumeWindow(id, "张工"); }, "Illegal window transition"));

    wdao.pauseWindow(id, "张工");
    // PAUSED 状态不能再次 pause
    ASSERT_TRUE(dbErrorContains([&] { wdao.pauseWindow(id, "张工"); }, "Illegal window transition"));

    // 关闭后一切迁移均被数据库拒绝（终态）
    auto closed = wdao.closeWindow(id, "张工");
    ASSERT_TRUE(closed.closedNow);
    ASSERT_TRUE(dbErrorContains([&] { wdao.pauseWindow(id, "张工"); }, "CLOSED"));
    ASSERT_TRUE(dbErrorContains([&] { wdao.resumeWindow(id, "张工"); }, "CLOSED"));
}

TEST(SpliceWindow_InsertRejectedWhenPaused) {
    cleanAllTables();
    dao::SpliceWindowDAO wdao;
    dao::SpliceDAO sdao;
    int id = wdao.openWindow("A班", "张工");

    // OPEN 时写入被接受
    ASSERT_GT(sdao.insert(makeSpliceInWindow(id)), 0);

    wdao.pauseWindow(id, "张工");
    // 暂停期间新接缝被数据库触发器拒绝
    ASSERT_TRUE(dbErrorContains([&] { sdao.insert(makeSpliceInWindow(id)); }, "not OPEN"));

    wdao.resumeWindow(id, "张工");
    // 恢复后写入重新被接受
    ASSERT_GT(sdao.insert(makeSpliceInWindow(id)), 0);
}

TEST(SpliceWindow_StateWriteRejectedAfterClose) {
    cleanAllTables();
    dao::SpliceWindowDAO wdao;
    dao::SpliceDAO sdao;
    int id = wdao.openWindow("A班", "张工");
    int spliceId = sdao.insert(makeSpliceInWindow(id, 1, 0, 0));

    wdao.closeWindow(id, "张工");

    // 接班人交班后，后到的状态写入必须被拒绝（旧接缝无法伪装成当前停机候选）
    ASSERT_TRUE(dbErrorContains([&] { sdao.updateFlags(spliceId, 1, 1); }, "not OPEN"));
    ASSERT_TRUE(dbErrorContains([&] { sdao.updateLast(spliceId, 0); }, "not OPEN"));
}

TEST(SpliceWindow_QueriesOnlyReturnOpenWindowData_CrossShift) {
    cleanAllTables();
    dao::SpliceWindowDAO wdao;
    dao::SpliceDAO sdao;

    // A 班遗留窗口：已关闭，但里面留着 last=1、stop=1 的旧接缝（接班误认来源）
    int winA = wdao.openWindow("A班", "张工");
    int aSplice = sdao.insert(makeSpliceInWindow(winA, 1, 1, 1));
    wdao.closeWindow(winA, "张工");

    // B 班开窗：新接缝在 OPEN 窗口内
    int winB = wdao.openWindow("B班", "李工");
    sdao.insert(makeSpliceInWindow(winB, 1, 1, 1));

    // 三个业务查询都只返回所在窗口 OPEN 的数据
    auto active = sdao.findActive();
    ASSERT_EQ(active.size(), (size_t)1);
    ASSERT_EQ(active[0].windowId, winB);

    auto ready = sdao.findReadyToStop();
    ASSERT_EQ(ready.size(), (size_t)1);
    ASSERT_EQ(ready[0].windowId, winB);

    auto stoppable = sdao.findStoppable();
    ASSERT_EQ(stoppable.size(), (size_t)1);
    ASSERT_EQ(stoppable[0].windowId, winB);

    // 旧接缝仍可通过 findById / findByWindow 复盘读取（读取接口不被破坏）
    ASSERT_TRUE(sdao.findById(aSplice).has_value());
    ASSERT_EQ(sdao.findByWindow(winA).size(), (size_t)1);

    // 可按班次定位窗口
    auto shiftA = wdao.findByShift("A班");
    ASSERT_EQ(shiftA.size(), (size_t)1);
    ASSERT_STR_EQ(shiftA[0].status, "CLOSED");
}

TEST(SpliceWindow_CloseIsIdempotentSameResult) {
    cleanAllTables();
    dao::SpliceWindowDAO wdao;
    int id = wdao.openWindow("A班", "张工");

    auto first = wdao.closeWindow(id, "张工");
    ASSERT_TRUE(first.closedNow);
    ASSERT_STR_EQ(first.closedBy, "张工");
    ASSERT_FALSE(first.closedAt.empty());

    // 重复关闭（甚至换人）返回同一结果，不产生新写入
    auto second = wdao.closeWindow(id, "李工");
    ASSERT_FALSE(second.closedNow);
    ASSERT_STR_EQ(second.status, "CLOSED");
    ASSERT_STR_EQ(second.closedBy, first.closedBy);
    ASSERT_STR_EQ(second.closedAt, first.closedAt);

    // CLOSED 事件只有一条
    auto events = wdao.findEvents(id);
    size_t closedEvents = 0;
    for (auto& e : events) if (e.event == "CLOSED") closedEvents++;
    ASSERT_EQ(closedEvents, (size_t)1);
}

TEST(SpliceWindow_ConcurrentCloseTwoOperators) {
    cleanAllTables();
    dao::SpliceWindowDAO wdao;
    dao::SpliceDAO sdao;
    int id = wdao.openWindow("A班", "张工");
    sdao.insert(makeSpliceInWindow(id, 1, 1, 0));

    auto& dbm = db::DatabaseManager::instance();
    auto sessionA = dbm.openSession();
    auto sessionB = dbm.openSession();

    std::atomic<int> ready{0};
    std::atomic<bool> go{false};
    dao::WindowCloseResult r1, r2;

    auto closer = [&](std::shared_ptr<db::Session> sess, const std::string& op,
                      dao::WindowCloseResult* out) {
        ready.fetch_add(1);
        while (!go.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        dao::SpliceWindowDAO local;
        *out = local.closeWindowOn(*sess, id, op);
    };

    std::thread t1(closer, sessionA, "张工", &r1);
    std::thread t2(closer, sessionB, "李工", &r2);
    while (ready.load() < 2) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    go.store(true);
    t1.join();
    t2.join();

    // 恰好一人真正关窗，另一人拿到幂等的同一结果
    ASSERT_NE(r1.closedNow, r2.closedNow);
    ASSERT_STR_EQ(r1.status, "CLOSED");
    ASSERT_STR_EQ(r2.status, "CLOSED");
    ASSERT_STR_EQ(r1.closedBy, r2.closedBy);
    ASSERT_STR_EQ(r1.closedAt, r2.closedAt);
    ASSERT_TRUE(r1.closedBy == "张工" || r1.closedBy == "李工");

    // 数据库里只产生一条 CLOSED 事件
    auto events = wdao.findEvents(id);
    size_t closedEvents = 0;
    for (auto& e : events) if (e.event == "CLOSED") closedEvents++;
    ASSERT_EQ(closedEvents, (size_t)1);

    sessionA->close();
    sessionB->close();
}

TEST(SpliceWindow_LastValidStatusSnapshotFrozenOnClose) {
    cleanAllTables();
    dao::SpliceWindowDAO wdao;
    dao::SpliceDAO sdao;
    int id = wdao.openWindow("A班", "张工");

    int s1 = sdao.insert(makeSpliceInWindow(id, 1, 0, 0));
    auto w1 = wdao.findById(id);
    ASSERT_STR_EQ(w1->lastStatus, "DETECTED");

    sdao.updateFlags(s1, 1, 0);
    auto w2 = wdao.findById(id);
    ASSERT_STR_EQ(w2->lastStatus, "READY");

    int s2 = sdao.insert(makeSpliceInWindow(id, 0, 1, 1));
    auto w3 = wdao.findById(id);
    ASSERT_STR_EQ(w3->lastStatus, "STOPPABLE");
    ASSERT_EQ(w3->lastSpliceId, s2);

    // 关窗后快照定格，供接班人复盘最后一次有效状态
    wdao.closeWindow(id, "张工");
    auto frozen = wdao.findById(id);
    ASSERT_STR_EQ(frozen->lastStatus, "STOPPABLE");
    ASSERT_EQ(frozen->lastSpliceId, s2);
    ASSERT_FALSE(frozen->closedAt.empty());
}

TEST(SpliceWindow_ClearLastDoesNotTouchClosedWindow) {
    cleanAllTables();
    dao::SpliceWindowDAO wdao;
    dao::SpliceDAO sdao;
    int winA = wdao.openWindow("A班", "张工");
    int aSplice = sdao.insert(makeSpliceInWindow(winA, 1, 0, 0));
    int winB = wdao.openWindow("B班", "李工");
    int bSplice = sdao.insert(makeSpliceInWindow(winB, 1, 0, 0));
    wdao.closeWindow(winA, "张工");

    int affected = sdao.clearAllLast();
    ASSERT_EQ(affected, 1);  // 仅 OPEN 的 B班窗口记录被清理
    ASSERT_EQ(sdao.findById(bSplice)->last, 0);
    // CLOSED 窗口记录的 last 定格不变
    ASSERT_EQ(sdao.findById(aSplice)->last, 1);
}

// ============================================
// 6. FlawDAO 测试 (13 methods)
// ============================================

static entity::Flaw makeFlaw(const std::string& cat = "crack", int lvl = 3, int cam = 2,
                              const std::string& dt = "2026-02-24") {
    entity::Flaw f;
    f.category = cat; f.level = lvl;
    f.url = "/data/flaw/" + cat + ".jpg"; f.camera = cam;
    f.location = 2500.0f; f.distance = 180.0f;
    f.size = "15x8mm"; f.coordinate = "X:120,Y:340";
    f.date = dt; f.time = 30.5f;
    f.flag = 0; f.stop = 0; f.epoch = 1;
    return f;
}

TEST(FlawDAO_InsertAndFindById) {
    cleanAllTables();
    dao::FlawDAO dao;
    auto f = makeFlaw();
    long long id = dao.insert(f);
    ASSERT_GT(id, 0LL);

    auto found = dao.findById(id);
    ASSERT_TRUE(found.has_value());
    ASSERT_STR_EQ(found->category, "crack");
    ASSERT_EQ(found->level, 3);
    ASSERT_EQ(found->camera, 2);
    ASSERT_FLOAT_EQ(found->location, 2500.0f);
    ASSERT_FLOAT_EQ(found->distance, 180.0f);
    ASSERT_STR_EQ(found->size, "15x8mm");
    ASSERT_STR_EQ(found->coordinate, "X:120,Y:340");
    ASSERT_STR_EQ(found->date, "2026-02-24");
    ASSERT_FLOAT_EQ(found->time, 30.5f);
    ASSERT_EQ(found->epoch, 1);
}

TEST(FlawDAO_FindByIdNotFound) {
    dao::FlawDAO dao;
    auto found = dao.findById(-999);
    ASSERT_FALSE(found.has_value());
}

TEST(FlawDAO_FindAll) {
    cleanAllTables();
    dao::FlawDAO dao;
    dao.insert(makeFlaw("crack"));
    dao.insert(makeFlaw("corrosion"));
    auto all = dao.findAll();
    ASSERT_GE(all.size(), (size_t)2);
}

TEST(FlawDAO_FindByCategory) {
    cleanAllTables();
    dao::FlawDAO dao;
    dao.insert(makeFlaw("crack"));
    dao.insert(makeFlaw("corrosion"));
    dao.insert(makeFlaw("crack"));
    auto cracks = dao.findByCategory("crack");
    ASSERT_EQ(cracks.size(), (size_t)2);
}

TEST(FlawDAO_FindByLevel) {
    cleanAllTables();
    dao::FlawDAO dao;
    dao.insert(makeFlaw("crack", 1));
    dao.insert(makeFlaw("crack", 3));
    dao.insert(makeFlaw("crack", 3));
    auto lvl3 = dao.findByLevel(3);
    ASSERT_EQ(lvl3.size(), (size_t)2);
}

TEST(FlawDAO_FindByCamera) {
    cleanAllTables();
    dao::FlawDAO dao;
    dao.insert(makeFlaw("crack", 1, 1));
    dao.insert(makeFlaw("crack", 2, 2));
    dao.insert(makeFlaw("crack", 3, 2));
    auto cam2 = dao.findByCamera(2);
    ASSERT_EQ(cam2.size(), (size_t)2);
}

TEST(FlawDAO_FindReadyToStop) {
    cleanAllTables();
    dao::FlawDAO dao;
    auto f = makeFlaw();
    long long id = dao.insert(f);
    dao.updateFlags(id, 1, 0);
    auto ready = dao.findReadyToStop();
    ASSERT_EQ(ready.size(), (size_t)1);
}

TEST(FlawDAO_FindStoppable) {
    cleanAllTables();
    dao::FlawDAO dao;
    auto f = makeFlaw();
    long long id = dao.insert(f);
    dao.updateFlags(id, 0, 1);
    auto stoppable = dao.findStoppable();
    ASSERT_EQ(stoppable.size(), (size_t)1);
}

TEST(FlawDAO_FindByDateRange) {
    cleanAllTables();
    dao::FlawDAO dao;
    dao.insert(makeFlaw("a", 1, 1, "2026-01-01"));
    dao.insert(makeFlaw("b", 2, 1, "2026-02-15"));
    dao.insert(makeFlaw("c", 3, 1, "2026-03-30"));
    auto range = dao.findByDateRange("2026-01-01", "2026-02-28");
    ASSERT_EQ(range.size(), (size_t)2);
}

TEST(FlawDAO_UpdateFlags) {
    cleanAllTables();
    dao::FlawDAO dao;
    long long id = dao.insert(makeFlaw());
    int affected = dao.updateFlags(id, 1, 1);
    ASSERT_EQ(affected, 1);
    auto found = dao.findById(id);
    ASSERT_EQ(found->flag, 1);
    ASSERT_EQ(found->stop, 1);
}

TEST(FlawDAO_UpdateEpoch) {
    cleanAllTables();
    dao::FlawDAO dao;
    long long id = dao.insert(makeFlaw());
    dao.updateEpoch(id, 10);
    auto found = dao.findById(id);
    ASSERT_EQ(found->epoch, 10);
}

TEST(FlawDAO_DeleteById) {
    cleanAllTables();
    dao::FlawDAO dao;
    long long id = dao.insert(makeFlaw());
    dao.deleteById(id);
    ASSERT_FALSE(dao.findById(id).has_value());
}

TEST(FlawDAO_Count) {
    cleanAllTables();
    dao::FlawDAO dao;
    ASSERT_EQ(dao.count(), 0);
    dao.insert(makeFlaw());
    dao.insert(makeFlaw());
    dao.insert(makeFlaw());
    ASSERT_EQ(dao.count(), 3);
}

// ============================================
// 7. StopDAO 测试 (9 methods)
// ============================================
TEST(StopDAO_InsertAndFindById) {
    cleanAllTables();
    dao::StopDAO dao;
    entity::Stop s;
    s.category = 1; s.distance = 200.0f; s.flag = 1; s.command = 0;
    long long id = dao.insert(s);
    ASSERT_GT(id, 0LL);

    auto found = dao.findById(id);
    ASSERT_TRUE(found.has_value());
    ASSERT_EQ(found->category, 1);
    ASSERT_FLOAT_EQ(found->distance, 200.0f);
    ASSERT_EQ(found->flag, 1);
    ASSERT_EQ(found->command, 0);
}

TEST(StopDAO_FindByIdNotFound) {
    dao::StopDAO dao;
    ASSERT_FALSE(dao.findById(-999).has_value());
}

TEST(StopDAO_FindAll) {
    cleanAllTables();
    dao::StopDAO dao;
    entity::Stop s; s.category = 1; s.distance = 100.0f; s.flag = 0; s.command = 0;
    dao.insert(s);
    dao.insert(s);
    auto all = dao.findAll();
    ASSERT_GE(all.size(), (size_t)2);
}

TEST(StopDAO_FindAllowed) {
    cleanAllTables();
    dao::StopDAO dao;
    entity::Stop s1; s1.category = 1; s1.distance = 100.0f; s1.flag = 1; s1.command = 0;
    entity::Stop s2; s2.category = 2; s2.distance = 200.0f; s2.flag = 0; s2.command = 0;
    dao.insert(s1);
    dao.insert(s2);
    auto allowed = dao.findAllowed();
    ASSERT_EQ(allowed.size(), (size_t)1);
}

TEST(StopDAO_FindCommanded) {
    cleanAllTables();
    dao::StopDAO dao;
    entity::Stop s; s.category = 1; s.distance = 100.0f; s.flag = 1; s.command = 0;
    long long id = dao.insert(s);
    dao.issueCommand(id);
    auto commanded = dao.findCommanded();
    ASSERT_EQ(commanded.size(), (size_t)1);
    ASSERT_EQ(commanded[0].command, 1);
}

TEST(StopDAO_IssueCommand) {
    cleanAllTables();
    dao::StopDAO dao;
    entity::Stop s; s.category = 1; s.distance = 100.0f; s.flag = 1; s.command = 0;
    long long id = dao.insert(s);
    int affected = dao.issueCommand(id);
    ASSERT_EQ(affected, 1);
    auto found = dao.findById(id);
    ASSERT_EQ(found->command, 1);
}

TEST(StopDAO_UpdateFlag) {
    cleanAllTables();
    dao::StopDAO dao;
    entity::Stop s; s.category = 1; s.distance = 100.0f; s.flag = 0; s.command = 0;
    long long id = dao.insert(s);
    dao.updateFlag(id, 1);
    auto found = dao.findById(id);
    ASSERT_EQ(found->flag, 1);
}

TEST(StopDAO_DeleteById) {
    cleanAllTables();
    dao::StopDAO dao;
    entity::Stop s; s.category = 1; s.distance = 100.0f; s.flag = 0; s.command = 0;
    long long id = dao.insert(s);
    dao.deleteById(id);
    ASSERT_FALSE(dao.findById(id).has_value());
}

TEST(StopDAO_Count) {
    cleanAllTables();
    dao::StopDAO dao;
    ASSERT_EQ(dao.count(), 0);
    entity::Stop s; s.category = 1; s.distance = 100.0f; s.flag = 0; s.command = 0;
    dao.insert(s);
    ASSERT_EQ(dao.count(), 1);
}

// ============================================
// 8. CompareDAO 测试 (8 methods)
// ============================================
TEST(CompareDAO_InsertAndFindById) {
    cleanAllTables();
    dao::CompareDAO dao;
    entity::Compare c;
    c.newUrl = "/new.jpg"; c.oldUrl = "/old.jpg";
    c.value = 0.85f; c.category = 1; c.level = 2; c.oldSize = "12x6mm";
    long long id = dao.insert(c);
    ASSERT_GT(id, 0LL);

    auto found = dao.findById(id);
    ASSERT_TRUE(found.has_value());
    ASSERT_STR_EQ(found->newUrl, "/new.jpg");
    ASSERT_STR_EQ(found->oldUrl, "/old.jpg");
    ASSERT_FLOAT_EQ(found->value, 0.85f);
    ASSERT_EQ(found->category, 1);
    ASSERT_EQ(found->level, 2);
    ASSERT_STR_EQ(found->oldSize, "12x6mm");
}

TEST(CompareDAO_FindByIdNotFound) {
    dao::CompareDAO dao;
    ASSERT_FALSE(dao.findById(-999).has_value());
}

TEST(CompareDAO_FindAll) {
    cleanAllTables();
    dao::CompareDAO dao;
    entity::Compare c; c.newUrl = "/n"; c.oldUrl = "/o";
    c.value = 0.5f; c.category = 1; c.level = 1; c.oldSize = "5x5mm";
    dao.insert(c);
    dao.insert(c);
    auto all = dao.findAll();
    ASSERT_GE(all.size(), (size_t)2);
}

TEST(CompareDAO_FindByCategory) {
    cleanAllTables();
    dao::CompareDAO dao;
    entity::Compare c1; c1.newUrl = "/n"; c1.oldUrl = "/o";
    c1.value = 0.5f; c1.category = 1; c1.level = 1; c1.oldSize = "5mm";
    entity::Compare c2; c2.newUrl = "/n"; c2.oldUrl = "/o";
    c2.value = 0.6f; c2.category = 2; c2.level = 2; c2.oldSize = "6mm";
    dao.insert(c1);
    dao.insert(c2);
    auto cat1 = dao.findByCategory(1);
    ASSERT_EQ(cat1.size(), (size_t)1);
}

TEST(CompareDAO_FindByLevel) {
    cleanAllTables();
    dao::CompareDAO dao;
    entity::Compare c; c.newUrl = "/n"; c.oldUrl = "/o";
    c.value = 0.5f; c.category = 1; c.level = 3; c.oldSize = "5mm";
    dao.insert(c);
    auto lvl3 = dao.findByLevel(3);
    ASSERT_EQ(lvl3.size(), (size_t)1);
}

TEST(CompareDAO_Update) {
    cleanAllTables();
    dao::CompareDAO dao;
    entity::Compare c; c.newUrl = "/n"; c.oldUrl = "/o";
    c.value = 0.5f; c.category = 1; c.level = 1; c.oldSize = "5mm";
    long long id = dao.insert(c);

    c.id = id;
    c.value = 0.99f;
    c.level = 5;
    c.newUrl = "/updated.jpg";
    int affected = dao.update(c);
    ASSERT_EQ(affected, 1);

    auto found = dao.findById(id);
    ASSERT_FLOAT_EQ(found->value, 0.99f);
    ASSERT_EQ(found->level, 5);
    ASSERT_STR_EQ(found->newUrl, "/updated.jpg");
}

TEST(CompareDAO_DeleteById) {
    cleanAllTables();
    dao::CompareDAO dao;
    entity::Compare c; c.newUrl = "/n"; c.oldUrl = "/o";
    c.value = 0.5f; c.category = 1; c.level = 1; c.oldSize = "5mm";
    long long id = dao.insert(c);
    dao.deleteById(id);
    ASSERT_FALSE(dao.findById(id).has_value());
}

TEST(CompareDAO_Count) {
    cleanAllTables();
    dao::CompareDAO dao;
    ASSERT_EQ(dao.count(), 0);
    entity::Compare c; c.newUrl = "/n"; c.oldUrl = "/o";
    c.value = 0.5f; c.category = 1; c.level = 1; c.oldSize = "5mm";
    dao.insert(c);
    ASSERT_EQ(dao.count(), 1);
}

// ============================================
// 9. HistoryDAO 测试 (9 methods)
// ============================================
TEST(HistoryDAO_InsertAndFindById) {
    cleanAllTables();
    dao::HistoryDAO dao;
    entity::History h;
    h.category = "corrosion"; h.level = 2;
    h.url = "/hist/001.jpg"; h.camera = 1;
    h.size = "20x15mm"; h.date = "2026-02-24";
    long long id = dao.insert(h);
    ASSERT_GT(id, 0LL);

    auto found = dao.findById(id);
    ASSERT_TRUE(found.has_value());
    ASSERT_STR_EQ(found->category, "corrosion");
    ASSERT_EQ(found->level, 2);
    ASSERT_STR_EQ(found->url, "/hist/001.jpg");
    ASSERT_EQ(found->camera, 1);
    ASSERT_STR_EQ(found->size, "20x15mm");
    ASSERT_STR_EQ(found->date, "2026-02-24");
}

TEST(HistoryDAO_FindByIdNotFound) {
    dao::HistoryDAO dao;
    ASSERT_FALSE(dao.findById(-999).has_value());
}

TEST(HistoryDAO_FindAll) {
    cleanAllTables();
    dao::HistoryDAO dao;
    entity::History h; h.category = "crack"; h.level = 1;
    h.url = "/h"; h.camera = 1; h.size = "5mm"; h.date = "2026-01-01";
    dao.insert(h);
    auto all = dao.findAll();
    ASSERT_GE(all.size(), (size_t)1);
}

TEST(HistoryDAO_FindByCategory) {
    cleanAllTables();
    dao::HistoryDAO dao;
    entity::History h1; h1.category = "crack"; h1.level = 1;
    h1.url = "/h"; h1.camera = 1; h1.size = "5mm"; h1.date = "2026-01-01";
    entity::History h2; h2.category = "corrosion"; h2.level = 2;
    h2.url = "/h"; h2.camera = 1; h2.size = "5mm"; h2.date = "2026-01-01";
    dao.insert(h1);
    dao.insert(h2);
    auto cracks = dao.findByCategory("crack");
    ASSERT_EQ(cracks.size(), (size_t)1);
}

TEST(HistoryDAO_FindByDate) {
    cleanAllTables();
    dao::HistoryDAO dao;
    entity::History h; h.category = "crack"; h.level = 1;
    h.url = "/h"; h.camera = 1; h.size = "5mm"; h.date = "2026-05-20";
    dao.insert(h);
    auto results = dao.findByDate("2026-05-20");
    ASSERT_EQ(results.size(), (size_t)1);
}

TEST(HistoryDAO_FindByCamera) {
    cleanAllTables();
    dao::HistoryDAO dao;
    entity::History h1; h1.category = "crack"; h1.level = 1;
    h1.url = "/h"; h1.camera = 3; h1.size = "5mm"; h1.date = "2026-01-01";
    entity::History h2; h2.category = "crack"; h2.level = 1;
    h2.url = "/h"; h2.camera = 5; h2.size = "5mm"; h2.date = "2026-01-01";
    dao.insert(h1);
    dao.insert(h2);
    auto cam3 = dao.findByCamera(3);
    ASSERT_EQ(cam3.size(), (size_t)1);
}

TEST(HistoryDAO_FindByDateRange) {
    cleanAllTables();
    dao::HistoryDAO dao;
    entity::History h; h.category = "crack"; h.level = 1;
    h.url = "/h"; h.camera = 1; h.size = "5mm";
    h.date = "2026-01-15"; dao.insert(h);
    h.date = "2026-02-15"; dao.insert(h);
    h.date = "2026-04-15"; dao.insert(h);
    auto range = dao.findByDateRange("2026-01-01", "2026-03-01");
    ASSERT_EQ(range.size(), (size_t)2);
}

TEST(HistoryDAO_DeleteById) {
    cleanAllTables();
    dao::HistoryDAO dao;
    entity::History h; h.category = "crack"; h.level = 1;
    h.url = "/h"; h.camera = 1; h.size = "5mm"; h.date = "2026-01-01";
    long long id = dao.insert(h);
    dao.deleteById(id);
    ASSERT_FALSE(dao.findById(id).has_value());
}

TEST(HistoryDAO_Count) {
    cleanAllTables();
    dao::HistoryDAO dao;
    ASSERT_EQ(dao.count(), 0);
    entity::History h; h.category = "crack"; h.level = 1;
    h.url = "/h"; h.camera = 1; h.size = "5mm"; h.date = "2026-01-01";
    dao.insert(h);
    dao.insert(h);
    ASSERT_EQ(dao.count(), 2);
}

// ============================================
// 10. RemoveDAO 测试 (7 methods)
// ============================================
TEST(RemoveDAO_Insert) {
    cleanAllTables();
    dao::RemoveDAO dao;
    long long id = dao.insert();
    ASSERT_GT(id, 0LL);
}

TEST(RemoveDAO_InsertWithId) {
    cleanAllTables();
    dao::RemoveDAO dao;
    int affected = dao.insertWithId(88888);
    ASSERT_EQ(affected, 1);
    auto found = dao.findById(88888);
    ASSERT_TRUE(found.has_value());
    ASSERT_EQ(found->id, 88888LL);
}

TEST(RemoveDAO_FindById) {
    cleanAllTables();
    dao::RemoveDAO dao;
    long long id = dao.insert();
    auto found = dao.findById(id);
    ASSERT_TRUE(found.has_value());
    ASSERT_EQ(found->id, id);
}

TEST(RemoveDAO_FindByIdNotFound) {
    dao::RemoveDAO dao;
    ASSERT_FALSE(dao.findById(-999).has_value());
}

TEST(RemoveDAO_FindAll) {
    cleanAllTables();
    dao::RemoveDAO dao;
    dao.insert();
    dao.insert();
    auto all = dao.findAll();
    ASSERT_GE(all.size(), (size_t)2);
}

TEST(RemoveDAO_Exists) {
    cleanAllTables();
    dao::RemoveDAO dao;
    long long id = dao.insert();
    ASSERT_TRUE(dao.exists(id));
    ASSERT_FALSE(dao.exists(-999));
}

TEST(RemoveDAO_DeleteById) {
    cleanAllTables();
    dao::RemoveDAO dao;
    long long id = dao.insert();
    dao.deleteById(id);
    ASSERT_FALSE(dao.exists(id));
}

TEST(RemoveDAO_Count) {
    cleanAllTables();
    dao::RemoveDAO dao;
    ASSERT_EQ(dao.count(), 0);
    dao.insert();
    dao.insert();
    dao.insert();
    ASSERT_EQ(dao.count(), 3);
}

// ============================================
// 主入口
// ============================================
int main() {
    std::cout << std::string(60, '*') << std::endl;
    std::cout << "  Industrial Inspection System - Test Suite" << std::endl;
    std::cout << "  工业检测系统 - 完整测试套件" << std::endl;
    std::cout << std::string(60, '*') << std::endl;

    try {
        // 初始化日志
        utils::Logger::instance().setLevel(utils::LogLevel::INFO);

        // 加载数据库配置
        config::DatabaseConfig dbConfig;
        dbConfig.loadFromEnv();

        // 连接数据库
        LOG_INFO("TestMain", "Connecting to database...");
        db::DatabaseManager::instance().init(dbConfig);

        // 运行所有测试
        int failures = test::TestRunner::instance().run();

        // 清理
        cleanAllTables();
        db::DatabaseManager::instance().close();

        return failures;

    } catch (const db::DatabaseException& e) {
        LOG_ERROR("TestMain", std::string("Database error: ") + e.what());
        std::cerr << "\n[FATAL] Database error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        LOG_ERROR("TestMain", std::string("Unexpected error: ") + e.what());
        std::cerr << "\n[FATAL] Unexpected error: " << e.what() << std::endl;
        return 2;
    }
}
