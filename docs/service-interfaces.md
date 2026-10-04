# Service interfaces: ownership and injection (R1.1)

This note covers the narrow service contracts introduced for R1 and the rules for owning and injecting them. Call-site migration is intentionally deferred to later R1 sub-issues.

## Contracts

| Interface | Purpose | Current wrapper |
| --- | --- | --- |
| `LauncherSettings` | Global and playtime settings access | `SettingsObjectLauncherSettings` |
| `NetworkContext` | Shared `QNetworkAccessManager`, `HttpMetaCache`, user-agent | `SharedNetworkContext` |
| `InstanceCatalog` | Instance lookup and lifecycle operations | `InstanceListCatalog` |
| `AccountService` | Account list, default account, refresh entry points | `AccountListService` |

Headers live under `launcher/services/`.

## Ownership

- `Application` remains the composition root. It continues to own the concrete `SettingsObject`, `QNetworkAccessManager`, `HttpMetaCache`, `InstanceList`, and `AccountList` instances.
- Wrappers hold non-owning pointers to those objects. They must not outlive the backing objects.
- Wrappers do not introduce a second service locator. Do not replace `APPLICATION` with another global that returns these interfaces.

Recommended lifetime (R1.2):

1. Application constructs and owns the backing objects (unchanged today).
2. Application constructs the four wrappers and stores them as `std::unique_ptr` members.
3. Application exposes `LauncherSettings*`, `NetworkContext*`, `InstanceCatalog*`, and `AccountService*` accessors for injection.
4. Existing `settings()`, `network()`, `instances()`, and `accounts()` accessors stay until call sites migrate.

## Injection rules

- New and migrated code takes the narrow interface it needs via constructor parameter (or setter only when Qt parent ownership forces a two-phase init).
- Prefer the smallest interface: settings-only code gets `LauncherSettings*`, not `Application*`.
- Do not pass `Application*` into domain, net, or provider types when one of these contracts suffices.
- Transitional methods such as `LauncherSettings::settingsObject()`, `InstanceCatalog::model()`, and `AccountService::model()` exist so Qt models and `SettingsObject` signal wiring can migrate incrementally. New non-UI logic should avoid them.
- Tests may construct wrappers around fakes or temporary `INISettingsObject` / list instances without constructing `QApplication` as a service locator.

## Out of scope for R1.1

- Rewriting `APPLICATION->…` call sites
- Extracting `AuthFlow` construction into `AccountService`
- Moving API keys / capability flags onto `NetworkContext`
- Splitting CMake targets by layer (R7)
