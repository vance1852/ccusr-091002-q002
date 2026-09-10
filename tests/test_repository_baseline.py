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
        tables = set(re.findall(r"CREATE\s+TABLE\s+([A-Z]+)", schema, re.IGNORECASE))
        self.assertTrue({"SPEED", "SPLICE", "FLAW", "STOP", "COMPARE", "HISTORY", "REMOVE"} <= {t.upper() for t in tables})

    def test_compose_exposes_application_and_test_services(self):
        compose = (ROOT / "docker-compose.yml").read_text(encoding="utf-8")
        self.assertRegex(compose, r"(?m)^\s{2}mysql:\s*$")
        self.assertRegex(compose, r"(?m)^\s{2}backend:\s*$")
        self.assertRegex(compose, r"(?m)^\s{2}test:\s*$")


if __name__ == "__main__":
    unittest.main()
