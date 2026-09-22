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

-- 接缝检测窗口表
-- 一个窗口对应一段连续有效的检测区间：开启(OPEN) -> 暂停(PAUSED) -> 恢复(OPEN) -> 关闭(CLOSED)
-- CLOSED 为终态；窗口状态决定接缝数据能否写入以及是否出现在当前业务查询中
DROP TABLE IF EXISTS SPLICE;
DROP TABLE IF EXISTS SPLICE_WINDOW_EVENT;
DROP TABLE IF EXISTS SPLICE_WINDOW;
CREATE TABLE SPLICE_WINDOW (
    id             INT AUTO_INCREMENT PRIMARY KEY COMMENT '主键',
    shift_code     VARCHAR(32) NOT NULL COMMENT '开启窗口的班次（跨班次交接依据）',
    operator       VARCHAR(64) NOT NULL COMMENT '开启窗口的操作员',
    status         ENUM('OPEN','PAUSED','CLOSED') NOT NULL DEFAULT 'OPEN' COMMENT '窗口状态：OPEN开启/PAUSED暂停/CLOSED关闭(终态)',
    opened_at      DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) COMMENT '开启时间',
    paused_at      DATETIME(3) NULL COMMENT '最近一次暂停时间',
    resumed_at     DATETIME(3) NULL COMMENT '最近一次恢复时间',
    closed_at      DATETIME(3) NULL COMMENT '关闭时间（终态时间戳）',
    closed_by      VARCHAR(64) NULL COMMENT '实际执行关闭的操作员（先关先得）',
    actor          VARCHAR(64) NULL COMMENT '最近一次迁移(暂停/恢复/关闭)的操作员',
    last_status    VARCHAR(16) NULL COMMENT '窗口内最后一次有效状态: DETECTED/READY/STOPPABLE/IDLE，关闭后定格供复盘',
    last_status_at DATETIME(3) NULL COMMENT '最后一次有效状态写入时间',
    last_splice_id INT NULL COMMENT '最后一次有效状态对应的接缝记录',
    KEY idx_window_status (status),
    KEY idx_window_shift (shift_code)
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4 COMMENT = '接缝检测窗口表';

-- 窗口生命周期事件表：开启/暂停/恢复/关闭的完整轨迹，供跨班次复盘
CREATE TABLE SPLICE_WINDOW_EVENT (
    id        BIGINT AUTO_INCREMENT PRIMARY KEY COMMENT '主键',
    window_id INT NOT NULL COMMENT '所属窗口',
    event     ENUM('OPENED','PAUSED','RESUMED','CLOSED') NOT NULL COMMENT '生命周期事件',
    event_at  DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) COMMENT '事件时间',
    operator  VARCHAR(64) NOT NULL DEFAULT '' COMMENT '执行该操作的操作员',
    KEY idx_event_window (window_id),
    CONSTRAINT fk_event_window FOREIGN KEY (window_id)
        REFERENCES SPLICE_WINDOW(id) ON DELETE CASCADE
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4 COMMENT = '接缝窗口生命周期事件表';

