/**
 * 工业检测系统 - C++ MySQL 数据访问层
 * 
 * 演示所有 DAO 的 CRUD 操作
 * 编译环境: Visual Studio 2019+ / CMake 3.16+ / MySQL Connector C
 */

#ifdef _WIN32
#include <winsock2.h>
#endif

#include <iostream>
#include <string>
#include <iomanip>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "config/AppConfig.h"
#include "utils/Logger.h"
#include "db/DatabaseManager.h"
#include "dao/SpeedDAO.h"
#include "dao/SpliceDAO.h"
#include "dao/FlawDAO.h"
#include "dao/StopDAO.h"
#include "dao/CompareDAO.h"
#include "dao/HistoryDAO.h"
#include "dao/RemoveDAO.h"

using namespace std;

// ============================================
// 辅助打印函数
// ============================================
static void printSeparator(const string& title) {
    cout << "\n" << string(60, '=') << endl;
    cout << "  " << title << endl;
    cout << string(60, '=') << endl;
}

static void printResult(const string& operation, bool success) {
    cout << "  [" << (success ? "OK" : "FAIL") << "] " << operation << endl;
}

// ============================================
// 各表 CRUD 演示
// ============================================
static void demoSpeed(dao::SpeedDAO& speedDao) {
    printSeparator("SPEED 速度表 CRUD");

    // INSERT
    entity::Speed s;
    s.value = 120.5f;
    s.date = "2026-02-24";
    s.flag = 0;
    int id = speedDao.insert(s);
    printResult("INSERT speed (id=" + to_string(id) + ")", id > 0);

    // SELECT by ID
    auto found = speedDao.findById(id);
    printResult("SELECT by id=" + to_string(id), found.has_value());
    if (found) {
        cout << "    value=" << found->value << ", date=" << found->date << ", flag=" << found->flag << endl;
    }

    // UPDATE
    int affected = speedDao.updateValue(id, 135.8f);
    printResult("UPDATE value -> 135.8", affected > 0);

    // MARK USED
    affected = speedDao.markUsed(id);
    printResult("MARK USED", affected > 0);

    // COUNT
    int cnt = speedDao.count();
    cout << "  Total records: " << cnt << endl;

    // FIND ALL
    auto all = speedDao.findAll();
    cout << "  FindAll returned " << all.size() << " records" << endl;

    // DELETE
    affected = speedDao.deleteById(id);
    printResult("DELETE id=" + to_string(id), affected > 0);
}

// 回收上次运行遗留的 OPEN 窗口，保证演示可重复执行
static void closeLeftoverWindow(dao::SpliceDAO& spliceDao) {
    auto cur = spliceDao.findCurrentWindow();
    if (cur.has_value()) {
        spliceDao.closeWindow(cur->id, "demo-cleanup");
        cout << "  Closed leftover window id=" << cur->id << " (" << cur->label << ")" << endl;
    }
}

static void demoSplice(dao::SpliceDAO& spliceDao) {
    printSeparator("SPLICE 接缝表 CRUD");

    closeLeftoverWindow(spliceDao);
    // 接缝写入必须发生在开启的检测窗口内
    int wid = spliceDao.openWindow("CRUD演示窗口", "demo");
    cout << "  Opened window id=" << wid << endl;

    entity::Splice s;
    s.location = 1500.0f;
    s.distance = 320.5f;
    s.time = "45";
    s.url = "/data/splice/img_001.jpg";
    s.last = 1;
    s.flag = 0;
    s.stop = 0;
    int id = spliceDao.insert(s);
    printResult("INSERT splice (id=" + to_string(id) + ")", id > 0);

    auto found = spliceDao.findById(id);
    printResult("SELECT by id", found.has_value());
    if (found) {
        cout << "    location=" << found->location << ", distance=" << found->distance
             << ", time=" << found->time << ", window_id=" << found->windowId << endl;
    }

    // 查询有效接头
    auto active = spliceDao.findActive();
    cout << "  Active splices: " << active.size() << endl;

    // 更新标志
    spliceDao.updateFlags(id, 1, 1);
    printResult("UPDATE flags (flag=1, stop=1)", true);

    // 清除 last
    spliceDao.clearAllLast();
    printResult("CLEAR all last flags", true);

    spliceDao.deleteById(id);
    printResult("DELETE", true);

    spliceDao.closeWindow(wid, "demo");
    cout << "  Closed window id=" << wid << endl;
}

