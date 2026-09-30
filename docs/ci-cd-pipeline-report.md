# CI/CD pipeline report

This write-up records how the course pipeline was designed, what the generated workflow does, and how it was tested. It follows the assignment in [CI-CD pipeline.md](CI-CD%20pipeline.md).

## Architecture review

Prism Launcher is a Qt/C++ desktop application. `launcher/main.cpp` constructs `Application`, which owns settings, networking, metadata, instances, accounts, and the updater. A Minecraft launch does not stay inside that process: `LauncherPartLaunch` starts a Java process whose entry point is `org.prismlauncher.EntryPoint` in `libraries/launcher`.

The pipeline has to build and test those pieces:

- Configure and compile the C++ application with the `linux` CMake preset, which uses vcpkg and Ninja.
- Compile the Java helper in the same CMake build. `libraries/launcher` and `libraries/javacheck` are subdirectories of the root project, so a separate Java job is unnecessary.
- Run the existing CTest suite. Those tests cover filesystem helpers, tasks, version and library parsing, pack metadata, and resource parsing. They exercise domain code without opening the main window or signing in.
- Publish a build the group can download. There is no server to deploy to, so the deploy step uploads the CMake install tree as a GitHub Actions artifact.

The workflow also has to account for constraints in the architecture and the build:

- Qt 6, Ninja, extra-cmake-modules, and the vcpkg submodule are required before configure.
- The host is Linux, matching the `linux` preset condition. Windows and macOS presets need different dependency setup and signing that this fork does not have.
- OAuth, mod-platform network calls, and the GUI are outside the unit tests. A green run does not claim those paths were executed.
- Package signing (GPG, Apple notarization, Azure Trusted Signing) is not part of this pipeline.

## AI step

### Prompts

1. Review `docs/CI-CD pipeline.md`, go through the assignment requirements, and create a plan to complete it.
2. Make our own CI/CD workflow, disable the current flows, and create our own.
3. Implement that plan: disable the upstream workflows, add the course pipeline, write this report, and run the workflow until the build, tests, and artifact upload succeed.

### What the assistant generated

The assistant read the architecture docs and the existing `.github/workflows/build.yml`, then proposed a single Ubuntu job: checkout with submodules, the existing dependency setup action, CMake configure and build, CTest, and an unsigned install artifact. After the second prompt, the plan also renamed the upstream workflow files so GitHub Actions would stop running them. The implemented file is `.github/workflows/course-ci.yml`.

### Decisions

- The eleven upstream workflow files were renamed from `*.yml` to `*.yml.disabled`. GitHub Actions only loads `.yml` and `.yaml` in `.github/workflows`, so those jobs no longer trigger. The files remain in the repository.
- Composite actions under `.github/actions/` were kept. The course workflow calls `setup-dependencies` instead of reinstalling Qt and system packages by hand.
- The job is one `ubuntu-24.04` Debug build using the `linux` preset and Qt 6.11.2, the same Qt version the upstream build file used.
- Triggers are pull requests, pushes to `develop` (the default branch of this fork), and manual `workflow_dispatch`.
- A new push cancels the in-progress run for the same ref.
- Permissions are `contents: read` and `packages: write` for the vcpkg cache. Signing token permissions were left out.
- Deploy is `actions/upload-artifact` of the `install` directory as `prismlauncher-linux-debug`. GPG signing is skipped.

## Test log

The workflow runs on GitHub-hosted Ubuntu. The completion check is a successful configure, a successful Debug build, a passing `ctest --preset linux` run, and an uploaded `prismlauncher-linux-debug` artifact.

