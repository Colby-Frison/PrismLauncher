#pragma once

#include <QImage>
#include <QList>
#include <QObject>
#include <QSet>

#include "minecraft/auth/AccountData.h"
#include "minecraft/auth/AuthStep.h"
#include "tasks/Task.h"

class AuthFlowProviderTest;

class AuthFlow : public Task {
    Q_OBJECT

   public:
    enum class Action { Refresh, Login, DeviceCode };

    explicit AuthFlow(AccountData* data, Action action = Action::Refresh);
    virtual ~AuthFlow() = default;

    void executeTask() override;

    AccountTaskState taskState() { return m_taskState; }

   public slots:
    bool abort() override;

   signals:
    void authorizeWithBrowser(const QUrl& url);
    void authorizeWithBrowserWithExtra(QString url, QString code, int expiresIn);

   protected:
    void succeed();
    void nextStep();

   private slots:
    // NOTE: true -> non-terminal state, false -> terminal state
    bool changeState(AccountTaskState newState, QString reason = QString());
    void stepFinished(AccountTaskState resultingState, QString message);

   private:
    friend class AuthFlowProviderTest;

    AccountTaskState m_taskState = AccountTaskState::STATE_CREATED;
    QList<AuthStep::Ptr> m_steps;
    AuthStep::Ptr m_currentStep;
    AccountData* m_data = nullptr;
    /**
     * Set when MSA auth was requested but no steps could be built (missing /
     * unsupported provider). Distinguished from a drained step list after a
     * successful MSA run, which must still call succeed().
     */
    bool m_msaProviderUnavailable = false;
};
