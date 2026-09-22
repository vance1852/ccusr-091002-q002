-- ============================================
-- 工业检测系统 数据库初始化脚本
-- ============================================

CREATE DATABASE IF NOT EXISTS industrial_inspection
    DEFAULT CHARACTER SET utf8mb4
    DEFAULT COLLATE utf8mb4_unicode_ci;

USE industrial_inspection;

-- 速度表
DROP TABLE IF EXISTS SPEED;
CREATE TABLE SPEED (
    id        INT AUTO_INCREMENT PRIMARY KEY COMMENT '主键',
    value     FLOAT NOT NULL COMMENT '速度值',
    date      VARCHAR(32) NOT NULL COMMENT '日期',
    flag      TINYINT DEFAULT 0 COMMENT '使用标志，1为已使用（废弃）'
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4 COMMENT = '速度表';

-- ============================================
-- 接缝检测窗口
-- 状态机：OPEN -> PAUSED -> OPEN ... -> CLOSED（终态）
-- 约束说明：
--   1. 同一时刻全库只允许一个 OPEN 窗口（open_singleton 唯一索引）
--   2. 状态迁移合法性由触发器 trg_window_state_guard 保证
--   3. 窗口关闭后 closed_at 由数据库补齐，且状态不可再变更
-- ============================================
-- 外键依赖：先删子表，再删父表
DROP TABLE IF EXISTS SPLICE_WINDOW_EVENT;
DROP TABLE IF EXISTS SPLICE;
DROP TABLE IF EXISTS SPLICE_WINDOW;

CREATE TABLE SPLICE_WINDOW (
    id             INT AUTO_INCREMENT PRIMARY KEY COMMENT '主键',
    label          VARCHAR(64) NOT NULL COMMENT '窗口标签（班次/产线）',
    state          VARCHAR(8) NOT NULL DEFAULT 'OPEN' COMMENT '窗口状态：OPEN/PAUSED/CLOSED',
    opened_at      DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) COMMENT '开启时间',
    closed_at      DATETIME(3) NULL DEFAULT NULL COMMENT '关闭时间，未关闭为NULL',
    version        INT NOT NULL DEFAULT 0 COMMENT '状态版本号，每次迁移加1',
    open_singleton TINYINT GENERATED ALWAYS AS (IF(state = 'OPEN', 1, NULL)) STORED COMMENT 'OPEN窗口唯一占位',
    CONSTRAINT chk_window_state CHECK (state IN ('OPEN', 'PAUSED', 'CLOSED')),
    UNIQUE KEY uq_window_open_singleton (open_singleton)
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4 COMMENT = '接缝检测窗口表';

-- 窗口状态迁移审计表（开启/暂停/恢复/关闭的时间与操作员，供交接复盘）
CREATE TABLE SPLICE_WINDOW_EVENT (
    id         BIGINT AUTO_INCREMENT PRIMARY KEY COMMENT '主键',
    window_id  INT NOT NULL COMMENT '所属窗口',
    action     VARCHAR(8) NOT NULL COMMENT '动作：OPEN/PAUSE/RESUME/CLOSE',
    from_state VARCHAR(8) NOT NULL COMMENT '迁移前状态，OPEN事件记为 -',
    to_state   VARCHAR(8) NOT NULL COMMENT '迁移后状态',
    actor      VARCHAR(64) NOT NULL COMMENT '操作员',
    created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) COMMENT '发生时间',
    CONSTRAINT fk_window_event_window FOREIGN KEY (window_id) REFERENCES SPLICE_WINDOW (id)
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4 COMMENT = '接缝检测窗口状态迁移审计表';

-- 接缝表
CREATE TABLE SPLICE (
    id        INT AUTO_INCREMENT PRIMARY KEY COMMENT '主键',
    window_id INT NOT NULL COMMENT '所属检测窗口',
    location  FLOAT NOT NULL COMMENT '当前位置',
    distance  FLOAT NOT NULL COMMENT '距离维修区距离',
    time      VARCHAR(32) NOT NULL COMMENT '倒计时时间（秒）',
    url       TEXT NOT NULL COMMENT '保存路径',
    last      TINYINT DEFAULT 0 COMMENT '当前检测接头标志，1有效',
    flag      TINYINT DEFAULT 0 COMMENT '准备标志，不为0则准备停机',
    stop      TINYINT DEFAULT 0 COMMENT '停机标志，不为0则可以停机',
    CONSTRAINT fk_splice_window FOREIGN KEY (window_id) REFERENCES SPLICE_WINDOW (id)
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4 COMMENT = '接缝表';

-- 损伤表
DROP TABLE IF EXISTS FLAW;
CREATE TABLE FLAW (
    id         BIGINT AUTO_INCREMENT PRIMARY KEY COMMENT '主键',
    category   VARCHAR(64) NOT NULL COMMENT '损伤类型',
    level      INT NOT NULL COMMENT '损伤级别',
    url        TEXT NOT NULL COMMENT '损伤记录保存路径',
    camera     INT NOT NULL COMMENT '监控摄像头编号',
    location   FLOAT NOT NULL COMMENT '当前位置',
    distance   FLOAT NOT NULL COMMENT '距离维修区距离',
    size       VARCHAR(64) NOT NULL COMMENT '损伤尺寸',
    coordinate VARCHAR(64) NOT NULL COMMENT '损伤坐标',
    date       VARCHAR(32) NOT NULL COMMENT '记录日期',
    time       FLOAT NOT NULL COMMENT '倒计时时间（秒）',
    flag       TINYINT DEFAULT 0 COMMENT '准备标志，不为0则准备停机',
    stop       TINYINT DEFAULT 0 COMMENT '停机标志，不为0则可以停机',
    epoch      INT DEFAULT 0 COMMENT '追踪当前缺陷的圈数'
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4 COMMENT = '损伤表';

-- 停机表
DROP TABLE IF EXISTS STOP;
CREATE TABLE STOP (
    id       BIGINT AUTO_INCREMENT PRIMARY KEY COMMENT '长ID为损伤，短ID为接缝',
    category INT NOT NULL COMMENT '损伤类型',
    distance FLOAT NOT NULL COMMENT '距离维修区距离',
    flag     TINYINT DEFAULT 0 COMMENT '停机标志，1为允许停机',
    command  TINYINT DEFAULT 0 COMMENT '停机命令标志，1为下发'
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4 COMMENT = '停机表';

-- 对比表
DROP TABLE IF EXISTS COMPARE;
CREATE TABLE COMPARE (
    id       BIGINT AUTO_INCREMENT PRIMARY KEY COMMENT '长ID为损伤，短ID为接缝',
    new_url  TEXT NOT NULL COMMENT '较新对比记录',
    old_url  TEXT NOT NULL COMMENT '较旧对比记录',
    value    FLOAT NOT NULL COMMENT '对比结果',
    category INT NOT NULL COMMENT '类型',
    level    INT NOT NULL COMMENT '结果级别',
    old_size VARCHAR(64) NOT NULL COMMENT '较旧记录尺寸（只对损伤有效）'
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4 COMMENT = '对比表';

-- 历史表
DROP TABLE IF EXISTS HISTORY;
CREATE TABLE HISTORY (
    id       BIGINT AUTO_INCREMENT PRIMARY KEY COMMENT '长ID为损伤，短ID为接缝',
    category VARCHAR(64) NOT NULL COMMENT '损伤类型',
    level    INT NOT NULL COMMENT '损伤级别',
    url      TEXT NOT NULL COMMENT '损伤记录保存路径',
    camera   INT NOT NULL COMMENT '监控摄像头编号',
    size     VARCHAR(64) NOT NULL COMMENT '损伤尺寸',
    date     VARCHAR(32) NOT NULL COMMENT '记录日期'
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4 COMMENT = '历史表';

-- 移除表
DROP TABLE IF EXISTS REMOVE;
CREATE TABLE REMOVE (
    id BIGINT AUTO_INCREMENT PRIMARY KEY COMMENT '移除记录ID'
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4 COMMENT = '移除表';

-- ============================================
-- 窗口与接缝的数据库级约束（触发器）
-- 即使绕过 DAO 直接执行 SQL，也无法违反窗口状态机
-- ============================================
DELIMITER //

-- 窗口只能以 OPEN 状态创建
DROP TRIGGER IF EXISTS trg_window_insert_guard //
CREATE TRIGGER trg_window_insert_guard
BEFORE INSERT ON SPLICE_WINDOW
FOR EACH ROW
BEGIN
    IF NEW.state <> 'OPEN' THEN
        SIGNAL SQLSTATE '45000'
            SET MESSAGE_TEXT = 'SPLICE_WINDOW: new window must start in OPEN state';
    END IF;
END //

-- 窗口状态机守卫：
--   OPEN   -> PAUSED / CLOSED
--   PAUSED -> OPEN   / CLOSED
--   CLOSED -> 终态，任何变更都被拒绝
-- 关闭时自动补 closed_at；非关闭状态 closed_at 恒为 NULL
DROP TRIGGER IF EXISTS trg_window_state_guard //
CREATE TRIGGER trg_window_state_guard
BEFORE UPDATE ON SPLICE_WINDOW
FOR EACH ROW
BEGIN
    IF OLD.state = 'CLOSED' AND NEW.state <> 'CLOSED' THEN
        SIGNAL SQLSTATE '45000'
            SET MESSAGE_TEXT = 'SPLICE_WINDOW: CLOSED is terminal, transition rejected';
    END IF;
    IF OLD.state = 'OPEN' AND NEW.state NOT IN ('OPEN', 'PAUSED', 'CLOSED') THEN
        SIGNAL SQLSTATE '45000'
            SET MESSAGE_TEXT = 'SPLICE_WINDOW: illegal transition from OPEN';
    END IF;
    IF OLD.state = 'PAUSED' AND NEW.state NOT IN ('PAUSED', 'OPEN', 'CLOSED') THEN
        SIGNAL SQLSTATE '45000'
            SET MESSAGE_TEXT = 'SPLICE_WINDOW: illegal transition from PAUSED';
    END IF;
    IF NEW.state = 'CLOSED' AND NEW.closed_at IS NULL THEN
        SET NEW.closed_at = NOW(3);
    END IF;
    IF NEW.state <> 'CLOSED' THEN
        SET NEW.closed_at = NULL;
    END IF;
END //

-- 接缝插入守卫：仅当所属窗口处于 OPEN 时允许写入
DROP TRIGGER IF EXISTS trg_splice_insert_guard //
CREATE TRIGGER trg_splice_insert_guard
BEFORE INSERT ON SPLICE
FOR EACH ROW
BEGIN
    IF NOT EXISTS (SELECT 1 FROM SPLICE_WINDOW w
                   WHERE w.id = NEW.window_id AND w.state = 'OPEN') THEN
        SIGNAL SQLSTATE '45000'
            SET MESSAGE_TEXT = 'SPLICE: insert rejected, window not OPEN';
    END IF;
END //

-- 接缝更新守卫：窗口暂停/关闭后拒绝迟到的状态写入，
-- 已落库的标志位保持最后一次有效状态，供交接复盘
DROP TRIGGER IF EXISTS trg_splice_update_guard //
CREATE TRIGGER trg_splice_update_guard
BEFORE UPDATE ON SPLICE
FOR EACH ROW
BEGIN
    IF NOT EXISTS (SELECT 1 FROM SPLICE_WINDOW w
                   WHERE w.id = NEW.window_id AND w.state = 'OPEN') THEN
        SIGNAL SQLSTATE '45000'
            SET MESSAGE_TEXT = 'SPLICE: update rejected, window not OPEN';
    END IF;
END //

DELIMITER ;
