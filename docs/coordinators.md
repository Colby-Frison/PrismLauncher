# Workflow coordinators (R2)

Ownership after R2.1–R2.4: UI gathers input and presents results; coordinators own sequencing; services own data/side effects.

## Map

| Workflow | UI entry | Coordinator | Services / adapters |
| --- | --- | --- | --- |
| Launch / kill | `MainWindow` (injected), `Application::launch/kill` facade for other callers | `LaunchCoordinator` + `LaunchUiHints` | `ILaunchExtrasStore`, `IRunningInstanceCounter` |
| Create / import / pack update | `MainWindow` (owns coordinator), `ManagedPackPage::runUpdateTask` | `CreateInstanceCoordinator` | `InstanceCatalog`, `LauncherSettings`, `IInstanceTaskRunner` |
| Account default / add / OAuth | `MainWindow` account menu, `AccountListPage`, `LoginWizardPage`, `processURLs` OAuth branch, `MSAStep` | `AccountCoordinator` | `AccountService` |

## Rules

- Prefer constructor injection of coordinator pointers from `Application` (composition root).
- Do not reintroduce launch/import/account sequencing into `Application` or `MainWindow` beyond request assembly and window presentation.
- `processURLs` may keep URL/protocol parsing in the UI; the create/OAuth terminal steps must call coordinators.
- `LaunchUiHints` holds the show-window / kill UX decisions shared by `Application` and `MainWindow` so those files stay thin.