// ============================================
// 检测窗口生命周期演示：交接班场景
// 复现：跨班次查询、非法迁移、两名操作员同时关窗
// ============================================

// 交接看板：让接班人直接看出每条记录为何出现或被运行查询排除
static void printHandoverBoard(dao::SpliceDAO& spliceDao) {
    auto view = spliceDao.findAllWithWindow();
    cout << "  --- 交接看板 --- 运行查询命中: 当前接头=" << spliceDao.findActive().size()
         << " 准备停机=" << spliceDao.findReadyToStop().size()
         << " 可停机=" << spliceDao.findStoppable().size() << endl;
    for (auto& v : view) {
        cout << "    splice#" << v.splice.id
             << " last=" << v.splice.last << " flag=" << v.splice.flag << " stop=" << v.splice.stop
             << " | 窗口#" << v.windowId << " " << v.windowLabel
             << " [" << entity::toString(v.windowState) << "] -> ";
        if (v.visibleInOpenQueries()) {
            cout << "进入运行查询";
        } else if (v.windowState == entity::WindowState::PAUSED) {
            cout << "被排除（窗口暂停，检测冻结）";
        } else {
            cout << "被排除（窗口已关闭，上一班遗留）";
        }
        cout << endl;
    }
}

static void demoSpliceWindowLifecycle(dao::SpliceDAO& spliceDao,
                                      const config::DatabaseConfig& dbConfig) {
    printSeparator("SPLICE 检测窗口生命周期（交接班场景）");

    closeLeftoverWindow(spliceDao);

    // ---- 1. 白班开窗，产生一条可停机接缝 ----
    int dayShift = spliceDao.openWindow("白班-2026-09-22", "调度员-白班");
    entity::Splice s;
    s.location = 1500.0f; s.distance = 320.5f; s.time = "45";
    s.url = "/data/splice/day_001.jpg"; s.last = 1; s.flag = 0; s.stop = 0;
    int sid = spliceDao.insert(s);
    spliceDao.updateFlags(sid, 1, 1);
    cout << "  [白班] 窗口#" << dayShift << " 接缝#" << sid << " flag=1 stop=1" << endl;
    printHandoverBoard(spliceDao);

    // ---- 2. 暂停 -> 运行查询冻结；恢复 -> 重新出现 ----
    cout << "\n  [白班] 暂停窗口（交接前冻结检测）" << endl;
    spliceDao.pauseWindow(dayShift, "调度员-白班");
    printHandoverBoard(spliceDao);
    spliceDao.resumeWindow(dayShift, "调度员-白班");
    printResult("恢复后接缝重新进入可停机查询", spliceDao.findStoppable().size() == 1);

    // ---- 3. 交班：关闭白班窗口 ----
    cout << "\n  [交班] 关闭白班窗口" << endl;
    auto closed = spliceDao.closeWindow(dayShift, "调度员-白班");
    cout << "    closed_at=" << closed.closedAt << " version=" << closed.version << endl;
    printHandoverBoard(spliceDao);
    auto retained = spliceDao.findById(sid);
    printResult("已关闭窗口的接缝保留最后有效状态供复盘（findById 可读，stop=1）",
                retained.has_value() && retained->stop == 1);

    // ---- 4. 窗口关闭后，迟到的状态写入被拒绝 ----
    cout << "\n  [关窗后] 迟到的状态写入：" << endl;
    try {
        spliceDao.updateFlags(sid, 0, 0);
        printResult("DAO 迟到写入 updateFlags", false);
    } catch (const dao::WindowStateException& e) {
        cout << "    [OK] DAO 拒绝 updateFlags: " << e.what() << endl;
    }
    entity::Splice late;
    late.windowId = dayShift;  // 显式写入已关闭窗口
    late.location = 1.0f; late.distance = 1.0f; late.time = "1"; late.url = "/late";
    try {
        spliceDao.insert(late);
        printResult("DAO 迟到写入 insert", false);
    } catch (const dao::WindowStateException& e) {
        cout << "    [OK] DAO 拒绝 insert: " << e.what() << endl;
    }
    try {
        db::DatabaseManager::instance().execute(
            "UPDATE SPLICE SET stop = 0 WHERE id = " + to_string(sid));
        printResult("绕过 DAO 直接 SQL 更新", false);
    } catch (const db::DatabaseException& e) {
        cout << "    [OK] 数据库触发器拒绝直接 UPDATE: " << e.what() << endl;
    }

    // ---- 5. 非法迁移 ----
    cout << "\n  [关窗后] 非法状态迁移：" << endl;
    try {
        spliceDao.resumeWindow(dayShift, "调度员-夜班");
        printResult("恢复已关闭窗口", false);
    } catch (const dao::WindowStateException& e) {
        cout << "    [OK] 拒绝 RESUME: " << e.what() << endl;
    }
    try {
        spliceDao.pauseWindow(dayShift, "调度员-夜班");
        printResult("暂停已关闭窗口", false);
    } catch (const dao::WindowStateException& e) {
        cout << "    [OK] 拒绝 PAUSE: " << e.what() << endl;
    }
    try {
        db::DatabaseManager::instance().execute(
            "UPDATE SPLICE_WINDOW SET state = 'OPEN' WHERE id = " + to_string(dayShift));
        printResult("绕过 DAO 直接 SQL 重开窗口", false);
    } catch (const db::DatabaseException& e) {
        cout << "    [OK] 数据库触发器拒绝重开: " << e.what() << endl;
    }

    // ---- 6. 重复关闭返回同一结果（幂等） ----
    auto again = spliceDao.closeWindow(dayShift, "调度员-夜班");
    printResult("重复关闭返回同一结果（closed_at/version 不变）",
                again.closedAt == closed.closedAt && again.version == closed.version);

    // ---- 7. 两名操作员同时关窗（各自独立连接，并发提交） ----
    cout << "\n  [并发] 两名操作员同时关闭同一窗口：" << endl;
    int raceWindow = spliceDao.openWindow("交接-并发演示", "调度员-白班");
    auto connA = db::DatabaseManager::openConnection(dbConfig);
    auto connB = db::DatabaseManager::openConnection(dbConfig);
    dao::SpliceDAO opA(connA.get());
    dao::SpliceDAO opB(connB.get());
    entity::SpliceWindow resultA, resultB;
    {
        std::mutex m;
        std::condition_variable cv;
        bool go = false;
        auto worker = [&](dao::SpliceDAO* dao, const char* who, entity::SpliceWindow* out) {
            {   // 起跑门闩：尽量让两次关闭真正并发
                std::unique_lock<std::mutex> lk(m);
                cv.wait(lk, [&] { return go; });
            }
            *out = dao->closeWindow(raceWindow, who);
        };
        std::thread t1(worker, &opA, "操作员-甲", &resultA);
        std::thread t2(worker, &opB, "操作员-乙", &resultB);
        {
            std::lock_guard<std::mutex> lk(m);
            go = true;
        }
        cv.notify_all();
        t1.join();
        t2.join();
    }
    connA->close();
    connB->close();
    cout << "    甲: state=" << entity::toString(resultA.state)
         << " closed_at=" << resultA.closedAt << " version=" << resultA.version << endl;
    cout << "    乙: state=" << entity::toString(resultB.state)
         << " closed_at=" << resultB.closedAt << " version=" << resultB.version << endl;
    printResult("并发关闭返回同一结果", resultA.closedAt == resultB.closedAt
                && resultA.version == resultB.version);
    int closeEvents = 0;
    for (auto& ev : spliceDao.windowEvents(raceWindow)) {
        if (ev.action == "CLOSE") closeEvents++;
    }
    printResult("审计日志中恰好一条 CLOSE 记录", closeEvents == 1);

    // ---- 8. 夜班开窗：跨班次查询只认当前窗口 ----
    cout << "\n  [夜班] 开启新窗口，白班遗留不再干扰：" << endl;
    spliceDao.openWindow("夜班-2026-09-22", "调度员-夜班");
    entity::Splice s2;
    s2.location = 2600.0f; s2.distance = 150.0f; s2.time = "30";
    s2.url = "/data/splice/night_001.jpg"; s2.last = 1; s2.flag = 0; s2.stop = 0;
    int sid2 = spliceDao.insert(s2);
    spliceDao.updateFlags(sid2, 1, 1);
    printHandoverBoard(spliceDao);
    auto stoppable = spliceDao.findStoppable();
    printResult("可停机查询只返回夜班窗口的接缝",
                stoppable.size() == 1 && stoppable[0].id == sid2);

    // ---- 9. 白班窗口的完整审计轨迹（起止时间与操作员） ----
    cout << "\n  [复盘] 白班窗口#" << dayShift << " 状态迁移审计：" << endl;
    for (auto& ev : spliceDao.windowEvents(dayShift)) {
        cout << "    " << ev.createdAt << "  " << ev.action
             << "  " << ev.fromState << " -> " << ev.toState
             << "  by " << ev.actor << endl;
    }

    // 演示收尾：关闭夜班窗口，保持现场整洁（演示可重复执行）
    closeLeftoverWindow(spliceDao);
}

