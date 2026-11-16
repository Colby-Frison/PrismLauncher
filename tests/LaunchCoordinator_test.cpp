// Intention: LaunchCoordinator (GitHub R2 / R2.1) owns launch orchestration without UI—
// decide whether to start a LaunchController, configure and store it, track running, and
// abort on kill—without MainWindow or APPLICATION.

#include <QTemporaryDir>
#include <QTest>
#include <QWidget>

#include <LaunchController.h>
#include <LaunchCoordinator.h>
#include <LaunchMode.h>
#include <minecraft/MinecraftInstance.h>
#include <minecraft/auth/MinecraftAccount.h>
#include <minecraft/launch/MinecraftTarget.h>
#include <settings/INISettingsObject.h>
#include <tools/BaseProfiler.h>

#include <map>
#include <memory>

namespace {

class FakeMinecraftInstance : public MinecraftInstance {
   public:
    FakeMinecraftInstance(SettingsObject* globalSettings, std::unique_ptr<SettingsObject> settings, const QString& rootDir)
        : MinecraftInstance(globalSettings, std::move(settings), rootDir)
    {
    }

    void setCanLaunch(bool value) { m_canLaunch = value; }
    void setCanEdit(bool value) { m_canEdit = value; }

    bool canLaunch() const override { return m_canLaunch; }
    bool canEdit() const override { return m_canEdit; }

   private:
    bool m_canLaunch = true;
    bool m_canEdit = true;
};

class DummyProfilerFactory : public BaseProfilerFactory {
   public:
    QString name() const override { return QStringLiteral("dummy-profiler"); }
    void registerSettings(SettingsObject*) override {}
    BaseExternalTool* createTool(BaseInstance*, QObject* parent = 0) override
    {
        Q_UNUSED(parent);
        return nullptr;
    }
    bool check(QString*) override { return true; }
    bool check(const QString&, QString*) override { return true; }
};

class RecordingLaunchController : public LaunchController {
   public:
    int abortCalls = 0;
    bool abortResult = true;

    bool abort() override
    {
        ++abortCalls;
        return abortResult;
    }
};

class FakeLaunchExtrasStore : public ILaunchExtrasStore {
   public:
    bool saveOpenWindowResult = true;
    int saveOpenWindowCalls = 0;
    QString lastSaveOpenWindowId;
    int setControllerCalls = 0;
    QString lastSetControllerId;
    LaunchController* lastStoredController = nullptr;

    bool saveOpenWindow(const QString& instanceId) override
    {
        ++saveOpenWindowCalls;
        lastSaveOpenWindowId = instanceId;
        return saveOpenWindowResult;
    }

    void setController(const QString& instanceId, std::unique_ptr<LaunchController> controller) override
    {
        ++setControllerCalls;
        lastSetControllerId = instanceId;
        lastStoredController = controller.get();
        m_controllers[instanceId] = std::move(controller);
    }

    LaunchController* controller(const QString& instanceId) const override
    {
        auto it = m_controllers.find(instanceId);
        if (it == m_controllers.end()) {
            return nullptr;
        }
        return it->second.get();
    }

    void seedController(const QString& instanceId, std::unique_ptr<LaunchController> controller)
    {
        setController(instanceId, std::move(controller));
        // Do not count seeding as a coordinator store in assertions that reset counters after seed.
    }

    void resetCallCounts()
    {
        saveOpenWindowCalls = 0;
        setControllerCalls = 0;
        lastSaveOpenWindowId.clear();
        lastSetControllerId.clear();
        lastStoredController = nullptr;
    }

   private:
    std::map<QString, std::unique_ptr<LaunchController>> m_controllers;
};

class FakeRunningInstanceCounter : public IRunningInstanceCounter {
   public:
    int addRunningInstanceCalls = 0;

    void addRunningInstance() override { ++addRunningInstanceCalls; }

    void resetCallCounts() { addRunningInstanceCalls = 0; }
};

}  // namespace

class LaunchCoordinatorTest : public QObject {
    Q_OBJECT

   private slots:
    void init();
    void cleanup();

    void updateRunningBlocksLaunch();
    void setUpdateRunningTogglesBlock();
    void successfulStartWiresFields();
    void saveOpenWindowFailureRejects();
    void alreadyRunningShowsConsole();
    void editableOnlyShowsInstance();
    void otherwiseRejected();
    void killWithoutController();
    void killWithControllerAborts();
    void nullInstanceLaunch();
    void nullInstanceKill();

   private:
    LaunchCoordinator::LaunchRequest baseRequest() const;

    std::unique_ptr<QTemporaryDir> m_root;
    std::unique_ptr<INISettingsObject> m_global;
    std::unique_ptr<FakeMinecraftInstance> m_instance;
    std::unique_ptr<FakeLaunchExtrasStore> m_extras;
    std::unique_ptr<FakeRunningInstanceCounter> m_running;
    std::unique_ptr<DummyProfilerFactory> m_profiler;
};

