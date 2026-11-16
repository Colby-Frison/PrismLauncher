// Intention: AccountCoordinator (GitHub R2 / R2.3) owns account add/refresh/default
// and OAuth-callback delivery without MainWindow or dialog code.

#include <QSignalSpy>
#include <QTest>
#include <QVariantMap>
#include <QVector>

#include <memory>

#include <AccountCoordinator.h>
#include <minecraft/auth/MinecraftAccount.h>
#include <services/AccountService.h>

namespace {

class FakeAccountService : public AccountService {
   public:
    QVector<MinecraftAccountPtr> accounts;
    MinecraftAccountPtr defaultAcct;
    bool validAccount = false;

    int addAccountCalls = 0;
    int setDefaultCalls = 0;
    int removeAccountCalls = 0;
    int requestRefreshCalls = 0;
    QString lastRefreshId;
    QModelIndex lastRemovedIndex;
    MinecraftAccountPtr lastAdded;
    MinecraftAccountPtr lastSetDefault;

    int count() const override { return accounts.size(); }

    MinecraftAccountPtr at(int index) const override
    {
        if (index < 0 || index >= accounts.size()) {
            return nullptr;
        }
        return accounts.at(index);
    }

    void addAccount(MinecraftAccountPtr account) override
    {
        ++addAccountCalls;
        lastAdded = account;
        accounts.append(account);
    }

    void removeAccount(QModelIndex index) override
    {
        ++removeAccountCalls;
        lastRemovedIndex = index;
        if (index.isValid() && index.row() >= 0 && index.row() < accounts.size()) {
            if (accounts.at(index.row()) == defaultAcct) {
                defaultAcct = nullptr;
            }
            accounts.removeAt(index.row());
        }
    }

    int findAccountByProfileId(const QString&) const override { return -1; }
    MinecraftAccountPtr getAccountByProfileName(const QString&) const override { return nullptr; }
    QStringList profileNames() const override { return {}; }

    MinecraftAccountPtr defaultAccount() const override { return defaultAcct; }

    void setDefaultAccount(MinecraftAccountPtr account) override
    {
        ++setDefaultCalls;
        lastSetDefault = account;
        defaultAcct = account;
    }

    bool anyAccountIsValid() override { return validAccount; }

    void requestRefresh(QString accountId) override
    {
        ++requestRefreshCalls;
        lastRefreshId = accountId;
    }

    void queueRefresh(QString) override {}

    AccountList* model() override { return nullptr; }

    void resetCallCounts()
    {
        addAccountCalls = 0;
        setDefaultCalls = 0;
        removeAccountCalls = 0;
        requestRefreshCalls = 0;
        lastRefreshId.clear();
        lastRemovedIndex = QModelIndex();
        lastAdded = nullptr;
        lastSetDefault = nullptr;
    }
};

}  // namespace

class AccountCoordinatorTest : public QObject {
    Q_OBJECT

   private slots:
    void init();
    void cleanup();

    void firstAccountAddSetsDefault();
    void subsequentAddLeavesDefault();
    void forceDefaultSetsDefault();
    void nullAccountRejected();
    void offlineRequiresValidMicrosoft();
    void offlineAddSucceedsWhenValid();
    void refreshRequestsByInternalId();
    void clearDefaultClears();
    void removeDelegatesToService();
    void oauthReplyIsDelivered();

   private:
    std::unique_ptr<FakeAccountService> m_accounts;
    std::unique_ptr<AccountCoordinator> m_coordinator;
};

void AccountCoordinatorTest::init()
{
    m_accounts = std::make_unique<FakeAccountService>();
    m_coordinator = std::make_unique<AccountCoordinator>(m_accounts.get());
}

void AccountCoordinatorTest::cleanup()
{
    m_coordinator.reset();
    m_accounts.reset();
}

void AccountCoordinatorTest::firstAccountAddSetsDefault()
{
    auto account = MinecraftAccount::createBlankMSA();

    const auto outcome = m_coordinator->addAccount(account);

    QCOMPARE(outcome, AccountCoordinator::AddOutcome::AddedAndSetDefault);
    QCOMPARE(m_accounts->addAccountCalls, 1);
    QCOMPARE(m_accounts->setDefaultCalls, 1);
    QCOMPARE(m_accounts->defaultAccount(), account);
    QCOMPARE(m_accounts->count(), 1);
}