Verification run: [Course CI #36435815684](https://github.com/Colby-Frison/PrismLauncher/actions/runs/36435815684), commit `38215ce42`, conclusion success.

| Step | Result |
| --- | --- |
| Checkout | success |
| Setup dependencies | success |
| Configure project | success |
| Run build | success |
| Run tests | success |
| Install | success |
| Upload install artifact | success (`prismlauncher-linux-debug`, about 135 MB) |

That push was the only workflow triggered for the commit. The renamed upstream files did not start. No compile, test, or upload failures occurred, so the workflow file was not changed after this run.

## Explanation notes

### What the pipeline does

On a pull request, a push to `develop`, or a manual run, GitHub Actions checks out the repository including submodules, installs Linux build dependencies and Qt, configures with the `linux` preset, builds Debug, runs CTest, installs into `install/`, and uploads that directory.

### Why the major steps exist

- Checkout with submodules brings in vcpkg, which the CMake preset sets as the toolchain file.
- Dependency setup installs the libraries the desktop app links against (Qt, Ninja, extra-cmake-modules, XCB, and related packages) and exports `VCPKG_TRIPLET`.
- Configure and build produce the `Application` binary and, through the Java subdirectory, `NewLaunch.jar` and `NewLaunchLegacy.jar`.
- CTest runs the headless domain tests that already exist under `tests/`.
- Install and upload are the deploy step for a desktop app that has no hosted service.

### How this relates to the architecture

The build is the composition root and the native-to-Java boundary from the architecture diagram: one C++ process (`Application`) and the Java entry point it launches. The tests match the domain models that diagram shows (tasks, versions, libraries, packs, resources) and do not start `MainWindow` or an online `AuthFlow`.

### What was generated and what was corrected

The workflow structure above is what the assistant generated from the architecture docs and the existing CMake presets. The correction from the group was to disable the upstream workflows and keep a single course-owned file as the pipeline that runs. The verification run passed without a further change to the workflow.

## Documented steps in detail

### Step 1: Review the system

We started from the Module 1 architecture in `docs/architecture.md` and `docs/architecture-diagrams.md`, and from the assignment in `docs/CI-CD pipeline.md`.

What the system is:

- `launcher/main.cpp` constructs `Application`, the composition root that owns settings, networking, metadata, instances, accounts, and the updater.
- A Minecraft launch leaves that process. `LauncherPartLaunch` starts Java and runs `org.prismlauncher.EntryPoint` from `libraries/launcher`.
- The same CMake project builds both parts. `CMakeLists.txt` adds `libraries/launcher` and `libraries/javacheck`.
- `tests/CMakeLists.txt` already has headless CTest coverage for filesystem helpers, tasks, versions, libraries, pack metadata, and resource parsing.

What the pipeline needs to do:

- Build the C++ application and the Java helper.
- Run those existing tests.
- Publish something downloadable. There is no server, so the deploy step is a GitHub Actions artifact of the CMake install tree.

What the workflow has to account for:

- The `linux` preset in `CMakePresets.json` needs Qt 6, Ninja, extra-cmake-modules, and the vcpkg submodule.
- Windows and macOS presets, plus GPG, Apple, and Azure signing, depend on secrets this fork does not have.
- The unit tests do not open `MainWindow`, run OAuth, or call mod-platform APIs.

We also inspected the eleven workflows already in `.github/workflows/`. They build, scan, package, publish, and release. Reusing them would not be a pipeline this group built, and the package steps would fail without signing secrets.

### Step 2: Ask AI to plan the pipeline

Prompt:

> Review `docs/CI-CD pipeline.md`. These are the instructions for an assignment. Go through the assignment requirements and create a plan to complete it.

What the assistant generated:

- A plan for one `ubuntu-24.04` job in `.github/workflows/course-ci.yml`.
- Steps: checkout with submodules, call the existing `.github/actions/setup-dependencies` action, `cmake --preset linux`, `cmake --build`, `ctest`, `cmake --install`, and `actions/upload-artifact`.
- Qt `6.11.2`, Debug, and the `linux` preset, matching the upstream build file.
- A report file, `docs/ci-cd-pipeline-report.md`, for the prompts, decisions, and test log.
- A recommendation to leave the upstream workflows in place.

Decision at this step: do not leave the upstream workflows running. That led to the next prompt.

### Step 3: Ask AI to replace the upstream workflows

Prompt:

> We need to make our own CI/CD workflow, so disable the current flows they have and create our own.

What the assistant generated:

- An updated plan that still adds `course-ci.yml`, and that disables every existing workflow by renaming `*.yml` to `*.yml.disabled`.
- The reason for a rename: GitHub Actions only loads `.yml` and `.yaml` from `.github/workflows`, so a renamed file never triggers, and the original text stays in the repo for this write-up.
- The list of files to disable: `build.yml`, `release.yml`, `publish.yml`, `clang-tidy.yml`, `codeql.yml`, `nix.yml`, `container.yml`, `update-flake.yml`, `backport.yml`, `blocked-prs.yml`, and `merge-blocking-pr.yml`.
- A decision to keep `.github/actions/` unchanged so the new workflow can still call `setup-dependencies`.

Group decisions locked in here:

- One course-owned workflow is the only file that runs.
- Linux Debug only. No Windows or macOS matrix.
- Reuse `setup-dependencies` with `artifact-name: Linux` and Qt `6.11.2`. Do not copy that setup into a new script.
- Permissions are `contents: read` and `packages: write` for the vcpkg cache. No signing token.
- Deploy is an unsigned install artifact named `prismlauncher-linux-debug`.
- Triggers are pull requests, pushes to `develop`, and `workflow_dispatch`. A newer push cancels the run already in progress for that ref.

### Step 4: Ask AI to implement the plan

Prompt:

> Implement the plan as specified.

What the assistant generated, in order:

1. Renamed the eleven workflow files with `git mv`, so Git showed them as renames rather than delete-and-add.
2. Added `.github/workflows/course-ci.yml` with the job described above. Install uses `cmake --install build --config Debug --prefix install`. Upload uses `actions/upload-artifact@v7`, the same major version the repository already used, and fails the job if `install/` is empty.
3. Wrote the first version of this report, with the architecture review, the prompts, the decisions, and a test log that still had to be filled in after a real run.
4. Committed those files as `38215ce42` (`Replace upstream GitHub workflows with a course CI pipeline.`) and pushed `develop` to `https://github.com/Colby-Frison/PrismLauncher.git`.

Unrelated local files were left unstaged: the deleted `docs/test.md` and the untracked assignment sheet `docs/CI-CD pipeline.md`.

### Step 5: Test the pipeline

Testing process:

1. Pushing `38215ce42` to `develop` was the test. The `push` trigger started Course CI automatically. Run: [Course CI #36435815684](https://github.com/Colby-Frison/PrismLauncher/actions/runs/36435815684).
2. `gh` was not installed on the machine used to watch the run. The run was polled with the public GitHub Actions API (`/repos/Colby-Frison/PrismLauncher/actions/runs/36435815684` and its `/jobs` endpoint) instead.
3. Watched steps, in order: Set up job, Checkout, Setup dependencies, Configure project, Run build, Run tests, Install, Upload install artifact.
4. Setup dependencies and configure finished first. The Debug build was still running after those, so the job was polled until it completed.
5. Final job conclusion: success. Every step above completed successfully. The uploaded artifact is `prismlauncher-linux-debug`, 135,474,701 bytes.
6. Listed workflow runs for commit `38215ce42`. Course CI was the only run. None of the renamed `*.yml.disabled` files started.

Problems encountered and how they were handled:

- The GitHub CLI was missing locally, so the run could not be watched with `gh run watch`. The REST API returned the same status, step list, and artifact size, and that was enough to confirm the result. The workflow file was not changed for this.
- The upstream Nix workflow had already succeeded on the previous commit, `ed53999de`, before the disable landed. That run is not part of this pipeline. The commit that contains the rename did not start Nix, Build, CodeQL, or any other upstream workflow.
- Configure, compile, CTest, install, and upload did not fail. There was no pipeline change to make after the run.

After the run, the test log table in this report was filled in with the step results and the run URL, then pushed as `757ccc383`. That second push is a documentation update. It starts another Course CI run because every push to `develop` triggers the workflow. The workflow file itself was not changed between the two commits.

## Summary of steps

1. We reviewed the Module 1 architecture and decided the pipeline must build the Qt/C++ launcher and its Java helper, run the existing headless CTest suite, and upload an install tree because there is no server to deploy to.
2. We asked the assistant to plan a course pipeline from the assignment. It proposed one Ubuntu Debug job that checks out submodules, sets up Qt and vcpkg, configures and builds the `linux` preset, runs CTest, and uploads an unsigned artifact.
3. We asked the assistant to disable the upstream workflows and use only our own. It renamed the eleven existing workflow files to `*.yml.disabled` and kept the shared setup action so the new job could still install dependencies.
4. We asked the assistant to implement that plan. It added `.github/workflows/course-ci.yml`, wrote the first version of this report, and pushed the change as commit `38215ce42`.
5. Pushing that commit started [Course CI #36435815684](https://github.com/Colby-Frison/PrismLauncher/actions/runs/36435815684). Checkout, dependency setup, configure, build, tests, install, and artifact upload all succeeded, and no disabled upstream workflow ran.
6. The only local snag was a missing `gh` command, so the run was watched through the GitHub API instead. The workflow itself did not fail, so it was not changed after the run, and this report was updated with the results.

## M2-S5 evaluation and improvement

### Checklist problem

The first Course CI run already built, tested, installed, and uploaded an artifact. It did not scan the repository, and it did not check the module boundaries in the target architecture diagram. Those boundaries are tasks and networking (`launcher/tasks`, `launcher/net`), the instance and account domain (`launcher/minecraft`, including `launcher/minecraft/auth`), provider adapters (`launcher/modplatform`), and UI (`launcher/ui`). UI may depend on the other layers. The other layers may not include UI. Tasks and networking also may not include the instance domain or provider adapters.

Splitting `Launcher_logic` into four CMake targets was rejected. The architecture notes say that extraction waits until service interfaces exist, and a hard ban on the current UI includes would fail the build immediately.

### What was generated and what was reviewed

The assistant generated `tools/check_architecture_boundaries.py`, the fixture tests in `tests/architecture/test_boundaries.py`, the `architecture-boundaries` job, and the completed checklist in `docs/requirement_Checklist.md`. The group review kept the existing Linux build job, added no marketplace actions, and recorded the 17 includes that already cross a layer in `tools/architecture_boundaries_baseline.txt`. New includes that break the rules fail the job. A baseline line that no longer matches the tree also fails, so a fixed include has to be removed from the baseline in the same change.

The checker tests do not link Qt. They cover an illegal adapter include, a legal UI include, and a stale baseline entry. Those three tests passed locally before the workflow change was pushed.

### Verification run

The run URL for this improvement is filled in after the push that adds the boundary job.