void LaunchCoordinatorTest::init()
{
    m_root = std::make_unique<QTemporaryDir>();
    QVERIFY(m_root->isValid());

    m_global = std::make_unique<INISettingsObject>(m_root->filePath("prismlauncher.cfg"));
    // BaseInstance registers overrides against these global settings.
    m_global->registerSetting("ShowGameTime", true);
    m_global->registerSetting("RecordGameTime", true);
    m_global->registerSetting("PreLoadCommand", "");
    m_global->registerSetting("PreLaunchCommand", "");
    m_global->registerSetting("WrapperCommand", "");
    m_global->registerSetting("PostExitCommand", "");
    m_global->registerSetting("ShowConsole", true);
    m_global->registerSetting("AutoCloseConsole", false);
    m_global->registerSetting("ShowConsoleOnError", true);
    m_global->registerSetting("LogPrePostOutput", true);
    m_global->registerSetting("ConsoleMaxLines", 100000);
    m_global->registerSetting("ConsoleOverflowStop", false);

    auto instanceSettings = std::make_unique<INISettingsObject>(m_root->filePath("instance.cfg"));
    m_instance = std::make_unique<FakeMinecraftInstance>(m_global.get(), std::move(instanceSettings), m_root->filePath("instance"));
    // Skip loadSpecificSettings — it needs many more globals and is unused by LaunchCoordinator.
    m_instance->setCanLaunch(true);
    m_instance->setCanEdit(true);
    m_instance->setRunning(false);

    m_extras = std::make_unique<FakeLaunchExtrasStore>();
    m_running = std::make_unique<FakeRunningInstanceCounter>();
    m_profiler = std::make_unique<DummyProfilerFactory>();
}

void LaunchCoordinatorTest::cleanup()
{
    m_profiler.reset();
    m_running.reset();
    m_extras.reset();
    m_instance.reset();
    m_global.reset();
    m_root.reset();
}

LaunchCoordinator::LaunchRequest LaunchCoordinatorTest::baseRequest() const
{
    LaunchCoordinator::LaunchRequest request;
    request.instance = m_instance.get();
    request.mode = LaunchMode::Normal;
    request.parentWidget = nullptr;
    return request;
}

void LaunchCoordinatorTest::updateRunningBlocksLaunch()
{
    LaunchCoordinator coordinator(m_extras.get(), m_running.get(), true);

    const auto outcome = coordinator.requestLaunch(baseRequest());

    QCOMPARE(outcome, LaunchCoordinator::LaunchOutcome::BlockedByUpdate);
    QCOMPARE(m_extras->setControllerCalls, 0);
    QCOMPARE(m_extras->saveOpenWindowCalls, 0);
    QCOMPARE(m_running->addRunningInstanceCalls, 0);
    QVERIFY(m_extras->controller(m_instance->id()) == nullptr);
}

void LaunchCoordinatorTest::setUpdateRunningTogglesBlock()
{
    LaunchCoordinator coordinator(m_extras.get(), m_running.get(), true);
    QCOMPARE(coordinator.requestLaunch(baseRequest()), LaunchCoordinator::LaunchOutcome::BlockedByUpdate);

    coordinator.setUpdateRunning(false);
    m_extras->saveOpenWindowResult = true;
    const auto outcome = coordinator.requestLaunch(baseRequest());

    QCOMPARE(outcome, LaunchCoordinator::LaunchOutcome::Started);
    QCOMPARE(m_running->addRunningInstanceCalls, 1);
    QVERIFY(m_extras->controller(m_instance->id()) != nullptr);
}

void LaunchCoordinatorTest::successfulStartWiresFields()
{
    auto target = std::make_shared<MinecraftTarget>();
    target->address = QStringLiteral("play.example.com");
    target->port = 25565;
    auto account = MinecraftAccount::createOffline(QStringLiteral("Tester"));

    LaunchCoordinator::LaunchRequest request = baseRequest();
    request.mode = LaunchMode::Offline;
    request.targetToJoin = target;
    request.accountToUse = account;
    request.offlineName = QStringLiteral("OfflinePlayer");
    request.profiler = m_profiler.get();
    request.parentWidget = nullptr;

    LaunchCoordinator coordinator(m_extras.get(), m_running.get(), false);
    m_extras->saveOpenWindowResult = true;

    const auto outcome = coordinator.requestLaunch(request);

    QCOMPARE(outcome, LaunchCoordinator::LaunchOutcome::Started);
    QCOMPARE(m_extras->saveOpenWindowCalls, 1);
    QCOMPARE(m_extras->lastSaveOpenWindowId, m_instance->id());
    QCOMPARE(m_extras->setControllerCalls, 1);
    QCOMPARE(m_extras->lastSetControllerId, m_instance->id());
    QCOMPARE(m_running->addRunningInstanceCalls, 1);

    auto* stored = m_extras->controller(m_instance->id());
    QVERIFY(stored != nullptr);
    QCOMPARE(stored->instance(), m_instance.get());
    // LaunchController has no public getters for mode/profiler/account/target/offlineName/parent;
    // those request fields are supplied above so production wiring is exercised once implemented.
}

