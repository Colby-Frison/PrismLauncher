# Architecture diagrams

These diagrams summarize the current architecture and the recommended target architecture. They intentionally omit individual leaf classes so the principal ownership, dependency, and extension boundaries remain visible.

## Before: current architecture

```mermaid
flowchart TB
    Entry["launcher/main.cpp"] --> App["Application<br/>QApplication + composition root + service locator"]

    subgraph Services["Application-owned services"]
        Settings["SettingsObject"]
        Network["QNetworkAccessManager"]
        Cache["HttpMetaCache"]
        Metadata["Meta::Index / VersionList"]
        Instances["InstanceList"]
        Accounts["AccountList"]
        Java["JavaInstallList"]
        UIAssets["ThemeManager / IconList"]
        Updater["ExternalUpdater"]
    end

    App --> Services

    subgraph UI["UI layer"]
        MainWindow
        InstanceWindow
        Dialogs["dialogs / widgets"]
        Pages["global / instance / mod-platform pages"]
    end

    App --> UI
    MainWindow --> Instances
    MainWindow --> Accounts

    subgraph Domain["Instance and Minecraft domain"]
        BaseInstance
        MinecraftInstance
        PackProfile
        Components["Component[]"]
        LaunchProfile
        Resources["mods / packs / worlds"]
        UpdateTasks["folder / library / asset tasks"]
    end

    Instances --> MinecraftInstance
    MinecraftInstance -->|inherits| BaseInstance
    MinecraftInstance --> PackProfile
    PackProfile --> Components
    Components --> LaunchProfile
    MinecraftInstance --> Resources
    MinecraftInstance --> UpdateTasks

    subgraph Launch["Launch subsystem"]
        LaunchController
        LaunchTask
        Steps["ordered LaunchStep chain"]
        JavaStep["LauncherPartLaunch"]
    end

    App --> LaunchController
    LaunchController --> LaunchTask
    MinecraftInstance -->|owns| LaunchTask
    LaunchTask --> Steps
    Steps --> JavaStep

    JavaStep --> JavaProcess["Java process<br/>EntryPoint + Minecraft JVM"]

    subgraph Providers["Hard-coded provider composition"]
        NewDialog["NewInstanceDialog"]
        ProviderPages["Flame / Modrinth / FTB / Technic / ATLauncher pages"]
        ResourceAPIs["ResourceAPI implementations"]
        AuthFlow["AuthFlow + AccountType switches"]
    end

    NewDialog --> ProviderPages
    ProviderPages --> ResourceAPIs
    Accounts --> AuthFlow

    subgraph Jobs["Per-job concurrency"]
        JobA["NetJob A<br/>limit N"]
        JobB["NetJob B<br/>limit N"]
        JobC["NetJob C<br/>limit N"]
    end

    ResourceAPIs --> Jobs
    Jobs --> Network

    App -. "APPLICATION macro<br/>hidden dependencies" .-> MainWindow
    App -. "APPLICATION macro" .-> MinecraftInstance
    App -. "APPLICATION macro" .-> ResourceAPIs
    App -. "APPLICATION macro" .-> UpdateTasks
```

### Current pressure points

- `Application` combines composition, service access, startup, window coordination, and launch coordination.
- UI and domain code obtain dependencies through the global `APPLICATION` macro.
- Providers and account types are constructed through central lists and switches.
- Instance discovery performs full scans and important lookups are linear.
- Each `NetJob` applies its own concurrency limit, so aggregate work is not globally bounded.
- Launch and authentication pipelines are assembled directly in concrete classes.

## After: recommended target architecture

```mermaid
flowchart TB
    Entry["launcher/main.cpp"] --> Root["Application composition root"]

    subgraph Contracts["Injected service contracts"]
        SettingsAPI["LauncherSettings"]
        NetworkAPI["NetworkContext"]
        MetadataAPI["MetadataRepository"]
        InstanceAPI["InstanceRepository"]
        AccountAPI["AccountRepository"]
    end

    Root --> Contracts

    subgraph Infrastructure["Infrastructure implementations"]
        SettingsRepo["Versioned settings repository"]
        Persistence["Atomic persistence + migrations"]
        Scheduler["Global work scheduler<br/>aggregate + per-host limits"]
        MetadataRepo["Metadata/cache repository"]
        Catalog["Incremental indexed instance catalog"]
    end

    SettingsAPI --> SettingsRepo
    NetworkAPI --> Scheduler
    MetadataAPI --> MetadataRepo
    InstanceAPI --> Catalog
    AccountAPI --> Persistence

    subgraph Workflows["Headless application workflows"]
        Startup["StartupCoordinator"]
        Import["ImportCoordinator"]
        InstanceActions["InstanceActionController"]
        LaunchService["InstanceLaunchService"]
        Update["UpdateCoordinator"]
    end

    Root --> Workflows
    Workflows --> Contracts

    subgraph Presentation["Presentation layer"]
        MainView["MainWindow"]
        InstanceView["InstanceWindow"]
        PageHost["PageContainer / dialogs / widgets"]
    end

    Root --> Presentation
    Presentation -->|commands and view state| Workflows

    subgraph Registries["Capability-based registries"]
        PlatformRegistry["ModPlatformRegistry"]
        AuthRegistry["AuthProviderRegistry"]
        InstanceRegistry["InstanceTypeRegistry"]
        PageRegistry["Contextual page contributions"]
    end

    Root --> Registries

    subgraph Pipelines["Named pipeline builders"]
        LaunchBuilder["Launch pipeline<br/>pre-metadata → update → game → post-exit"]
        AuthBuilder["Authentication pipeline"]
        UpdateBuilder["Update pipeline"]
    end

    Registries --> Pipelines
    LaunchService --> LaunchBuilder
    Update --> UpdateBuilder
    AccountAPI --> AuthBuilder

    subgraph Domain["Domain contracts and models"]
        BaseInstance["BaseInstance"]
        PackProfile["PackProfile / Component / LaunchProfile"]
        Resources["Resource and folder models"]
        TypedResults["ProviderResult&lt;T&gt;"]
    end

    Catalog --> BaseInstance
    BaseInstance --> PackProfile
    BaseInstance --> Resources
    PlatformRegistry --> TypedResults
    TypedResults --> Scheduler

    LaunchBuilder --> JavaBoundary["LauncherPartLaunch"]
    JavaBoundary --> JavaProcess["Java process<br/>EntryPoint + Minecraft JVM"]

    subgraph Modules["Enforced build boundaries"]
        TaskNet["tasks + networking"]
        InstanceAccount["instance + account domains"]
        Adapters["provider adapters"]
        UIModel["UI"]
    end

    Infrastructure --> TaskNet
    Domain --> InstanceAccount
    Registries --> Adapters
    Presentation --> UIModel
```

### Target properties

- `Application` remains the composition root but is no longer the dependency API used by domain code.
- Workflows are testable without creating application windows.
- Providers, authentication methods, instance types, and pages register through capability-based descriptors.
- Network work is globally scheduled with provider-aware limits and retry policies.
- The instance catalog uses indexed lookup and incremental filesystem reconciliation.
- Launch, update, and authentication behavior is composed through deterministic named phases.
- Persistence has explicit schemas, atomic writes, and versioned migrations.
- Build targets enforce the intended architectural boundaries.

## Migration direction

```mermaid
flowchart LR
    P1["Phase 1<br/>Tests, service interfaces, indexes"]
    P2["Phase 2<br/>Scheduler, registries, coordinators, pipelines"]
    P3["Phase 3<br/>Incremental catalog, repositories, module extraction"]

    P1 --> P2 --> P3
```
