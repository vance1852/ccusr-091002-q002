# 工业检测系统 - C++ MySQL 数据访问层

## 运行

使用 Docker Compose 启动 MySQL 和应用：

```bash
docker compose up --build -d
docker compose run --rm test
```

查看应用输出：

```bash
docker compose logs -f backend
```

本地构建需要 CMake 3.16、C++17 编译器和 MySQL Connector/C。进入 `backend` 后执行 `cmake ..` 和 `cmake --build .`，连接参数通过 `DB_HOST`、`DB_PORT`、`DB_USER`、`DB_PASSWORD`、`DB_NAME` 环境变量提供。生产环境应从运行时安全存储注入密码，不要把凭据提交到仓库或打印到日志。

## 目录职责

`backend/src/entity` 定义检测数据结构，`backend/src/dao` 封装各业务表的数据访问，`backend/src/db` 管理 MySQL 连接和查询，`backend/src/utils` 提供日志，`backend/src/test` 保存回归测试。`backend/sql/schema.sql` 是数据库初始化脚本，`docker-compose.yml` 描述本地服务依赖。

## 数据范围

初始化脚本创建 SPEED、SPLICE、FLAW、STOP、COMPARE、HISTORY、REMOVE 七张业务表，以及 SPLICE_WINDOW（检测窗口）和 SPLICE_WINDOW_EVENT（窗口状态迁移审计）两张窗口表。字段含义、默认值和索引以 SQL 脚本为准；应用通过 DAO 执行增删改查并在关键操作处记录日志。

## 接缝检测窗口

每条接缝记录都属于一个检测窗口（`SPLICE.window_id`）。窗口状态机为 `OPEN -> PAUSED -> OPEN ... -> CLOSED`（CLOSED 为终态），每次迁移都带起止时间并写入审计表。约束由数据库（唯一索引、CHECK、触发器）和 SpliceDAO 共同承担：

- 同一时刻至多一个 OPEN 窗口（`open_singleton` 生成列唯一索引）。
- 当前接头、准备停机、可停机查询只返回 OPEN 窗口内的数据；暂停/关闭窗口的记录被排除，但 `findById`/`findAll` 不受窗口过滤，已关闭窗口的最后有效状态仍可复盘。
- 窗口关闭（或暂停）后，迟到的状态写入被 DAO 以 `WindowStateException` 拒绝；绕过 DAO 的直接 SQL 由触发器拒绝。
- 关闭是幂等的：重复关闭（含两名操作员并发关闭）返回同一结果，审计表恰好记录一次 CLOSE。
- 非法迁移（如恢复已关闭窗口）在 DAO 与数据库触发器两层均被拒绝。

`SpliceDAO` 提供 `openWindow` / `pauseWindow` / `resumeWindow` / `closeWindow` / `findWindow` / `findCurrentWindow` / `windowEvents` / `findAllWithWindow`（交接看板视图）。应用入口演示了完整的交接班场景：跨班次查询、非法迁移、迟到写入与两名操作员同时关窗。

注意：窗口表带外键与触发器，本地已有数据卷的情况下需 `docker compose down -v` 后重新初始化才能应用新 schema。
