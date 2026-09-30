Technical Quality

Group ID: (fill in before the Canvas post)

[x] Syntax & Execution: Course CI completed on a clean `ubuntu-24.04` runner in [run 36435815684](https://github.com/Colby-Frison/PrismLauncher/actions/runs/36435815684). Checkout, Qt and vcpkg setup, CMake configure, the Debug build, CTest, install, and artifact upload all succeeded. The workflow uses `actions/checkout@v7` and `actions/upload-artifact@v7`. Shell steps are short command lines, not scripts assembled from unpinned downloads.

[x] Task Completeness: The build job configures, compiles, runs `ctest --preset linux`, installs with `cmake --install`, and uploads `prismlauncher-linux-debug`. The architecture-boundaries job scans includes and runs the checker tests. None of those steps is an `echo` placeholder.

[x] Architectural Conformance: The target diagram separates tasks and networking, the instance and account domain, provider adapters, and UI. `architecture-boundaries` runs `tools/check_architecture_boundaries.py`, which fails if a new include crosses those layers or if a recorded exception disappears. The 17 includes that already cross a layer are listed in `tools/architecture_boundaries_baseline.txt`. `Launcher_logic` stays one library because the architecture notes say target extraction comes after service interfaces exist.

[x] Pipeline Maintainability: `.github/workflows/course-ci.yml` is two jobs with pinned action tags. The boundary job is checkout plus two `python3` commands and does not install Qt. The build job still uses the repository's existing setup action.

---

AI-Assisted Verification

[x] Origin Tracking: The first pipeline and this boundary scan were drafted by the assistant from the architecture docs and the checklist. The group chose the layer rules, kept the upstream workflows disabled, and rejected a four-library split. That split is recorded in `docs/ci-cd-pipeline-report.md`.

[x] Security & Action Auditing: The workflow file adds only `actions/checkout@v7` and `actions/upload-artifact@v7`. Both are published GitHub actions at major versions already used in this repository. The boundary job sets `contents: read` and calls no third-party action. The build job keeps `contents: read` and `packages: write` for the vcpkg cache. It does not request an id token and does not read signing secrets. No step pipes a remote script into a shell.

[x] Defect & Hallucination Remediation: The checker uses `--root`, `--baseline`, and `--write-baseline`. Those flags are implemented in the script and covered by `tests/architecture/test_boundaries.py` (illegal include, legal UI include, stale baseline entry). The local test run passed before the workflow was pushed.

[x] Team Comprehension: On a push to `develop` or a pull request, the boundary job and the Linux build start together. The boundary job fails if the include graph no longer matches the baseline. The build job fails if configure, compile, CTest, install, or the artifact upload fails. A boundary failure does not change the build job, and a build failure does not skip the scan.

[x] Human Oversight Gates: This workflow does not deploy to production and does not use credentials. The uploaded install tree is a build artifact, not a release. A release that signs or publishes a package would be a separate, human-triggered workflow. No architectural exception is accepted by the scanner unless a person edits the baseline file and reviews that commit.