static void demoFlaw(dao::FlawDAO& flawDao) {
    printSeparator("FLAW 损伤表 CRUD");

    entity::Flaw f;
    f.category = "crack";
    f.level = 3;
    f.url = "/data/flaw/crack_001.jpg";
    f.camera = 2;
    f.location = 2500.0f;
    f.distance = 180.0f;
    f.size = "15x8mm";
    f.coordinate = "X:120,Y:340";
    f.date = "2026-02-24";
    f.time = 30.5f;
    f.flag = 0;
    f.stop = 0;
    f.epoch = 1;
    long long id = flawDao.insert(f);
    printResult("INSERT flaw (id=" + to_string(id) + ")", id > 0);

    auto found = flawDao.findById(id);
    printResult("SELECT by id", found.has_value());
    if (found) {
        cout << "    category=" << found->category << ", level=" << found->level
             << ", size=" << found->size << ", camera=" << found->camera << endl;
    }

    // 按类型查询
    auto cracks = flawDao.findByCategory("crack");
    cout << "  Cracks found: " << cracks.size() << endl;

    // 更新追踪圈数
    flawDao.updateEpoch(id, 5);
    printResult("UPDATE epoch -> 5", true);

    // 更新停机标志
    flawDao.updateFlags(id, 1, 1);
    printResult("UPDATE flags (flag=1, stop=1)", true);

    flawDao.deleteById(id);
    printResult("DELETE", true);
}

