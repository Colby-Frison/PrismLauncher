// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2026 Prism Launcher Contributors
 *
 *  Verifies AuthFlow fails for MSA when the auth provider is missing, that
 *  offline flows still complete successfully with an empty step list, and that
 *  a drained MSA step list after successful steps still succeeds.
 */

#include <QTest>

#include "QObjectPtr.h"
#include "minecraft/auth/AccountData.h"
#include "minecraft/auth/AuthFlow.h"
#include "minecraft/auth/AuthProviderRegistry.h"
#include "minecraft/auth/AuthStep.h"

/** Emits STATE_WORKING immediately — models a successful MSA auth step. */
class InstantSuccessStep : public AuthStep {
    Q_OBJECT
   public:
    using AuthStep::AuthStep;
    QString describe() override { return QStringLiteral("Instant success"); }
    void perform() override { emit finished(AccountTaskState::STATE_WORKING, QStringLiteral("ok")); }
};

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

    /**
     * Regression: after MSA steps were queued, draining them with STATE_WORKING
     * must succeed. Models that state without constructing real MSAStep (needs
     * Application / network).
     */
    void msaSucceedsWhenQueuedStepsDrain()
    {
        AccountData data;
        data.type = AccountType::MSA;

        AuthFlow flow(&data, AuthFlow::Action::Login);
        QVERIFY(flow.m_msaProviderUnavailable);
        QVERIFY(flow.m_steps.isEmpty());

        // Same post-construction state as when the MSA provider successfully
        // queued steps: failure flag clear, at least one step present.
        flow.m_msaProviderUnavailable = false;
        flow.m_steps.append(makeShared<InstantSuccessStep>(&data));

        flow.start();

        QTRY_VERIFY_WITH_TIMEOUT(flow.isFinished(), 2000);
        QVERIFY(flow.wasSuccessful());
        QCOMPARE(flow.taskState(), AccountTaskState::STATE_SUCCEEDED);
    }
};

QTEST_GUILESS_MAIN(AuthFlowProviderTest)

#include "AuthFlowProvider_test.moc"
