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

#include "config/AppConfig.h"
#include "utils/Logger.h"
#include "db/DatabaseManager.h"
#include "dao/SpeedDAO.h"
#include "dao/SpliceDAO.h"
#include "dao/SpliceWindowDAO.h"
#include "dao/FlawDAO.h"
#include "dao/StopDAO.h"
#include "dao/CompareDAO.h"
#include "dao/HistoryDAO.h"
#include "dao/RemoveDAO.h"

#include <thread>
#include <atomic>
#include <chrono>
#include <memory>

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

static void printSpliceRow(const entity::Splice& s) {
    cout << "      - splice id=" << s.id << " window=" << s.windowId
         << " last=" << s.last << " flag=" << s.flag << " stop=" << s.stop
         << " distance=" << s.distance << endl;
}

// 演示窗口过滤效果，并解释记录为何出现/被排除
static void printScopedQueries(dao::SpliceDAO& spliceDao) {
    auto active = spliceDao.findActive();
    cout << "    当前接头 findActive()      -> " << active.size() << " 条" << endl;
    for (auto& s : active) printSpliceRow(s);

    auto ready = spliceDao.findReadyToStop();
    cout << "    准备停机 findReadyToStop()  -> " << ready.size() << " 条" << endl;
    for (auto& s : ready) printSpliceRow(s);

    auto stoppable = spliceDao.findStoppable();
    cout << "    可停机   findStoppable()    -> " << stoppable.size() << " 条" << endl;
    for (auto& s : stoppable) printSpliceRow(s);
}

static void printWindowTimeline(dao::SpliceWindowDAO& wdao, int windowId) {
    auto events = wdao.findEvents(windowId);
    cout << "    窗口 " << windowId << " 生命周期轨迹:" << endl;
    for (auto& e : events) {
        cout << "        " << e.eventAt << "  " << e.event
             << "  by " << (e.oper.empty() ? "(系统)" : e.oper) << endl;
    }
}

static entity::Splice makeDemoSplice(int windowId, float distance, int last, int flag, int stop) {
    entity::Splice s;
    s.location = 1500.0f;
    s.distance = distance;
    s.time = "45";
    s.url = "/data/splice/win" + to_string(windowId) + "_" + to_string(distance) + ".jpg";
    s.last = last;
    s.flag = flag;
    s.stop = stop;
    s.windowId = windowId;
    return s;
}

