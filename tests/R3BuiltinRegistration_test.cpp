// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2026 Prism Launcher Contributors
 *
 *  Verifies built-in provider registration is reachable through ensure*()
 *  (static-library link anchor) and restores providers after resetForTesting().
 */

#include <QTest>

#include "minecraft/auth/AuthProviderRegistry.h"
#include "minecraft/auth/AuthProviders.h"
#include "modplatform/ModPlatformProviders.h"
#include "modplatform/ModPlatformRegistry.h"

class R3BuiltinRegistrationTest : public QObject {
    Q_OBJECT

   private slots:
    void builtinModPlatformProvidersAreRegisteredViaEnsure()
    {
        using namespace ModPlatform;

        auto& registry = ModPlatformRegistry::instance();
        registry.resetForTesting();
        QCOMPARE(registry.providers().size(), 0);

        ensureBuiltInProvidersRegistered();

        QVERIFY(registry.find(QStringLiteral("atl")) != nullptr);
        QVERIFY(registry.find(QStringLiteral("flame")) != nullptr);
        QVERIFY(registry.find(QStringLiteral("ftb")) != nullptr);
        QVERIFY(registry.find(QStringLiteral("legacy_ftb")) != nullptr);
        QVERIFY(registry.find(QStringLiteral("import_ftb")) != nullptr);
        QVERIFY(registry.find(QStringLiteral("modrinth")) != nullptr);
        QVERIFY(registry.find(QStringLiteral("technic")) != nullptr);

        // Flame is registered but unavailable without APPLICATION; others remain available.
        const auto pages = registry.providersWith(ProviderCapability::NewInstancePage);
        QCOMPARE(pages.size(), 6);
        QVERIFY(!containsId(pages, QStringLiteral("flame")));
        QVERIFY(containsId(pages, QStringLiteral("modrinth")));

        QVERIFY(registry.find(QStringLiteral("modrinth"))->createResourceAPI);
        QVERIFY(registry.find(QStringLiteral("modrinth"))->createNewInstancePage);
        QVERIFY(registry.find(QStringLiteral("modrinth"))->createModPage);
        QVERIFY(registry.find(QStringLiteral("flame"))->createResourceAPI);

        // Idempotent
        ensureBuiltInProvidersRegistered();
        QCOMPARE(registry.providers().size(), 7);
    }

    void builtinAuthProvidersAreRegisteredViaEnsure()
    {
        using namespace Auth;

        auto& registry = AuthProviderRegistry::instance();
        registry.resetForTesting();
        QCOMPARE(registry.providers().size(), 0);

        ensureBuiltInProvidersRegistered();

        const auto* msa = registry.find(QStringLiteral("msa"));
        const auto* offline = registry.find(QStringLiteral("offline"));
        QVERIFY(msa != nullptr);
        QVERIFY(offline != nullptr);
        QVERIFY(msa->hasCapability(AuthCapability::InteractiveLogin));
        QVERIFY(msa->hasCapability(AuthCapability::SilentRefresh));
        QVERIFY(offline->hasCapability(AuthCapability::OfflineSession));

        ensureBuiltInProvidersRegistered();
        QCOMPARE(registry.providers().size(), 2);
    }

   private:
    static bool containsId(const QList<const ModPlatform::ModPlatformProvider*>& providers, const QString& id)
    {
        for (const auto* provider : providers) {
            if (provider && provider->id == id) {
                return true;
            }
        }
        return false;
    }
};

QTEST_GUILESS_MAIN(R3BuiltinRegistrationTest)

#include "R3BuiltinRegistration_test.moc"
