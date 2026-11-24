// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2026 Prism Launcher Contributors
 *
 *  R3 / Issue 3 — Mod-platform registry unit tests
 *
 *  RUNNABLE when: launcher/modplatform/ModPlatformRegistry.h is present
 *  (issue 3.1 infrastructure on feature/r3.1-modplatform-provider-registry).
 *
 *  Covers requirements: 1 (mod-platform half), 2, 4, 6, 7 (static registration style).
 */

#include <QTest>

#include "modplatform/ModPlatformRegistry.h"

using namespace ModPlatform;

namespace {

ModPlatformProvider makeProvider(const QString& id,
                                 ProviderCapabilityFlags capabilities = ProviderCapability::None,
                                 const QString& displayName = {})
{
    ModPlatformProvider provider;
    provider.id = id;
    provider.displayName = displayName.isEmpty() ? id : displayName;
    provider.iconName = id;
    provider.cacheKey = id + QStringLiteral("Cache");
    provider.capabilities = capabilities;
    return provider;
}

bool containsProviderId(const QList<const ModPlatformProvider*>& providers, const QString& id)
{
    for (const auto* provider : providers) {
        if (provider && provider->id == id) {
            return true;
        }
    }
    return false;
}

}  // namespace

class ProviderRegistryTest : public QObject {
    Q_OBJECT

   private slots:
    void init() { ModPlatformRegistry::instance().resetForTesting(); }
    void cleanup() { ModPlatformRegistry::instance().resetForTesting(); }

    // Req 1: mod-platform providers can be registered and retrieved.
    void registerAndRetrieveModPlatformProvider()
    {
        auto& registry = ModPlatformRegistry::instance();

        auto provider = makeProvider(QStringLiteral("modrinth"),
                                     ProviderCapability::NewInstancePage | ProviderCapability::ResourceMods |
                                         ProviderCapability::HasResourceAPI,
                                     QStringLiteral("Modrinth"));
        provider.cacheKey = QStringLiteral("ModrinthPacks");

        QVERIFY(registry.registerProvider(std::move(provider)));

        const auto* found = registry.find(QStringLiteral("modrinth"));
        QVERIFY(found != nullptr);
        QCOMPARE(found->id, QStringLiteral("modrinth"));
        QCOMPARE(found->displayName, QStringLiteral("Modrinth"));
        QCOMPARE(found->cacheKey, QStringLiteral("ModrinthPacks"));
        QCOMPARE(registry.providers().size(), 1);
    }

    // Req 2: capabilities are reported; filtering returns only compatible providers.
    void capabilitiesAndCapabilityFiltering()
    {
        auto& registry = ModPlatformRegistry::instance();

        auto mods = makeProvider(QStringLiteral("mods-only"), ProviderCapability::ResourceMods);
        auto packs = makeProvider(QStringLiteral("packs-only"),
                                  ProviderCapability::NewInstancePage | ProviderCapability::InstallTask);
        auto both = makeProvider(QStringLiteral("both"),
                                 ProviderCapability::ResourceMods | ProviderCapability::NewInstancePage |
                                     ProviderCapability::HasResourceAPI | ProviderCapability::InstallTask);

        QVERIFY(registry.registerProvider(std::move(mods)));
        QVERIFY(registry.registerProvider(std::move(packs)));
        QVERIFY(registry.registerProvider(std::move(both)));

        const auto* foundBoth = registry.find(QStringLiteral("both"));
        QVERIFY(foundBoth != nullptr);
        QVERIFY(foundBoth->hasCapability(ProviderCapability::ResourceMods));
        QVERIFY(foundBoth->hasCapability(ProviderCapability::HasResourceAPI));
        QVERIFY(foundBoth->hasCapability(ProviderCapability::InstallTask));
        QVERIFY(!foundBoth->hasCapability(ProviderCapability::RequiresApiKey));

        const auto modProviders = registry.providersWith(ProviderCapability::ResourceMods);
        QCOMPARE(modProviders.size(), 2);
        QVERIFY(containsProviderId(modProviders, QStringLiteral("mods-only")));
        QVERIFY(containsProviderId(modProviders, QStringLiteral("both")));
        QVERIFY(!containsProviderId(modProviders, QStringLiteral("packs-only")));

        const auto needApi = registry.providersWith(ProviderCapability::ResourceMods |
                                                    ProviderCapability::HasResourceAPI);
        QCOMPARE(needApi.size(), 1);
        QCOMPARE(needApi.first()->id, QStringLiteral("both"));
    }

    // Req 4: multiple providers stay independent; lookup logic is id-based, not hard-coded.
    void multipleProvidersIndependentLookup()
    {
        auto& registry = ModPlatformRegistry::instance();

        QVERIFY(registry.registerProvider(
            makeProvider(QStringLiteral("modrinth"), ProviderCapability::ResourceMods, QStringLiteral("Modrinth"))));
        QVERIFY(registry.registerProvider(makeProvider(
            QStringLiteral("flame"), ProviderCapability::ResourceMods | ProviderCapability::RequiresApiKey,
            QStringLiteral("CurseForge"))));
        QVERIFY(registry.registerProvider(
            makeProvider(QStringLiteral("custom-new"), ProviderCapability::NewInstancePage, QStringLiteral("Custom"))));

        QCOMPARE(registry.providers().size(), 3);
        QCOMPARE(registry.find(QStringLiteral("modrinth"))->displayName, QStringLiteral("Modrinth"));
        QCOMPARE(registry.find(QStringLiteral("flame"))->displayName, QStringLiteral("CurseForge"));
        QCOMPARE(registry.find(QStringLiteral("custom-new"))->displayName, QStringLiteral("Custom"));

        // Adding "custom-new" did not require special-case lookup — find() is generic.
        QVERIFY(registry.find(QStringLiteral("custom-new"))->hasCapability(ProviderCapability::NewInstancePage));
        QVERIFY(!registry.find(QStringLiteral("modrinth"))->hasCapability(ProviderCapability::RequiresApiKey));
    }

