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

The workflow is set up to run on GitHub-hosted Ubuntu. The completion check is a successful configure, a successful Debug build, a passing `ctest --preset linux` run, and an uploaded `prismlauncher-linux-debug` artifact.

Results from the verification run are recorded below after the workflow finishes.

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

The workflow structure above is what the assistant generated from the architecture docs and the existing CMake presets. The correction from the group was to disable the upstream workflows and keep a single course-owned file as the pipeline that runs. Further corrections from the verification run are listed in the test log.
