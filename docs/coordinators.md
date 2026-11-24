# Workflow coordinators (R2)

Ownership after R2.1–R2.4: UI gathers input and presents results; coordinators own sequencing; services own data/side effects.

## Map

| Workflow | UI entry | Coordinator | Services / adapters |
| --- | --- | --- | --- |
| Launch / kill | `MainWindow` (injected), `Application::launch/kill` facade for other callers | `LaunchCoordinator` + `LaunchUiHints` | `ILaunchExtrasStore`, `IRunningInstanceCounter` |
| Create / import / pack update | `MainWindow` (owns coordinator), `ManagedPackPage::runUpdateTask` (injected catalog/settings) | `CreateInstanceCoordinator` | `InstanceCatalog`, `LauncherSettings`, `IInstanceTaskRunner` |
| Account default / add / OAuth / reauth replace | `MainWindow` (injected), `AccountListPage` / `LoginWizardPage` (injected), `LaunchController` via `LaunchCoordinator`, `processURLs` OAuth branch, `MSAStep` | `AccountCoordinator` | `AccountService` |

## Rules

- Prefer constructor injection of coordinator pointers from `Application` (composition root).
- Do not reintroduce launch/import/account sequencing into `Application` or `MainWindow` beyond request assembly and window presentation.
- `processURLs` may keep URL/protocol parsing in the UI; the create/OAuth terminal steps must call coordinators (via injected members, not `APPLICATION`).
- `LaunchUiHints::handleLaunchOutcome` is the single facade-layer home for launch outcome logging / window hints / accept result.
- `LaunchUiHints` also holds kill UX decisions shared by `Application` and `MainWindow` so those files stay thin.
- `MSAStep` still reaches `AccountCoordinator` via `APPLICATION` until AuthFlow/MinecraftAccount gain injection (transitional).