    // Req 6: unknown / invalid lookups are safe.
    void unknownAndInvalidLookupsAreSafe()
    {
        auto& registry = ModPlatformRegistry::instance();

        QCOMPARE(registry.find(QStringLiteral("missing")), nullptr);
        QCOMPARE(registry.find(QString()), nullptr);
        QCOMPARE(registry.providersWith(ProviderCapability::ResourceMods).size(), 0);

        QVERIFY(!registry.registerProvider(ModPlatformProvider{}));  // empty id
        QCOMPARE(registry.providers().size(), 0);

        auto provider = makeProvider(QStringLiteral("dup"), ProviderCapability::ResourceMods);
        QVERIFY(registry.registerProvider(provider));
        QVERIFY(!registry.registerProvider(provider));  // duplicate
        QCOMPARE(registry.find(QStringLiteral("DUP")), nullptr);  // case-sensitive miss
    }

    // Req 2 + 6 + 7: missing optional capabilities / factories are not exposed.
    void missingOptionalCapabilitiesNotExposed()
    {
        auto& registry = ModPlatformRegistry::instance();

        QVERIFY(registry.registerProvider(makeProvider(QStringLiteral("minimal"), ProviderCapability::ResourceMods)));

        const auto* found = registry.find(QStringLiteral("minimal"));
        QVERIFY(found != nullptr);
        QVERIFY(!found->hasCapability(ProviderCapability::HasResourceAPI));
        QVERIFY(!found->hasCapability(ProviderCapability::InstallTask));
        QVERIFY(!found->hasCapability(ProviderCapability::NewInstancePage));
        QVERIFY(!static_cast<bool>(found->createResourceAPI));
        QVERIFY(!static_cast<bool>(found->createInstallTask));
        QVERIFY(!static_cast<bool>(found->createNewInstancePage));
        QVERIFY(!static_cast<bool>(found->resourcePageFactory(ResourceType::Mod)));

        QCOMPARE(registry.providersWith(ProviderCapability::HasResourceAPI).size(), 0);
        QCOMPARE(registry.providersWith(ProviderCapability::InstallTask).size(), 0);
    }

    // Req 2 edge: availability gate excludes providers from capability queries.
    void capabilityFilterHonorsAvailability()
    {
        auto& registry = ModPlatformRegistry::instance();

        auto ok = makeProvider(QStringLiteral("available"), ProviderCapability::ResourceMods);
        auto blocked = makeProvider(QStringLiteral("blocked"),
                                    ProviderCapability::ResourceMods | ProviderCapability::RequiresApiKey);
        blocked.isAvailable = [] { return false; };

        QVERIFY(registry.registerProvider(std::move(ok)));
        QVERIFY(registry.registerProvider(std::move(blocked)));

        const auto filtered = registry.providersWith(ProviderCapability::ResourceMods);
        QCOMPARE(filtered.size(), 1);
        QCOMPARE(filtered.first()->id, QStringLiteral("available"));
        QVERIFY(registry.find(QStringLiteral("blocked")) != nullptr);  // still registered
    }

    // Req 7: registration API is the static-registration entry point (no plugin loader).
    void registrationUsesStaticRegisterApi()
    {
        // Production providers use REGISTER_MOD_PLATFORM → registerProvider at static init.
        // This test validates that same API without exercising dynamic plugin loading
        // (R3 intentionally has none).
        auto& registry = ModPlatformRegistry::instance();
        QVERIFY(registry.registerProvider(makeProvider(QStringLiteral("static-style"), ProviderCapability::ResourceMods)));
        QVERIFY(registry.find(QStringLiteral("static-style")) != nullptr);
    }

    void resourceTypeCapabilityMapping()
    {
        QCOMPARE(capabilityForResourceType(ResourceType::Mod), ProviderCapability::ResourceMods);
        QCOMPARE(capabilityForResourceType(ResourceType::ResourcePack), ProviderCapability::ResourcePacks);
        QCOMPARE(capabilityForResourceType(ResourceType::TexturePack), ProviderCapability::TexturePacks);
        QCOMPARE(capabilityForResourceType(ResourceType::ShaderPack), ProviderCapability::ShaderPacks);
        QCOMPARE(capabilityForResourceType(ResourceType::DataPack), ProviderCapability::DataPacks);
        QCOMPARE(capabilityForResourceType(ResourceType::Modpack), ProviderCapability::NewInstancePage);
        QCOMPARE(capabilityForResourceType(ResourceType::Unknown), ProviderCapability::None);
    }
};

QTEST_GUILESS_MAIN(ProviderRegistryTest)

#include "ProviderRegistry_test.moc"
