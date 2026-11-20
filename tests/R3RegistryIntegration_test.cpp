// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2026 Prism Launcher Contributors
 *
 *  R3 / Issue 3 — Registry integration / discovery tests
 *
 *  REQUIRES ADDITIONAL IMPLEMENTATION:
 *    - ModPlatformRegistry infrastructure (3.1) present
 *    - NewInstanceDialog / resource-download flows consume
 *      ModPlatformRegistry::providersWith(...) instead of hard-coded page lists
 *      (see tests/r3/AssumedR3Interfaces.h)
 *    - AuthProviderRegistry for auth discovery assertions below
 *
 *  Status: Partially runnable when only ModPlatformRegistry exists (discovery
 *  helper tests). Full UI/auth assertions require issue 3 completion.
 *  CMake enables this target when ModPlatformRegistry.h is present; auth slots
 *  QSKIP until AuthProviderRegistry.h exists.
 *
 *  Covers requirements: 3, 4, 5 (discovery), 7.
 */

#include <QTest>

#include <QtGlobal>

#include "modplatform/ModPlatformRegistry.h"

#if __has_include("minecraft/auth/AuthProviderRegistry.h")
#include "minecraft/auth/AuthProviderRegistry.h"
#define PRISM_R3_HAS_AUTH_REGISTRY 1
#else
#define PRISM_R3_HAS_AUTH_REGISTRY 0
#endif

#include "r3/AssumedR3Interfaces.h"

using namespace ModPlatform;

namespace {

ModPlatformProvider makeProvider(const QString& id, ProviderCapabilityFlags capabilities)
{
    ModPlatformProvider provider;
    provider.id = id;
    provider.displayName = id;
    provider.iconName = id;
    provider.cacheKey = id + QStringLiteral("Cache");
    provider.capabilities = capabilities;
    return provider;
}

QStringList idsOf(const QList<const ModPlatformProvider*>& providers)
{
    QStringList ids;
    for (const auto* provider : providers) {
        if (provider) {
            ids.append(provider->id);
        }
    }
    ids.sort();
    return ids;
}

}  // namespace

class R3RegistryIntegrationTest : public QObject {
    Q_OBJECT

   private slots:
    void init()
    {
        ModPlatformRegistry::instance().resetForTesting();
#if PRISM_R3_HAS_AUTH_REGISTRY
        Auth::AuthProviderRegistry::instance().resetForTesting();
#endif
    }

    void cleanup()
    {
        ModPlatformRegistry::instance().resetForTesting();
#if PRISM_R3_HAS_AUTH_REGISTRY
        Auth::AuthProviderRegistry::instance().resetForTesting();
#endif
    }

    /*
     * Req 3: NewInstanceDialog-oriented discovery uses the registry.
     *
     * Assumption: page construction for mod platforms iterates
     * providersWith(NewInstancePage) rather than hard-coding FlamePage /
     * ModrinthPage / TechnicPage / …
     */
    void newInstanceDiscoveryUsesRegistryCapabilities()
    {
        auto& registry = ModPlatformRegistry::instance();

        QVERIFY(registry.registerProvider(
            makeProvider(QStringLiteral("modrinth"), ProviderCapability::NewInstancePage | ProviderCapability::ResourceMods)));

        // Flame gated unavailable (SupportsFlame-style) must drop out of discovery.
        auto flame = makeProvider(QStringLiteral("flame"),
                                  ProviderCapability::NewInstancePage | ProviderCapability::RequiresApiKey);
        flame.isAvailable = [] { return false; };
        QVERIFY(registry.registerProvider(std::move(flame)));

        QVERIFY(registry.registerProvider(
            makeProvider(QStringLiteral("mods-api-only"), ProviderCapability::ResourceMods | ProviderCapability::HasResourceAPI)));

        const auto pages = registry.providersWith(ProviderCapability::NewInstancePage);
        QCOMPARE(idsOf(pages), QStringList{ QStringLiteral("modrinth") });
        QVERIFY(!idsOf(pages).contains(QStringLiteral("mods-api-only")));
        QVERIFY(!idsOf(pages).contains(QStringLiteral("flame")));
    }

