"""验证工业检测项目的基础构建与测试约定。"""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]


class RepositoryBaselineTest(unittest.TestCase):
    def test_cpp_test_target_is_registered(self):
        cmake = (ROOT / "backend" / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn("add_executable(IndustrialInspectionTest", cmake)
        self.assertIn("enable_testing()", cmake)
        self.assertIn("add_test(NAME AllTests", cmake)

    def test_existing_domain_layers_are_present(self):
        names = ("Speed", "Splice", "Flaw", "Stop", "Compare", "History", "Remove")
        for name in names:
            self.assertTrue((ROOT / "backend" / "src" / "entity" / f"{name}.h").is_file())
            self.assertTrue((ROOT / "backend" / "src" / "dao" / f"{name}DAO.h").is_file())

    def test_schema_initializes_all_existing_tables(self):
        schema = (ROOT / "backend" / "sql" / "schema.sql").read_text(encoding="utf-8")
        tables = set(re.findall(r"CREATE\s+TABLE\s+([A-Z_]+)", schema, re.IGNORECASE))
        self.assertTrue(
            {"SPEED", "SPLICE", "SPLICE_WINDOW", "SPLICE_WINDOW_EVENT",
             "FLAW", "STOP", "COMPARE", "HISTORY", "REMOVE"}
            <= {t.upper() for t in tables})

    def test_window_state_machine_is_enforced_in_schema(self):
        schema = (ROOT / "backend" / "sql" / "schema.sql").read_text(encoding="utf-8")
        # 窗口状态列必须是 OPEN/PAUSED/CLOSED 状态枚举
        self.assertRegex(schema, r"ENUM\('OPEN','PAUSED','CLOSED'\)")
        # 必须存在数据库层触发器：窗口迁移、接缝写入、接缝状态更新约束
        triggers = set(re.findall(r"CREATE\s+TRIGGER\s+(\w+)", schema, re.IGNORECASE))
        for required in ("trg_splice_window_bu", "trg_splice_bi", "trg_splice_bu"):
            self.assertIn(required, triggers)
        # SPLICE 必须关联窗口
        self.assertIn("fk_splice_window", schema)

    def test_window_dao_layer_is_present(self):
        self.assertTrue((ROOT / "backend" / "src" / "entity" / "SpliceWindow.h").is_file())
        self.assertTrue((ROOT / "backend" / "src" / "dao" / "SpliceWindowDAO.h").is_file())

    def test_splice_read_apis_keep_their_names(self):
        dao = (ROOT / "backend" / "src" / "dao" / "SpliceDAO.h").read_text(encoding="utf-8")
        for method in ("findActive", "findReadyToStop", "findStoppable"):
            self.assertRegex(dao, rf"\b{method}\s*\(")
        # 业务查询必须按窗口状态过滤，而不是只看标志位
        self.assertIn("SPLICE_WINDOW", dao)

    def test_compose_exposes_application_and_test_services(self):
        compose = (ROOT / "docker-compose.yml").read_text(encoding="utf-8")
        self.assertRegex(compose, r"(?m)^\s{2}mysql:\s*$")
        self.assertRegex(compose, r"(?m)^\s{2}backend:\s*$")
        self.assertRegex(compose, r"(?m)^\s{2}test:\s*$")


if __name__ == "__main__":
    unittest.main()
