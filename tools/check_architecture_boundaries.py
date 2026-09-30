#!/usr/bin/env python3
"""Fail when a new cross-layer include appears under launcher/.

The target architecture separates tasks and networking, the instance and
account domain, provider adapters, and UI. Existing violations are listed in
the baseline. The check fails if the tree gains a violation or a baseline
entry no longer exists.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

INCLUDE_RE = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)\1')
SOURCE_SUFFIXES = {".h", ".hpp", ".hh", ".c", ".cc", ".cpp", ".cxx"}

# First path component under launcher/. Account sources live in
# launcher/minecraft/auth and use the minecraft rule.
LAYER_RULES = {
    "tasks": {"ui", "modplatform", "minecraft"},
    "net": {"ui", "modplatform", "minecraft"},
    "minecraft": {"ui"},
    "modplatform": {"ui"},
    "ui": set(),
}


def source_layer(relative_path: str) -> str | None:
    parts = relative_path.split("/")
    if len(parts) < 3 or parts[0] != "launcher":
        return None
    layer = parts[1]
    if layer not in LAYER_RULES:
        return None
    return layer


def included_layer(source_relative: str, include_path: str) -> str | None:
    normalized = include_path.replace("\\", "/")
    if normalized.startswith("launcher/"):
        candidate = normalized
    elif normalized.startswith("."):
        source_dir = "/".join(source_relative.split("/")[:-1])
        candidate = _normalize(f"{source_dir}/{normalized}")
    else:
        candidate = f"launcher/{normalized}"
    return source_layer(candidate)


def _normalize(path: str) -> str:
    parts: list[str] = []
    for part in path.split("/"):
        if part in ("", "."):
            continue
        if part == "..":
            if parts:
                parts.pop()
            continue
        parts.append(part)
    return "/".join(parts)


def violation_id(source_relative: str, include_path: str) -> str:
    return f"{source_relative}: {include_path}"


def find_violations(root: Path) -> set[str]:
    launcher = root / "launcher"
    violations: set[str] = set()
    if not launcher.is_dir():
        return violations
    for path in launcher.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in SOURCE_SUFFIXES:
            continue
        relative = path.relative_to(root).as_posix()
        layer = source_layer(relative)
        if layer is None:
            continue
        forbidden = LAYER_RULES[layer]
        if not forbidden:
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        for line in text.splitlines():
            match = INCLUDE_RE.match(line)
            if match is None:
                continue
            include_path = match.group(2)
            target = included_layer(relative, include_path)
            if target in forbidden:
                violations.add(violation_id(relative, include_path))
    return violations


def read_baseline(path: Path) -> set[str]:
    if not path.is_file():
        raise FileNotFoundError(path)
    entries: set[str] = set()
    for line in path.read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        entries.add(stripped)
    return entries


def compare(current: set[str], baseline: set[str]) -> tuple[list[str], list[str]]:
    new_violations = sorted(current - baseline)
    stale_entries = sorted(baseline - current)
    return new_violations, stale_entries


def write_baseline(path: Path, violations: set[str]) -> None:
    lines = [
        "# Existing cross-layer includes. Remove a line when the include is fixed.",
        "# A new include that breaks the layer rules must be removed, not added here.",
        *sorted(violations),
        "",
    ]
    path.write_text("\n".join(lines), encoding="utf-8", newline="\n")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument(
        "--baseline",
        type=Path,
        default=Path(__file__).resolve().parent / "architecture_boundaries_baseline.txt",
    )
    parser.add_argument("--write-baseline", action="store_true")
    args = parser.parse_args(argv)

    root = args.root.resolve()
    current = find_violations(root)
    if args.write_baseline:
        write_baseline(args.baseline, current)
        print(f"Wrote {len(current)} baseline entries to {args.baseline}")
        return 0

    baseline = read_baseline(args.baseline)
    new_violations, stale_entries = compare(current, baseline)
    if not new_violations and not stale_entries:
        print(f"Architecture boundaries match the baseline ({len(current)} known includes).")
        return 0

    if new_violations:
        print("New cross-layer includes:", file=sys.stderr)
        for entry in new_violations:
            print(f"  {entry}", file=sys.stderr)
    if stale_entries:
        print("Baseline entries that are no longer present:", file=sys.stderr)
        for entry in stale_entries:
            print(f"  {entry}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
