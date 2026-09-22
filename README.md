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

初始化脚本创建 SPEED、SPLICE、SPLICE_WINDOW、SPLICE_WINDOW_EVENT、FLAW、STOP、COMPARE、HISTORY 和 REMOVE 九张表。SPLICE 接缝归属于有明确状态（OPEN/PAUSED/CLOSED）与起止时间的检测窗口；只有 OPEN 窗口接受接缝写入并出现在当前接头/准备停机/可停机查询中，关闭窗口后迟到的写入由数据库触发器拒绝，重复关窗幂等返回首次结果，窗口关闭后定格最后一次有效状态并保留生命周期事件供跨班次复盘。字段含义、默认值和索引以 SQL 脚本为准；应用通过 DAO 执行增删改查并在关键操作处记录日志。