void AccountCoordinatorTest::subsequentAddLeavesDefault()
{
    auto first = MinecraftAccount::createBlankMSA();
    auto second = MinecraftAccount::createBlankMSA();
    m_coordinator->addAccount(first);
    m_accounts->resetCallCounts();

    const auto outcome = m_coordinator->addAccount(second);

    QCOMPARE(outcome, AccountCoordinator::AddOutcome::Added);
    QCOMPARE(m_accounts->addAccountCalls, 1);
    QCOMPARE(m_accounts->setDefaultCalls, 0);
    QCOMPARE(m_accounts->defaultAccount(), first);
    QCOMPARE(m_accounts->count(), 2);
}

void AccountCoordinatorTest::forceDefaultSetsDefault()
{
    auto first = MinecraftAccount::createBlankMSA();
    auto second = MinecraftAccount::createBlankMSA();
    m_coordinator->addAccount(first);
    m_accounts->resetCallCounts();

    const auto outcome = m_coordinator->addAccount(second, true);

    QCOMPARE(outcome, AccountCoordinator::AddOutcome::AddedAndSetDefault);
    QCOMPARE(m_accounts->setDefaultCalls, 1);
    QCOMPARE(m_accounts->defaultAccount(), second);
}

void AccountCoordinatorTest::nullAccountRejected()
{
    const auto outcome = m_coordinator->addAccount(nullptr);

    QCOMPARE(outcome, AccountCoordinator::AddOutcome::Rejected);
    QCOMPARE(m_accounts->addAccountCalls, 0);
}

void AccountCoordinatorTest::offlineRequiresValidMicrosoft()
{
    m_accounts->validAccount = false;
    auto offline = MinecraftAccount::createOffline(QStringLiteral("Player"));

    const auto outcome = m_coordinator->addOfflineAccount(offline);

    QCOMPARE(outcome, AccountCoordinator::AddOutcome::RejectedMissingMicrosoftAccount);
    QCOMPARE(m_accounts->addAccountCalls, 0);
}

void AccountCoordinatorTest::offlineAddSucceedsWhenValid()
{
    m_accounts->validAccount = true;
    auto msa = MinecraftAccount::createBlankMSA();
    m_coordinator->addAccount(msa);
    m_accounts->resetCallCounts();
    // After first MSA, fake still reports valid when asked.
    m_accounts->validAccount = true;

    auto offline = MinecraftAccount::createOffline(QStringLiteral("Player"));
    const auto outcome = m_coordinator->addOfflineAccount(offline);

    QCOMPARE(outcome, AccountCoordinator::AddOutcome::Added);
    QCOMPARE(m_accounts->addAccountCalls, 1);
    QCOMPARE(m_accounts->count(), 2);
}

void AccountCoordinatorTest::refreshRequestsByInternalId()
{
    auto account = MinecraftAccount::createBlankMSA();
    m_coordinator->addAccount(account);
    m_accounts->resetCallCounts();

    m_coordinator->refreshAccount(account);

    QCOMPARE(m_accounts->requestRefreshCalls, 1);
    QCOMPARE(m_accounts->lastRefreshId, account->internalId());
}

void AccountCoordinatorTest::clearDefaultClears()
{
    auto account = MinecraftAccount::createBlankMSA();
    m_coordinator->addAccount(account);
    m_accounts->resetCallCounts();

    m_coordinator->clearDefaultAccount();

    QCOMPARE(m_accounts->setDefaultCalls, 1);
    QVERIFY(m_accounts->defaultAccount().get() == nullptr);
}

void AccountCoordinatorTest::removeDelegatesToService()
{
    auto account = MinecraftAccount::createBlankMSA();
    m_coordinator->addAccount(account);
    m_accounts->resetCallCounts();

    m_coordinator->removeAccount(QModelIndex());

    QCOMPARE(m_accounts->removeAccountCalls, 1);
}

void AccountCoordinatorTest::oauthReplyIsDelivered()
{
    QSignalSpy spy(m_coordinator.get(), &AccountCoordinator::oauthReplyReceived);
    QVariantMap payload;
    payload.insert(QStringLiteral("code"), QStringLiteral("abc"));
    payload.insert(QStringLiteral("state"), QStringLiteral("xyz"));

    m_coordinator->deliverOAuthReply(payload);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toMap(), payload);
}

QTEST_GUILESS_MAIN(AccountCoordinatorTest)

#include "AccountCoordinator_test.moc"
