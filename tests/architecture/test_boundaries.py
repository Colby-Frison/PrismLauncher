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
    def test_illegal_include_fails_against_an_empty_baseline(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            write_source(root, "launcher/modplatform/adapter.h", "ui/dialogs/BlockedModsDialog.h")
            violations = checker.find_violations(root)
            new_violations, stale = checker.compare(violations, set())
            self.assertEqual(
                new_violations,
                ['launcher/modplatform/adapter.h: ui/dialogs/BlockedModsDialog.h'],
            )
            self.assertEqual(stale, [])

    def test_ui_may_include_other_layers(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            write_source(root, "launcher/ui/window.cpp", "minecraft/MinecraftInstance.h")
            violations = checker.find_violations(root)
            new_violations, stale = checker.compare(violations, set())
            self.assertEqual(new_violations, [])
            self.assertEqual(stale, [])

    def test_stale_baseline_entry_fails(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            write_source(root, "launcher/net/job.cpp", "net/NetJob.h")
            violations = checker.find_violations(root)
            baseline = {"launcher/net/job.cpp: minecraft/MinecraftInstance.h"}
            new_violations, stale = checker.compare(violations, baseline)
            self.assertEqual(new_violations, [])
            self.assertEqual(stale, ["launcher/net/job.cpp: minecraft/MinecraftInstance.h"])


if __name__ == "__main__":
    unittest.main()
