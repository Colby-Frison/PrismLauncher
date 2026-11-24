// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2026 Prism Launcher Contributors
 *
 *  R3 / Issue 3 — Authentication registry unit tests
 *
 *  REQUIRES ADDITIONAL IMPLEMENTATION:
 *    launcher/minecraft/auth/AuthProviderRegistry.h (+ .cpp) matching
 *    tests/r3/AssumedR3Interfaces.h
 *
 *  Status: NOT runnable until that production API exists.
 *  CMake enables this target only when AuthProviderRegistry.h is present.
 *
 *  Covers requirements: 1 (auth half), 2 (auth capabilities), 5, 6, 7.
 */

#include <QTest>

#include "minecraft/auth/AuthProviderRegistry.h"
#include "r3/AssumedR3Interfaces.h"  // documentation only; empty of declarations

using namespace Auth;

namespace {

AuthProvider makeAuthProvider(const QString& id,
                              AuthCapabilityFlags capabilities,
                              const QString& displayName = {})
{
    AuthProvider provider;
    provider.id = id;
    provider.displayName = displayName.isEmpty() ? id : displayName;
    provider.capabilities = capabilities;
    return provider;
}

bool containsAuthId(const QList<const AuthProvider*>& providers, const QString& id)
{
    for (const auto* provider : providers) {
        if (provider && provider->id == id) {
            return true;
        }
    }
    return false;
}

}  // namespace

class AuthProviderRegistryTest : public QObject {
    Q_OBJECT

   private slots:
    void init() { AuthProviderRegistry::instance().resetForTesting(); }
    void cleanup() { AuthProviderRegistry::instance().resetForTesting(); }

    // Req 1: authentication providers can be registered and retrieved.
    void registerAndRetrieveAuthProvider()
    {
        auto& registry = AuthProviderRegistry::instance();

        auto msa = makeAuthProvider(QStringLiteral("msa"),
                                    AuthCapability::InteractiveLogin | AuthCapability::SilentRefresh |
                                        AuthCapability::Persistence | AuthCapability::AccountCreationUI,
                                    QStringLiteral("Microsoft"));

        QVERIFY(registry.registerProvider(std::move(msa)));

        const auto* found = registry.find(QStringLiteral("msa"));
        QVERIFY(found != nullptr);
        QCOMPARE(found->displayName, QStringLiteral("Microsoft"));
        QCOMPARE(registry.providers().size(), 1);
    }

    // Req 5: Microsoft (MSA) and offline providers are registered and discoverable.
    void microsoftAndOfflineAreDiscoverable()
    {
        auto& registry = AuthProviderRegistry::instance();

        QVERIFY(registry.registerProvider(makeAuthProvider(
            QStringLiteral("msa"),
            AuthCapability::InteractiveLogin | AuthCapability::SilentRefresh | AuthCapability::Persistence |
                AuthCapability::AccountCreationUI,
            QStringLiteral("Microsoft"))));

        QVERIFY(registry.registerProvider(makeAuthProvider(
            QStringLiteral("offline"),
            AuthCapability::OfflineSession | AuthCapability::Persistence | AuthCapability::AccountCreationUI,
            QStringLiteral("Offline"))));

        const auto* msa = registry.find(QStringLiteral("msa"));
        const auto* offline = registry.find(QStringLiteral("offline"));
        QVERIFY(msa != nullptr);
        QVERIFY(offline != nullptr);

        QVERIFY(msa->hasCapability(AuthCapability::InteractiveLogin));
        QVERIFY(msa->hasCapability(AuthCapability::SilentRefresh));
        QVERIFY(!msa->hasCapability(AuthCapability::OfflineSession));

        QVERIFY(offline->hasCapability(AuthCapability::OfflineSession));
        QVERIFY(!offline->hasCapability(AuthCapability::InteractiveLogin));
        QVERIFY(!offline->hasCapability(AuthCapability::SilentRefresh));

        const auto interactive = registry.providersWith(AuthCapability::InteractiveLogin);
        QCOMPARE(interactive.size(), 1);
        QCOMPARE(interactive.first()->id, QStringLiteral("msa"));

        const auto offlineCapable = registry.providersWith(AuthCapability::OfflineSession);
        QCOMPARE(offlineCapable.size(), 1);
        QCOMPARE(offlineCapable.first()->id, QStringLiteral("offline"));

        const auto persistable = registry.providersWith(AuthCapability::Persistence);
        QCOMPARE(persistable.size(), 2);
        QVERIFY(containsAuthId(persistable, QStringLiteral("msa")));
        QVERIFY(containsAuthId(persistable, QStringLiteral("offline")));
    }

