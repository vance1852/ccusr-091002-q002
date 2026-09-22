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
        INT window_id FK "所属检测窗口"
        FLOAT location "当前位置"
        FLOAT distance "距离维修区距离"
        VARCHAR time "倒计时时间"
        TEXT url "保存路径"
        TINYINT last "当前检测接头标志"
        TINYINT flag "准备标志"
        TINYINT stop "停机标志"
    }
    SPLICE_WINDOW {
        INT id PK "主键"
        VARCHAR label "窗口标签（班次/产线）"
        VARCHAR state "OPEN/PAUSED/CLOSED"
        DATETIME opened_at "开启时间"
        DATETIME closed_at "关闭时间，未关闭为NULL"
        INT version "状态版本号"
    }
    SPLICE_WINDOW_EVENT {
        BIGINT id PK "主键"
        INT window_id FK "所属窗口"
        VARCHAR action "OPEN/PAUSE/RESUME/CLOSE"
        VARCHAR from_state "迁移前状态"
        VARCHAR to_state "迁移后状态"
        VARCHAR actor "操作员"
        DATETIME created_at "发生时间"
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

    FLAW ||--o{ STOP : "损伤触发停机"
    SPLICE ||--o{ STOP : "接缝触发停机"
    FLAW ||--o{ COMPARE : "损伤对比"
    SPLICE ||--o{ COMPARE : "接缝对比"
    FLAW ||--o{ HISTORY : "损伤归档"
    FLAW ||--o{ REMOVE : "损伤移除"
    SPLICE_WINDOW ||--o{ SPLICE : "窗口包含接缝"
    SPLICE_WINDOW ||--o{ SPLICE_WINDOW_EVENT : "窗口状态迁移审计"
```

### 检测窗口状态机

```
OPEN --pause--> PAUSED --resume--> OPEN
OPEN --close--> CLOSED（终态）
PAUSED --close--> CLOSED
```

- 同一时刻至多一个 OPEN 窗口（生成列唯一索引 `uq_window_open_singleton`）。
- 运行查询（当前接头/准备停机/可停机）只返回 OPEN 窗口的数据；`findById`/`findAll` 不过滤，保留最后有效状态供复盘。
- 写入（插入/标志更新）仅 OPEN 窗口可写；关闭后迟到的写入由 DAO 与触发器双层拒绝。
- 关闭幂等：重复/并发关闭返回同一 `closed_at` 与 `version`，审计表恰好一条 CLOSE。
- 全部迁移由触发器 `trg_window_state_guard` 校验，并落入 `SPLICE_WINDOW_EVENT` 审计表。

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