void LaunchCoordinatorTest::saveOpenWindowFailureRejects()
{
    LaunchCoordinator coordinator(m_extras.get(), m_running.get(), false);
    m_extras->saveOpenWindowResult = false;

    const auto outcome = coordinator.requestLaunch(baseRequest());

    QCOMPARE(outcome, LaunchCoordinator::LaunchOutcome::Rejected);
    QCOMPARE(m_extras->saveOpenWindowCalls, 1);
    QCOMPARE(m_extras->setControllerCalls, 0);
    QCOMPARE(m_running->addRunningInstanceCalls, 0);
    QVERIFY(m_extras->controller(m_instance->id()) == nullptr);
}

void LaunchCoordinatorTest::alreadyRunningShowsConsole()
{
    m_instance->setCanLaunch(false);
    m_instance->setRunning(true);
    m_instance->setCanEdit(true);

    LaunchCoordinator coordinator(m_extras.get(), m_running.get(), false);

    const auto outcome = coordinator.requestLaunch(baseRequest());

    QCOMPARE(outcome, LaunchCoordinator::LaunchOutcome::ShowConsoleBecauseRunning);
    QCOMPARE(m_extras->setControllerCalls, 0);
    QCOMPARE(m_extras->saveOpenWindowCalls, 0);
    QCOMPARE(m_running->addRunningInstanceCalls, 0);
}

void LaunchCoordinatorTest::editableOnlyShowsInstance()
{
    m_instance->setCanLaunch(false);
    m_instance->setRunning(false);
    m_instance->setCanEdit(true);

    LaunchCoordinator coordinator(m_extras.get(), m_running.get(), false);

    const auto outcome = coordinator.requestLaunch(baseRequest());

    QCOMPARE(outcome, LaunchCoordinator::LaunchOutcome::ShowInstanceBecauseEditable);
    QCOMPARE(m_extras->setControllerCalls, 0);
    QCOMPARE(m_running->addRunningInstanceCalls, 0);
}

void LaunchCoordinatorTest::otherwiseRejected()
{
    m_instance->setCanLaunch(false);
    m_instance->setRunning(false);
    m_instance->setCanEdit(false);

    LaunchCoordinator coordinator(m_extras.get(), m_running.get(), false);

    const auto outcome = coordinator.requestLaunch(baseRequest());

    QCOMPARE(outcome, LaunchCoordinator::LaunchOutcome::Rejected);
    QCOMPARE(m_extras->setControllerCalls, 0);
    QCOMPARE(m_running->addRunningInstanceCalls, 0);
}

void LaunchCoordinatorTest::killWithoutController()
{
    LaunchCoordinator coordinator(m_extras.get(), m_running.get(), false);

    QVERIFY(!coordinator.requestKill(m_instance.get()));
}

void LaunchCoordinatorTest::killWithControllerAborts()
{
    auto recording = std::make_unique<RecordingLaunchController>();
    auto* raw = recording.get();
    raw->abortResult = true;
    m_extras->seedController(m_instance->id(), std::move(recording));
    m_extras->resetCallCounts();
    m_running->resetCallCounts();

    LaunchCoordinator coordinator(m_extras.get(), m_running.get(), false);

    QVERIFY(coordinator.requestKill(m_instance.get()));
    QCOMPARE(raw->abortCalls, 1);

    raw->abortResult = false;
    QVERIFY(!coordinator.requestKill(m_instance.get()));
    QCOMPARE(raw->abortCalls, 2);
}

void LaunchCoordinatorTest::nullInstanceLaunch()
{
    LaunchCoordinator coordinator(m_extras.get(), m_running.get(), false);
    LaunchCoordinator::LaunchRequest request;
    request.instance = nullptr;

    QCOMPARE(coordinator.requestLaunch(request), LaunchCoordinator::LaunchOutcome::Rejected);
    QCOMPARE(m_extras->setControllerCalls, 0);
    QCOMPARE(m_running->addRunningInstanceCalls, 0);
}

void LaunchCoordinatorTest::nullInstanceKill()
{
    LaunchCoordinator coordinator(m_extras.get(), m_running.get(), false);

    QVERIFY(!coordinator.requestKill(nullptr));
}

QTEST_GUILESS_MAIN(LaunchCoordinatorTest)

#include "LaunchCoordinator_test.moc"