    // Req 2: capability filtering for auth providers.
    void authCapabilityFiltering()
    {
        auto& registry = AuthProviderRegistry::instance();

        QVERIFY(registry.registerProvider(
            makeAuthProvider(QStringLiteral("msa"), AuthCapability::InteractiveLogin | AuthCapability::SilentRefresh)));
        QVERIFY(registry.registerProvider(
            makeAuthProvider(QStringLiteral("offline"), AuthCapability::OfflineSession)));

        QCOMPARE(registry.providersWith(AuthCapability::SilentRefresh).size(), 1);
        QCOMPARE(registry.providersWith(AuthCapability::OfflineSession).size(), 1);
        QCOMPARE(registry.providersWith(AuthCapability::InteractiveLogin | AuthCapability::SilentRefresh).size(), 1);
        QCOMPARE(registry.providersWith(AuthCapability::AccountCreationUI).size(), 0);
    }

    // Req 1 + 4: multiple auth providers independent.
    void multipleAuthProvidersIndependent()
    {
        auto& registry = AuthProviderRegistry::instance();

        QVERIFY(registry.registerProvider(
            makeAuthProvider(QStringLiteral("msa"), AuthCapability::InteractiveLogin, QStringLiteral("Microsoft"))));
        QVERIFY(registry.registerProvider(
            makeAuthProvider(QStringLiteral("offline"), AuthCapability::OfflineSession, QStringLiteral("Offline"))));
        QVERIFY(registry.registerProvider(
            makeAuthProvider(QStringLiteral("future-auth"), AuthCapability::InteractiveLogin, QStringLiteral("Future"))));

        QCOMPARE(registry.providers().size(), 3);
        QCOMPARE(registry.find(QStringLiteral("future-auth"))->displayName, QStringLiteral("Future"));

        const auto interactive = registry.providersWith(AuthCapability::InteractiveLogin);
        QCOMPARE(interactive.size(), 2);
        QVERIFY(containsAuthId(interactive, QStringLiteral("msa")));
        QVERIFY(containsAuthId(interactive, QStringLiteral("future-auth")));
    }

    // Req 6: unknown / invalid auth lookups.
    void unknownAuthLookupsAreSafe()
    {
        auto& registry = AuthProviderRegistry::instance();

        QCOMPARE(registry.find(QStringLiteral("yggdrasil")), nullptr);
        QCOMPARE(registry.find(QString()), nullptr);
        QVERIFY(!registry.registerProvider(AuthProvider{}));

        auto provider = makeAuthProvider(QStringLiteral("msa"), AuthCapability::InteractiveLogin);
        QVERIFY(registry.registerProvider(provider));
        QVERIFY(!registry.registerProvider(provider));
        QCOMPARE(registry.find(QStringLiteral("MSA")), nullptr);
    }

    // Req 2 + 6: unsupported capabilities / unavailable providers.
    void unsupportedCapabilitiesAndAvailability()
    {
        auto& registry = AuthProviderRegistry::instance();

        auto offline = makeAuthProvider(QStringLiteral("offline"), AuthCapability::OfflineSession);
        auto gated = makeAuthProvider(QStringLiteral("msa"), AuthCapability::InteractiveLogin);
        gated.isAvailable = [] { return false; };

        QVERIFY(registry.registerProvider(std::move(offline)));
        QVERIFY(registry.registerProvider(std::move(gated)));

        QVERIFY(!registry.find(QStringLiteral("offline"))->hasCapability(AuthCapability::InteractiveLogin));
        QCOMPARE(registry.providersWith(AuthCapability::InteractiveLogin).size(), 0);
        QVERIFY(registry.find(QStringLiteral("msa")) != nullptr);
    }

    // Req 7: static registration entry point (REGISTER_AUTH_PROVIDER → registerProvider).
    void authRegistrationIsStaticStyle()
    {
        auto& registry = AuthProviderRegistry::instance();
        QVERIFY(registry.registerProvider(
            makeAuthProvider(QStringLiteral("msa"), AuthCapability::InteractiveLogin | AuthCapability::Persistence)));
        QVERIFY(registry.find(QStringLiteral("msa")) != nullptr);
    }
};

QTEST_GUILESS_MAIN(AuthProviderRegistryTest)

#include "AuthProviderRegistry_test.moc"