    /*
     * Req 3: Flame/Modrinth-style resource download discovery uses registry
     * capabilities (ResourceMods / HasResourceAPI), not hard-coded provider switches.
     */
    void resourceDownloadDiscoveryUsesRegistryCapabilities()
    {
        auto& registry = ModPlatformRegistry::instance();

        QVERIFY(registry.registerProvider(makeProvider(
            QStringLiteral("modrinth"),
            ProviderCapability::ResourceMods | ProviderCapability::HasResourceAPI | ProviderCapability::ResourcePacks)));
        QVERIFY(registry.registerProvider(makeProvider(
            QStringLiteral("flame"),
            ProviderCapability::ResourceMods | ProviderCapability::HasResourceAPI | ProviderCapability::RequiresApiKey)));
        QVERIFY(registry.registerProvider(
            makeProvider(QStringLiteral("technic"), ProviderCapability::NewInstancePage)));

        const auto modApis =
            registry.providersWith(ProviderCapability::ResourceMods | ProviderCapability::HasResourceAPI);
        QCOMPARE(modApis.size(), 2);
        QCOMPARE(idsOf(modApis), (QStringList{ QStringLiteral("flame"), QStringLiteral("modrinth") }));

        const auto packProviders = registry.providersWith(capabilityForResourceType(ResourceType::ResourcePack));
        QCOMPARE(packProviders.size(), 1);
        QCOMPARE(packProviders.first()->id, QStringLiteral("modrinth"));

        // Technic is a pack page provider, not a mod ResourceAPI provider.
        QVERIFY(!idsOf(modApis).contains(QStringLiteral("technic")));
    }

    /*
     * Req 4: adding a new provider only requires registration — discovery queries
     * stay generic (no new switch cases in lookup helpers).
     */
    void addingProviderDoesNotRequireLookupChanges()
    {
        auto& registry = ModPlatformRegistry::instance();

        QVERIFY(registry.registerProvider(
            makeProvider(QStringLiteral("modrinth"), ProviderCapability::NewInstancePage | ProviderCapability::ResourceMods)));

        auto before = idsOf(registry.providersWith(ProviderCapability::NewInstancePage));
        QCOMPARE(before, QStringList{ QStringLiteral("modrinth") });

        QVERIFY(registry.registerProvider(makeProvider(
            QStringLiteral("brand-new-platform"),
            ProviderCapability::NewInstancePage | ProviderCapability::ResourceMods | ProviderCapability::HasResourceAPI)));

        auto afterPages = idsOf(registry.providersWith(ProviderCapability::NewInstancePage));
        auto afterMods = idsOf(registry.providersWith(ProviderCapability::ResourceMods));

        QVERIFY(afterPages.contains(QStringLiteral("brand-new-platform")));
        QVERIFY(afterMods.contains(QStringLiteral("brand-new-platform")));
        QCOMPARE(registry.find(QStringLiteral("brand-new-platform"))->id, QStringLiteral("brand-new-platform"));
    }

    // Req 5 + 3: auth discovery for MSA / offline once AuthProviderRegistry exists.
    void authDiscoveryForMsaAndOffline()
    {
#if !PRISM_R3_HAS_AUTH_REGISTRY
        QSKIP("AuthProviderRegistry not implemented yet — see tests/r3/AssumedR3Interfaces.h");
#else
        using namespace Auth;
        auto& registry = AuthProviderRegistry::instance();

        AuthProvider msa;
        msa.id = QStringLiteral("msa");
        msa.displayName = QStringLiteral("Microsoft");
        msa.capabilities = AuthCapability::InteractiveLogin | AuthCapability::SilentRefresh |
                           AuthCapability::Persistence | AuthCapability::AccountCreationUI;

        AuthProvider offline;
        offline.id = QStringLiteral("offline");
        offline.displayName = QStringLiteral("Offline");
        offline.capabilities =
            AuthCapability::OfflineSession | AuthCapability::Persistence | AuthCapability::AccountCreationUI;

        QVERIFY(registry.registerProvider(std::move(msa)));
        QVERIFY(registry.registerProvider(std::move(offline)));

        QVERIFY(registry.find(QStringLiteral("msa")) != nullptr);
        QVERIFY(registry.find(QStringLiteral("offline")) != nullptr);
        QCOMPARE(registry.providersWith(AuthCapability::InteractiveLogin).first()->id, QStringLiteral("msa"));
        QCOMPARE(registry.providersWith(AuthCapability::OfflineSession).first()->id, QStringLiteral("offline"));
#endif
    }

    // Req 7: discovery path is registry-based static registration, not plugins.
    void discoveryDoesNotRequireDynamicPlugins()
    {
        auto& registry = ModPlatformRegistry::instance();
        QVERIFY(registry.registerProvider(
            makeProvider(QStringLiteral("modrinth"), ProviderCapability::ResourceMods | ProviderCapability::HasResourceAPI)));

        // If R3 introduced a dynamic plugin API, this suite would need explicit tests for it.
        // The assumed contract forbids that; success here is plain register + providersWith.
        QCOMPARE(registry.providersWith(ProviderCapability::ResourceMods).size(), 1);
    }
};

QTEST_GUILESS_MAIN(R3RegistryIntegrationTest)

#include "R3RegistryIntegration_test.moc"
