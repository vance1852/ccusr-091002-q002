# 工业检测系统 - C++ MySQL 数据访问层

## 1. 系统架构

```mermaid
flowchart TD
    A[C++ Application Main] --> B[DatabaseManager]
    B --> C[MySQL Connector/C++]
    C --> D[(MySQL 8.0)]
    
    A --> E[SpeedDAO]
    A --> F[SpliceDAO]
    A --> G[FlawDAO]
    A --> H[StopDAO]
    A --> I[CompareDAO]
    A --> J[HistoryDAO]
    A --> K[RemoveDAO]
    
    E --> B
    F --> B
    G --> B
    H --> B
    I --> B
    J --> B
    K --> B
```

## 2. ER 图

```mermaid
erDiagram
    SPEED {
        INT id PK "主键"
        FLOAT value "速度值"
        VARCHAR date "日期"
        TINYINT flag "使用标志"
    }
    SPLICE {
        INT id PK "主键"
        FLOAT location "当前位置"
        FLOAT distance "距离维修区距离"
        VARCHAR time "倒计时时间"
        TEXT url "保存路径"
        TINYINT last "当前检测接头标志"
        TINYINT flag "准备标志"
        TINYINT stop "停机标志"
        INT window_id FK "所属检测窗口"
    }
    SPLICE_WINDOW {
        INT id PK "主键"
        VARCHAR shift_code "班次"
        VARCHAR operator "开窗操作员"
        ENUM status "OPEN/PAUSED/CLOSED"
        DATETIME opened_at "开启时间"
        DATETIME paused_at "暂停时间"
        DATETIME resumed_at "恢复时间"
        DATETIME closed_at "关闭时间(终态)"
        VARCHAR closed_by "实际关窗人"
        VARCHAR last_status "最后有效状态快照"
        INT last_splice_id "最后有效状态接缝"
    }
    SPLICE_WINDOW_EVENT {
        BIGINT id PK "主键"
        INT window_id FK "所属窗口"
        ENUM event "OPENED/PAUSED/RESUMED/CLOSED"
        DATETIME event_at "事件时间"
        VARCHAR operator "操作员"
    }
    FLAW {
        BIGINT id PK "主键"
        VARCHAR category "损伤类型"
        INT level "损伤级别"
        TEXT url "保存路径"
        INT camera "摄像头编号"
        FLOAT location "当前位置"
        FLOAT distance "距离维修区距离"
        VARCHAR size "损伤尺寸"
        VARCHAR coordinate "损伤坐标"
        VARCHAR date "记录日期"
        FLOAT time "倒计时时间"
        TINYINT flag "准备标志"
        TINYINT stop "停机标志"
        INT epoch "追踪圈数"
    }
    STOP {
        BIGINT id PK "主键"
        INT category "损伤类型"
        FLOAT distance "距离维修区距离"
        TINYINT flag "停机标志"
        TINYINT command "停机命令标志"
    }
    COMPARE {
        BIGINT id PK "主键"
        TEXT new_url "较新对比记录"
        TEXT old_url "较旧对比记录"
        FLOAT value "对比结果"
        INT category "类型"
        INT level "结果级别"
        VARCHAR old_size "较旧记录尺寸"
    }
    HISTORY {
        BIGINT id PK "主键"
        VARCHAR category "损伤类型"
        INT level "损伤级别"
        TEXT url "保存路径"
        INT camera "摄像头编号"
        VARCHAR size "损伤尺寸"
        VARCHAR date "记录日期"
    }
    REMOVE {
        BIGINT id PK "移除记录ID"
    }

    SPLICE_WINDOW ||--o{ SPLICE : "窗口包含接缝"
    SPLICE_WINDOW ||--o{ SPLICE_WINDOW_EVENT : "窗口生命周期轨迹"
    FLAW ||--o{ STOP : "损伤触发停机"
    SPLICE ||--o{ STOP : "接缝触发停机"
    FLAW ||--o{ COMPARE : "损伤对比"
    SPLICE ||--o{ COMPARE : "接缝对比"
    FLAW ||--o{ HISTORY : "损伤归档"
    FLAW ||--o{ REMOVE : "损伤移除"
```

## 3. 模块清单

| 模块 | 文件 | 职责 |
|------|------|------|
| DatabaseManager | db/DatabaseManager.h/.cpp | 连接池管理、SQL执行 |
| Entity | entity/*.h | 各表实体类定义 |
| DAO | dao/*.h/*.cpp | 各表CRUD操作 |
| Logger | utils/Logger.h/.cpp | 日志记录 |
| Main | main.cpp | 入口与演示 |

## 4. 技术选型

- C++17
- MySQL Connector/C++ 8.0 (X DevAPI / Legacy C API)
- CMake 3.16+
- spdlog (日志，可选，本项目使用自实现轻量Logger)

## 5. 接缝检测窗口生命周期

为解决跨班次时“上一班遗留接缝被误认成当前停机候选”的问题，SPLICE 记录不再靠零散标志位表达时效，而是归属于一个有明确状态与起止时间的**检测窗口**。

### 5.1 状态机

```
OPEN(开启/检测中) ──pause──> PAUSED(暂停)
PAUSED            ──resume─> OPEN
OPEN / PAUSED     ──close──> CLOSED(关闭，终态)
```

- 只有 **OPEN** 窗口接受 SPLICE 插入与 `last/flag/stop` 状态写入。
- PAUSED 窗口拒绝新数据；CLOSED 窗口拒绝一切写入和状态迁移。
- 约束由数据库触发器强制（`trg_splice_window_bu`、`trg_splice_bi`、`trg_splice_bu`），绕过 DAO 也无法写入。

### 5.2 查询范围

`findActive / findReadyToStop / findStoppable` 通过窗口状态过滤：只返回**所在窗口 OPEN** 的接缝，以及未纳入窗口管理的历史数据（`window_id IS NULL`，保持旧行为）。CLOSED 窗口记录不出现在当前查询，但仍可经 `findById / findAll / findByWindow` 复盘读取，既有读取接口签名不变。

### 5.3 关闭语义

- 关闭使用 `SELECT ... FOR UPDATE` 行锁事务：先关先得（`closed_by` 记录实际关窗人）。
- **幂等**：重复关闭返回首次关闭的同一 `closedBy/closedAt`，`closedNow=false`，不产生新事件或写入。
- 两名操作员用各自连接并发关窗时由行锁串行化，恰好一人成功。
- 窗口保存 `last_status/last_status_at/last_splice_id` 快照（由 AFTER 触发器维护），关闭后定格，供接班人复盘“窗口内最后一次有效状态”。

### 5.4 复盘轨迹

`SPLICE_WINDOW_EVENT` 记录 OPENED/PAUSED/RESUMED/CLOSED 全部事件、时间与操作人；状态迁移由触发器在同事务内追加，时间戳取迁移时刻（`paused_at/resumed_at/closed_at`）。

