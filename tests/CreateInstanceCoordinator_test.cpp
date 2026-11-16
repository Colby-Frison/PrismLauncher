// Intention: CreateInstanceCoordinator (GitHub R2 / R2.2) owns create/import
// InstanceTask staging+commit sequencing—wrap via InstanceCatalog, run via an
// injected runner, remember last-used prefs—without MainWindow or dialogs.

#include <QTest>
#include <QVariant>

#include <CreateInstanceCoordinator.h>
#include <InstanceTask.h>
#include <services/InstanceCatalog.h>
#include <services/LauncherSettings.h>
#include <settings/Setting.h>
#include <tasks/Task.h>

#include <map>
#include <memory>

namespace {

class ImmediateTask : public Task {
   public:
    explicit ImmediateTask(bool succeed) : m_succeed(succeed) {}

   protected:
    void executeTask() override
    {
        if (m_succeed) {
            emitSucceeded();
        } else {
            emitFailed(QStringLiteral("forced failure"));
        }
    }

   private:
    bool m_succeed = true;
};

class StubInstanceTask : public InstanceTask {
   protected:
    void executeTask() override { emitSucceeded(); }
};

class FakeInstanceCatalog : public InstanceCatalog {
   public:
    bool succeedWrap = true;
    bool wrapSucceedsTask = true;
    int wrapCalls = 0;
    InstanceTask* lastWrapped = nullptr;
    Task* lastStaged = nullptr;

    int count() const override { return 0; }
    MinecraftInstance* at(int) const override { return nullptr; }
    MinecraftInstance* getById(const QString&) const override { return nullptr; }
    MinecraftInstance* getByManagedName(const QString&) const override { return nullptr; }

    InstanceList::InstListError loadList() override { return InstanceList::NoError; }
    void saveNow() override {}

    QStringList getGroups() override { return {}; }
    GroupId getInstanceGroup(const InstanceId&) const override { return {}; }
    void setInstanceGroup(const InstanceId&, GroupId) override {}
    void deleteGroup(const GroupId&) override {}
    void renameGroup(const GroupId&, const GroupId&) override {}

    bool trashInstance(const InstanceId&) override { return false; }
    bool trashedSomething() const override { return false; }
    bool undoTrashInstance() override { return false; }
    void deleteInstance(const InstanceId&) override {}

    Task* wrapInstanceTask(InstanceTask* task) override
    {
        ++wrapCalls;
        lastWrapped = task;
        // Match InstanceStaging: catalog/wrap consumes the InstanceTask; caller owns the returned Task*.
        m_ownedChild.reset(task);
        if (!succeedWrap) {
            lastStaged = nullptr;
            return nullptr;
        }
        lastStaged = new ImmediateTask(wrapSucceedsTask);
        return lastStaged;
    }

    QString getStagedInstancePath(const QString&) override { return {}; }
    bool commitStagedInstance(const QString&, const InstanceTask&, QString) override { return false; }

    QString primaryDir() const override { return {}; }
    InstanceList* model() override { return nullptr; }

    void resetCallCounts()
    {
        wrapCalls = 0;
        lastWrapped = nullptr;
        lastStaged = nullptr;
    }

   private:
    std::unique_ptr<InstanceTask> m_ownedChild;
};

class FakeLauncherSettings : public LauncherSettings {
   public:
    std::map<QString, QVariant> values;
    int setCalls = 0;

    QVariant get(const QString& id) override
    {
        auto it = values.find(id);
        if (it == values.end()) {
            return {};
        }
        return it->second;
    }

    bool set(const QString& id, QVariant value) override
    {
        ++setCalls;
        values[id] = value;
        return true;
    }

    void reset(const QString& id) override { values.erase(id); }
    bool contains(const QString& id) override { return values.count(id) > 0; }
    std::shared_ptr<Setting> getSetting(const QString&) const override { return {}; }
    std::shared_ptr<Setting> getOrRegisterSetting(const QString&, QVariant) override { return {}; }

    QVariant getPlaytime(const QString&) override { return {}; }
    bool setPlaytime(const QString&, QVariant) override { return false; }

    SettingsObject* settingsObject() override { return nullptr; }
    SettingsObject* playtimeSettingsObject() override { return nullptr; }

    void resetCallCounts() { setCalls = 0; }
};

class FakeTaskRunner : public IInstanceTaskRunner {
   public:
    bool runResult = true;
    int runCalls = 0;
    Task* lastTask = nullptr;
    bool startTask = false;

    bool run(Task* stagedTask) override
    {
        ++runCalls;
        lastTask = stagedTask;
        if (startTask && stagedTask) {
            stagedTask->start();
        }
        return runResult;
    }

    void resetCallCounts()
    {
        runCalls = 0;
        lastTask = nullptr;
    }
};

}  // namespace

class CreateInstanceCoordinatorTest : public QObject {
    Q_OBJECT

   private slots:
    void init();
    void cleanup();

    void nullTaskIsRejected();
    void successfulCreateWrapsAndRuns();
    void wrapFailureIsRejected();
    void runnerFailureReportsFailed();
    void remembersPrefsWhenRequested();
    void skipsPrefsWhenNotRequested();
    void resolveInitialGroupPrefersExplicit();
    void resolveInitialGroupFallsBackToSettings();

   private:
    std::unique_ptr<FakeInstanceCatalog> m_catalog;
    std::unique_ptr<FakeLauncherSettings> m_settings;
    std::unique_ptr<FakeTaskRunner> m_runner;
};