-- 接缝表
CREATE TABLE SPLICE (
    id        INT AUTO_INCREMENT PRIMARY KEY COMMENT '主键',
    location  FLOAT NOT NULL COMMENT '当前位置',
    distance  FLOAT NOT NULL COMMENT '距离维修区距离',
    time      VARCHAR(32) NOT NULL COMMENT '倒计时时间（秒）',
    url       TEXT NOT NULL COMMENT '保存路径',
    last      TINYINT DEFAULT 0 COMMENT '当前检测接头标志，1有效',
    flag      TINYINT DEFAULT 0 COMMENT '准备标志，不为0则准备停机',
    stop      TINYINT DEFAULT 0 COMMENT '停机标志，不为0则可以停机',
    window_id INT NULL COMMENT '所属检测窗口；NULL为未纳入窗口管理的历史数据',
    KEY idx_splice_window (window_id),
    CONSTRAINT fk_splice_window FOREIGN KEY (window_id)
        REFERENCES SPLICE_WINDOW(id) ON DELETE RESTRICT
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4 COMMENT = '接缝表';

-- --------------------------------------------
-- 窗口状态机触发器（数据库层硬约束）
-- --------------------------------------------
DELIMITER //

-- 开窗即记录 OPENED 事件
CREATE TRIGGER trg_splice_window_ai
AFTER INSERT ON SPLICE_WINDOW
FOR EACH ROW
BEGIN
    INSERT INTO SPLICE_WINDOW_EVENT (window_id, event, event_at, operator)
    VALUES (NEW.id, 'OPENED', NEW.opened_at, NEW.operator);
END//

-- 窗口状态迁移：只允许 OPEN->PAUSED、PAUSED->OPEN、OPEN/PAUSED->CLOSED；CLOSED 为终态
CREATE TRIGGER trg_splice_window_bu
BEFORE UPDATE ON SPLICE_WINDOW
FOR EACH ROW
BEGIN
    IF NEW.status <> OLD.status THEN
        IF OLD.status = 'CLOSED' THEN
            SIGNAL SQLSTATE '45000'
                SET MESSAGE_TEXT = 'SPLICE_WINDOW rejected: CLOSED is terminal, no transition allowed';
        ELSEIF OLD.status = 'OPEN' AND NEW.status = 'PAUSED' THEN
            SET NEW.paused_at = NOW(3);
        ELSEIF OLD.status = 'PAUSED' AND NEW.status = 'OPEN' THEN
            SET NEW.resumed_at = NOW(3);
        ELSEIF NEW.status = 'CLOSED' THEN
            SET NEW.closed_at = NOW(3);
        ELSE
            SIGNAL SQLSTATE '45000'
                SET MESSAGE_TEXT = 'SPLICE_WINDOW rejected: illegal state transition';
        END IF;
    END IF;
END//

-- 迁移成功后追加事件（重复关闭不改状态，不会产生重复 CLOSED 事件）
CREATE TRIGGER trg_splice_window_au
AFTER UPDATE ON SPLICE_WINDOW
FOR EACH ROW
BEGIN
    IF NEW.status <> OLD.status THEN
        INSERT INTO SPLICE_WINDOW_EVENT (window_id, event, event_at, operator)
        VALUES (NEW.id,
                CASE NEW.status
                    WHEN 'PAUSED' THEN 'PAUSED'
                    WHEN 'OPEN'   THEN 'RESUMED'
                    WHEN 'CLOSED' THEN 'CLOSED'
                END,
                CASE NEW.status
                    WHEN 'PAUSED' THEN NEW.paused_at
                    WHEN 'OPEN'   THEN NEW.resumed_at
                    WHEN 'CLOSED' THEN NEW.closed_at
                END,
                COALESCE(NEW.actor, NEW.closed_by, ''));
    END IF;
END//

-- 接缝插入：只有 OPEN 窗口接受数据；PAUSED/CLOSED 窗口一律拒绝
CREATE TRIGGER trg_splice_bi
BEFORE INSERT ON SPLICE
FOR EACH ROW
BEGIN
    DECLARE v_status VARCHAR(16) DEFAULT NULL;
    IF NEW.window_id IS NOT NULL THEN
        SELECT status INTO v_status FROM SPLICE_WINDOW WHERE id = NEW.window_id;
        IF v_status IS NULL THEN
            SIGNAL SQLSTATE '45000'
                SET MESSAGE_TEXT = 'SPLICE insert rejected: window does not exist';
        ELSEIF v_status <> 'OPEN' THEN
            SIGNAL SQLSTATE '45000'
                SET MESSAGE_TEXT = 'SPLICE insert rejected: window is not OPEN (paused or closed)';
        END IF;
    END IF;
END//

-- 接缝状态写入(last/flag/stop)：窗口暂停或关闭后拒绝，关闭窗口后迟到的状态更新无法落库
CREATE TRIGGER trg_splice_bu
BEFORE UPDATE ON SPLICE
FOR EACH ROW
BEGIN
    DECLARE v_status VARCHAR(16) DEFAULT NULL;
    IF NEW.window_id IS NOT NULL
       AND (NEW.last <> OLD.last OR NEW.flag <> OLD.flag OR NEW.stop <> OLD.stop) THEN
        SELECT status INTO v_status FROM SPLICE_WINDOW WHERE id = NEW.window_id;
        IF v_status IS NOT NULL AND v_status <> 'OPEN' THEN
            SIGNAL SQLSTATE '45000'
                SET MESSAGE_TEXT = 'SPLICE state write rejected: window is not OPEN (paused or closed)';
        END IF;
    END IF;
END//

-- 维护窗口内“最后一次有效状态”，窗口关闭后写入已在 BEFORE 触发器被拒，该快照随之定格供复盘
CREATE TRIGGER trg_splice_ai_snapshot
AFTER INSERT ON SPLICE
FOR EACH ROW
BEGIN
    IF NEW.window_id IS NOT NULL THEN
        UPDATE SPLICE_WINDOW
           SET last_status_at = NOW(3),
               last_status = CASE
                   WHEN NEW.stop <> 0 THEN 'STOPPABLE'
                   WHEN NEW.flag <> 0 THEN 'READY'
                   WHEN NEW.last <> 0 THEN 'DETECTED'
                   ELSE 'IDLE'
               END,
               last_splice_id = NEW.id
         WHERE id = NEW.window_id;
    END IF;
END//

CREATE TRIGGER trg_splice_au_snapshot
AFTER UPDATE ON SPLICE
FOR EACH ROW
BEGIN
    IF NEW.window_id IS NOT NULL THEN
        UPDATE SPLICE_WINDOW
           SET last_status_at = NOW(3),
               last_status = CASE
                   WHEN NEW.stop <> 0 THEN 'STOPPABLE'
                   WHEN NEW.flag <> 0 THEN 'READY'
                   WHEN NEW.last <> 0 THEN 'DETECTED'
                   ELSE 'IDLE'
               END,
               last_splice_id = NEW.id
         WHERE id = NEW.window_id;
    END IF;
END//

DELIMITER ;

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