static void demoStop(dao::StopDAO& stopDao) {
    printSeparator("STOP 停机表 CRUD");

    entity::Stop s;
    s.category = 1;
    s.distance = 200.0f;
    s.flag = 1;
    s.command = 0;
    long long id = stopDao.insert(s);
    printResult("INSERT stop (id=" + to_string(id) + ")", id > 0);

    auto found = stopDao.findById(id);
    printResult("SELECT by id", found.has_value());

    // 下发停机命令
    stopDao.issueCommand(id);
    printResult("ISSUE stop command", true);

    // 查询已下发命令的记录
    auto commanded = stopDao.findCommanded();
    cout << "  Commanded stops: " << commanded.size() << endl;

    stopDao.deleteById(id);
    printResult("DELETE", true);
}

static void demoCompare(dao::CompareDAO& compareDao) {
    printSeparator("COMPARE 对比表 CRUD");

    entity::Compare c;
    c.newUrl = "/data/compare/new_001.jpg";
    c.oldUrl = "/data/compare/old_001.jpg";
    c.value = 0.85f;
    c.category = 1;
    c.level = 2;
    c.oldSize = "12x6mm";
    long long id = compareDao.insert(c);
    printResult("INSERT compare (id=" + to_string(id) + ")", id > 0);

    auto found = compareDao.findById(id);
    printResult("SELECT by id", found.has_value());
    if (found) {
        cout << "    value=" << found->value << ", level=" << found->level << endl;
    }

    // 更新
    c.id = id;
    c.value = 0.92f;
    c.level = 3;
    compareDao.update(c);
    printResult("UPDATE value -> 0.92, level -> 3", true);

    compareDao.deleteById(id);
    printResult("DELETE", true);
}