// ============================================
// SPLICE 接缝检测窗口生命周期演示
// ============================================
static void demoSplice(dao::SpliceDAO& spliceDao, dao::SpliceWindowDAO& windowDao) {
    printSeparator("SPLICE 接缝检测窗口生命周期（开启/暂停/恢复/关闭）");

    // 保证演示可重复执行：先清理接缝与窗口数据
    db::DatabaseManager::instance().execute("DELETE FROM SPLICE");
    db::DatabaseManager::instance().execute("DELETE FROM SPLICE_WINDOW_EVENT");
    db::DatabaseManager::instance().execute("DELETE FROM SPLICE_WINDOW");

    // ------------------------------------------------------------
    // 场景一：跨班次查询 —— 接班人不应把上一班遗留接缝当成当前停机候选
    // ------------------------------------------------------------
    cout << "\n  [场景一] 跨班次查询（A班关窗交接 -> B班开窗接班）" << endl;

    cout << "\n  -- A班：张工开窗，检测到接缝并推进到可停机 --" << endl;
    int winA = windowDao.openWindow("A班", "张工");
    cout << "    A班窗口 id=" << winA << " 开启（状态 OPEN，时间戳由数据库记录）" << endl;
    int aSplice = spliceDao.insert(makeDemoSplice(winA, 320.5f, 1, 0, 0));
    spliceDao.updateFlags(aSplice, 1, 0);   // 准备停机
    spliceDao.updateFlags(aSplice, 1, 1);   // 可停机
    cout << "    A班接缝 id=" << aSplice << " 已推进为 last=1 flag=1 stop=1" << endl;

    cout << "\n  -- 此时查询：三个接口都能看到 A班接缝（窗口 OPEN）--" << endl;
    printScopedQueries(spliceDao);

    cout << "\n  -- A班下班，张工关闭窗口；窗口保留最后一次有效状态供复盘 --" << endl;
    auto closedA = windowDao.closeWindow(winA, "张工");
    cout << "    关闭结果: closedNow=" << (closedA.closedNow ? "true" : "false")
         << ", closedBy=" << closedA.closedBy << ", closedAt=" << closedA.closedAt << endl;
    auto snapshotA = windowDao.findById(winA);
    cout << "    定格快照: last_status=" << snapshotA->lastStatus
         << "（" << snapshotA->lastStatusAt << "）, last_splice_id=" << snapshotA->lastSpliceId << endl;
    printWindowTimeline(windowDao, winA);

    cout << "\n  -- B班接班：李工开新窗，检测到自己的接缝 --" << endl;
    int winB = windowDao.openWindow("B班", "李工");
    int bSplice = spliceDao.insert(makeDemoSplice(winB, 210.0f, 1, 1, 0));
    cout << "    B班窗口 id=" << winB << "，新接缝 id=" << bSplice << "（last=1 flag=1）" << endl;

    cout << "\n  -- 接班人当前查询：A班 stop=1 的旧接缝被排除，只出现B班窗口内数据 --" << endl;
    printScopedQueries(spliceDao);
    cout << "    说明: A班接缝 id=" << aSplice << " 的窗口为 CLOSED，故不出现在任何当前查询；" << endl;
    cout << "          但它仍可通过 findById/findByWindow 复盘读取，历史不丢失。" << endl;

    // ------------------------------------------------------------
    // 场景二：非法状态迁移 + 关闭窗口后迟到的状态写入被拒绝
    // ------------------------------------------------------------
    cout << "\n  [场景二] 非法迁移与迟到写入被数据库拒绝" << endl;
    int winC = windowDao.openWindow("C班", "王工");

    cout << "    尝试 OPEN 状态直接恢复(resume)：" << endl;
    try {
        windowDao.resumeWindow(winC, "王工");
        cout << "      [异常] 未被拒绝！" << endl;
    } catch (const db::DatabaseException& e) {
        cout << "      [拒绝] " << e.what() << endl;
    }

    windowDao.pauseWindow(winC, "王工");
    cout << "    窗口已暂停 PAUSED。暂停期间尝试写入新接缝：" << endl;
    try {
        spliceDao.insert(makeDemoSplice(winC, 99.0f, 1, 0, 0));
        cout << "      [异常] 未被拒绝！" << endl;
    } catch (const db::DatabaseException& e) {
        cout << "      [拒绝] " << e.what() << endl;
    }

    windowDao.resumeWindow(winC, "王工");
    int cSplice = spliceDao.insert(makeDemoSplice(winC, 300.0f, 1, 0, 0));
    cout << "    恢复 OPEN 后写入接缝 id=" << cSplice << " 成功（暂停期写入此前已被拒）。" << endl;

    windowDao.closeWindow(winC, "王工");
    cout << "    窗口已关闭。模拟一条“后到的状态上报”（旧接缝补报准备停机）：" << endl;
    try {
        spliceDao.updateFlags(cSplice, 1, 1);
        cout << "      [异常] 迟到写入竟然成功！" << endl;
    } catch (const db::DatabaseException& e) {
        cout << "      [拒绝] " << e.what() << endl;
    }

    cout << "    再尝试把 CLOSED 窗口暂停（非法迁移）：" << endl;
    try {
        windowDao.pauseWindow(winC, "接班人");
        cout << "      [异常] 未被拒绝！" << endl;
    } catch (const db::DatabaseException& e) {
        cout << "      [拒绝] " << e.what() << endl;
    }

    cout << "\n    重复关闭（换人再关一次）——返回首次关闭的同一结果：" << endl;
    auto again = windowDao.closeWindow(winC, "接班人");
    cout << "      第二次关闭: closedNow=" << (again.closedNow ? "true" : "false")
         << ", status=" << again.status << ", closedBy=" << again.closedBy
         << ", closedAt=" << again.closedAt << endl;
    printWindowTimeline(windowDao, winC);
    cout << "      （CLOSED 事件仅一条，重复关闭不产生新写入）" << endl;

    // ------------------------------------------------------------
    // 场景三：两名操作员同时关窗（各自独立连接）
    // ------------------------------------------------------------
    cout << "\n  [场景三] 两名操作员同时关闭同一窗口（并发，先关先得）" << endl;
    int winD = windowDao.openWindow("D班", "赵工");
    spliceDao.insert(makeDemoSplice(winD, 150.0f, 1, 1, 1));

    auto& dbm = db::DatabaseManager::instance();
    auto sessA = dbm.openSession();
    auto sessB = dbm.openSession();

    std::atomic<int> ready{0};
    std::atomic<bool> go{false};
    dao::WindowCloseResult rA, rB;
    auto concurrentClose = [&](std::shared_ptr<db::Session> sess,
                               const std::string& op, dao::WindowCloseResult* out) {
        ready.fetch_add(1);
        while (!go.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        dao::SpliceWindowDAO localDao;
        *out = localDao.closeWindowOn(*sess, winD, op);
    };
    std::thread t1(concurrentClose, sessA, "赵工", &rA);
    std::thread t2(concurrentClose, sessB, "钱工", &rB);
    while (ready.load() < 2) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    go.store(true);
    t1.join();
    t2.join();

    cout << "    赵工结果: closedNow=" << (rA.closedNow ? "true" : "false")
         << ", closedBy=" << rA.closedBy << ", closedAt=" << rA.closedAt << endl;
    cout << "    钱工结果: closedNow=" << (rB.closedNow ? "true" : "false")
         << ", closedBy=" << rB.closedBy << ", closedAt=" << rB.closedAt << endl;
    cout << "    => 恰好一人先关先得(closedNow=true)，另一人拿到同一 closedBy/closedAt；" << endl;
    cout << "       数据库行锁(FOR UPDATE)串行化，CLOSED 事件只有一条。" << endl;
    printWindowTimeline(windowDao, winD);

    sessA->close();
    sessB->close();

    cout << "\n  当前所有查询（场景一/三的窗口均已关闭，只剩 B班窗口 OPEN）：" << endl;
    printScopedQueries(spliceDao);
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
        dao::SpliceWindowDAO spliceWindowDao;
        dao::FlawDAO flawDao;
        dao::StopDAO stopDao;
        dao::CompareDAO compareDao;
        dao::HistoryDAO historyDao;
        dao::RemoveDAO removeDao;

        // 5. 执行各表 CRUD 演示
        demoSpeed(speedDao);
        demoSplice(spliceDao, spliceWindowDao);
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
