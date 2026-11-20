// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2026 Prism Launcher Contributors
 *
 *  Verifies AuthFlow fails for MSA when the auth provider is missing, and that
 *  offline flows still complete successfully with an empty step list.
 */

#include <QTest>

#include "minecraft/auth/AccountData.h"
#include "minecraft/auth/AuthFlow.h"
#include "minecraft/auth/AuthProviderRegistry.h"

class AuthFlowProviderTest : public QObject {
    Q_OBJECT

   private slots:
    void init() { Auth::AuthProviderRegistry::instance().resetForTesting(); }
    void cleanup() { Auth::AuthProviderRegistry::instance().resetForTesting(); }

    void msaFailsWhenProviderMissing()
    {
        AccountData data;
        data.type = AccountType::MSA;

        AuthFlow flow(&data, AuthFlow::Action::Login);
        flow.start();

        QTRY_VERIFY_WITH_TIMEOUT(flow.isFinished(), 2000);
        QVERIFY(flow.wasSuccessful() == false);
        QCOMPARE(flow.taskState(), AccountTaskState::STATE_FAILED_HARD);
    }

    void msaFailsWhenRequiredCapabilityMissing()
    {
        Auth::AuthProvider provider;
        provider.id = QStringLiteral("msa");
        provider.displayName = QStringLiteral("Microsoft");
        // Intentionally omit InteractiveLogin
        provider.capabilities = Auth::AuthCapability::Persistence;
        QVERIFY(Auth::AuthProviderRegistry::instance().registerProvider(std::move(provider)));

        AccountData data;
        data.type = AccountType::MSA;

        AuthFlow flow(&data, AuthFlow::Action::Login);
        flow.start();

        QTRY_VERIFY_WITH_TIMEOUT(flow.isFinished(), 2000);
        QVERIFY(!flow.wasSuccessful());
        QCOMPARE(flow.taskState(), AccountTaskState::STATE_FAILED_HARD);
    }

    void offlineStillSucceedsWithNoSteps()
    {
        AccountData data;
        data.type = AccountType::Offline;

        AuthFlow flow(&data, AuthFlow::Action::Login);
        flow.start();

        QTRY_VERIFY_WITH_TIMEOUT(flow.isFinished(), 2000);
        QVERIFY(flow.wasSuccessful());
        QCOMPARE(flow.taskState(), AccountTaskState::STATE_SUCCEEDED);
    }
};

QTEST_GUILESS_MAIN(AuthFlowProviderTest)

#include "AuthFlowProvider_test.moc"
