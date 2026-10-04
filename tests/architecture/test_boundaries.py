import tempfile
import unittest
from pathlib import Path

import importlib.util

SCRIPT = Path(__file__).resolve().parents[2] / "tools" / "check_architecture_boundaries.py"
SPEC = importlib.util.spec_from_file_location("check_architecture_boundaries", SCRIPT)
checker = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(checker)


def write_source(root: Path, relative: str, include: str) -> None:
    path = root / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(f'#include "{include}"\n', encoding="utf-8")


class ArchitectureBoundaryTests(unittest.TestCase):
    def test_illegal_include_is_reported(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            write_source(root, "launcher/modplatform/adapter.h", "ui/dialogs/BlockedModsDialog.h")
            violations = checker.find_violations(root)
            self.assertEqual(
                violations,
                {"launcher/modplatform/adapter.h: ui/dialogs/BlockedModsDialog.h"},
            )

    def test_ui_may_include_other_layers(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            write_source(root, "launcher/ui/window.cpp", "minecraft/MinecraftInstance.h")
            violations = checker.find_violations(root)
            self.assertEqual(violations, set())

    def test_clean_tree_has_no_violations(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            write_source(root, "launcher/net/job.cpp", "net/NetJob.h")
            write_source(root, "launcher/tasks/work.cpp", "tasks/Task.h")
            write_source(root, "launcher/minecraft/instance.cpp", "minecraft/BaseInstance.h")
            violations = checker.find_violations(root)
            self.assertEqual(violations, set())


if __name__ == "__main__":
    unittest.main()