void CreateInstanceCoordinatorTest::init()
{
    m_catalog = std::make_unique<FakeInstanceCatalog>();
    m_settings = std::make_unique<FakeLauncherSettings>();
    m_runner = std::make_unique<FakeTaskRunner>();
}

void CreateInstanceCoordinatorTest::cleanup()
{
    m_runner.reset();
    m_settings.reset();
    m_catalog.reset();
}

void CreateInstanceCoordinatorTest::nullTaskIsRejected()
{
    CreateInstanceCoordinator coordinator(m_catalog.get(), m_settings.get(), m_runner.get());

    CreateInstanceCoordinator::CreateRequest request;
    request.task = nullptr;

    QCOMPARE(coordinator.createFromTask(request), CreateInstanceCoordinator::Outcome::Rejected);
    QCOMPARE(m_catalog->wrapCalls, 0);
    QCOMPARE(m_runner->runCalls, 0);
}

void CreateInstanceCoordinatorTest::successfulCreateWrapsAndRuns()
{
    m_catalog->succeedWrap = true;
    m_runner->runResult = true;

    auto* creation = new StubInstanceTask();
    CreateInstanceCoordinator coordinator(m_catalog.get(), m_settings.get(), m_runner.get());

    CreateInstanceCoordinator::CreateRequest request;
    request.task = creation;

    QCOMPARE(coordinator.createFromTask(request), CreateInstanceCoordinator::Outcome::Succeeded);
    QCOMPARE(m_catalog->wrapCalls, 1);
    QCOMPARE(m_catalog->lastWrapped, creation);
    QCOMPARE(m_runner->runCalls, 1);
    QCOMPARE(m_runner->lastTask, m_catalog->lastStaged);
}

void CreateInstanceCoordinatorTest::wrapFailureIsRejected()
{
    m_catalog->succeedWrap = false;

    auto* creation = new StubInstanceTask();
    CreateInstanceCoordinator coordinator(m_catalog.get(), m_settings.get(), m_runner.get());

    CreateInstanceCoordinator::CreateRequest request;
    request.task = creation;

    QCOMPARE(coordinator.createFromTask(request), CreateInstanceCoordinator::Outcome::Rejected);
    QCOMPARE(m_catalog->wrapCalls, 1);
    QCOMPARE(m_runner->runCalls, 0);
}

void CreateInstanceCoordinatorTest::runnerFailureReportsFailed()
{
    m_catalog->succeedWrap = true;
    m_runner->runResult = false;

    auto* creation = new StubInstanceTask();
    CreateInstanceCoordinator coordinator(m_catalog.get(), m_settings.get(), m_runner.get());

    CreateInstanceCoordinator::CreateRequest request;
    request.task = creation;

    QCOMPARE(coordinator.createFromTask(request), CreateInstanceCoordinator::Outcome::Failed);
    QCOMPARE(m_runner->runCalls, 1);
}

void CreateInstanceCoordinatorTest::remembersPrefsWhenRequested()
{
    m_catalog->succeedWrap = true;
    m_runner->runResult = true;

    auto* creation = new StubInstanceTask();
    CreateInstanceCoordinator coordinator(m_catalog.get(), m_settings.get(), m_runner.get());

    CreateInstanceCoordinator::CreateRequest request;
    request.task = creation;
    request.rememberPrefs = true;
    request.groupToRemember = QStringLiteral("Modpacks");
    request.instDirToRemember = QStringLiteral("C:/instances");

    QCOMPARE(coordinator.createFromTask(request), CreateInstanceCoordinator::Outcome::Succeeded);
    QCOMPARE(m_settings->get(QStringLiteral("LastUsedGroupForNewInstance")).toString(), QStringLiteral("Modpacks"));
    QCOMPARE(m_settings->get(QStringLiteral("LastUsedInstDirForNewInstance")).toString(), QStringLiteral("C:/instances"));
    QVERIFY(m_settings->setCalls >= 2);
}

void CreateInstanceCoordinatorTest::skipsPrefsWhenNotRequested()
{
    m_catalog->succeedWrap = true;
    m_runner->runResult = true;

    auto* creation = new StubInstanceTask();
    CreateInstanceCoordinator coordinator(m_catalog.get(), m_settings.get(), m_runner.get());

    CreateInstanceCoordinator::CreateRequest request;
    request.task = creation;
    request.rememberPrefs = false;
    request.groupToRemember = QStringLiteral("Modpacks");

    QCOMPARE(coordinator.createFromTask(request), CreateInstanceCoordinator::Outcome::Succeeded);
    QCOMPARE(m_settings->setCalls, 0);
}

void CreateInstanceCoordinatorTest::resolveInitialGroupPrefersExplicit()
{
    m_settings->values[QStringLiteral("LastUsedGroupForNewInstance")] = QStringLiteral("Saved");
    CreateInstanceCoordinator coordinator(m_catalog.get(), m_settings.get(), m_runner.get());

    QCOMPARE(coordinator.resolveInitialGroup(QStringLiteral("FromAction")), QStringLiteral("FromAction"));
}

void CreateInstanceCoordinatorTest::resolveInitialGroupFallsBackToSettings()
{
    m_settings->values[QStringLiteral("LastUsedGroupForNewInstance")] = QStringLiteral("Saved");
    CreateInstanceCoordinator coordinator(m_catalog.get(), m_settings.get(), m_runner.get());

    QCOMPARE(coordinator.resolveInitialGroup(QString()), QStringLiteral("Saved"));
}

QTEST_GUILESS_MAIN(CreateInstanceCoordinatorTest)

#include "CreateInstanceCoordinator_test.moc"