static void demoHistory(dao::HistoryDAO& historyDao) {
    printSeparator("HISTORY 历史表 CRUD");

    entity::History h;
    h.category = "corrosion";
    h.level = 2;
    h.url = "/data/history/corrosion_001.jpg";
    h.camera = 1;
    h.size = "20x15mm";
    h.date = "2026-02-24";
    long long id = historyDao.insert(h);
    printResult("INSERT history (id=" + to_string(id) + ")", id > 0);

    auto found = historyDao.findById(id);
    printResult("SELECT by id", found.has_value());

    auto byCategory = historyDao.findByCategory("corrosion");
    cout << "  Corrosion records: " << byCategory.size() << endl;

    historyDao.deleteById(id);
    printResult("DELETE", true);
}

static void demoRemove(dao::RemoveDAO& removeDao) {
    printSeparator("REMOVE 移除表 CRUD");

    long long id = removeDao.insert();
    printResult("INSERT remove (id=" + to_string(id) + ")", id > 0);

    bool exists = removeDao.exists(id);
    printResult("EXISTS check", exists);

    removeDao.insertWithId(99999);
    printResult("INSERT with specific id=99999", true);

    auto all = removeDao.findAll();
    cout << "  Total remove records: " << all.size() << endl;

    removeDao.deleteById(id);
    removeDao.deleteById(99999);
    printResult("DELETE all test records", true);
}

// ============================================
// 主入口
// ============================================
int main() {
    cout << string(60, '*') << endl;
    cout << "  Industrial Inspection System - C++ MySQL Data Access Layer" << endl;
    cout << "  工业检测系统 - C++ 数据访问层" << endl;
    cout << string(60, '*') << endl;

    try {
        // 1. 初始化日志
        utils::Logger::instance().setLevel(utils::LogLevel::INFO);

        // 2. 加载数据库配置
        config::DatabaseConfig dbConfig;
        dbConfig.loadFromEnv();

        // 3. 连接数据库
        LOG_INFO("Main", "Connecting to database...");
        db::DatabaseManager::instance().init(dbConfig);

        // 4. 初始化 DAO
        dao::SpeedDAO speedDao;
        dao::SpliceDAO spliceDao;
        dao::FlawDAO flawDao;
        dao::StopDAO stopDao;
        dao::CompareDAO compareDao;
        dao::HistoryDAO historyDao;
        dao::RemoveDAO removeDao;

        // 5. 执行各表 CRUD 演示
        demoSpeed(speedDao);
        demoSplice(spliceDao);
        demoSpliceWindowLifecycle(spliceDao, dbConfig);
        demoFlaw(flawDao);
        demoStop(stopDao);
        demoCompare(compareDao);
        demoHistory(historyDao);
        demoRemove(removeDao);

        // 6. 关闭连接
        db::DatabaseManager::instance().close();

        printSeparator("ALL TESTS COMPLETED SUCCESSFULLY");

    } catch (const db::DatabaseException& e) {
        LOG_ERROR("Main", string("Database error: ") + e.what());
        cerr << "\n[FATAL] Database error: " << e.what() << endl;
        return 1;
    } catch (const exception& e) {
        LOG_ERROR("Main", string("Unexpected error: ") + e.what());
        cerr << "\n[FATAL] Unexpected error: " << e.what() << endl;
        return 2;
    }

    return 0;
}
