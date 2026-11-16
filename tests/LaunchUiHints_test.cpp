// Intention: LaunchUiHints (GitHub R2 / R2.4) maps LaunchCoordinator outcomes
// to instance-window UI actions and shares kill semantics so Application and
// MainWindow stay thin presentation/wiring layers.
//
// Seam: LaunchUiHints free functions (instanceWindowHintFor, launchAccepted,
// killInstance). Catches UX regressions when wiring MainWindow to the
// coordinator; misses full window lifecycle (still owned by Application).

#include <QTest>
#include <QTemporaryDir>

#include <LaunchController.h>
#include <LaunchCoordinator.h>
#include <LaunchUiHints.h>
#include <minecraft/MinecraftInstance.h>
#include <settings/INISettingsObject.h>

#include <map>
#include <memory>

namespace {

class FakeMinecraftInstance : public MinecraftInstance {
   public:
    FakeMinecraftInstance(SettingsObject* globalSettings, std::unique_ptr<SettingsObject> settings, const QString& rootDir)
        : MinecraftInstance(globalSettings, std::move(settings), rootDir)
    {
    }
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
    std::map<QString, std::unique_ptr<LaunchController>> controllers;

    bool saveOpenWindow(const QString&) override { return true; }
    void setController(const QString& instanceId, std::unique_ptr<LaunchController> controller) override
    {
        controllers[instanceId] = std::move(controller);
    }
    LaunchController* controller(const QString& instanceId) const override
    {
        auto it = controllers.find(instanceId);
        return it == controllers.end() ? nullptr : it->second.get();
    }
    void seedController(const QString& id, std::unique_ptr<LaunchController> controller) { setController(id, std::move(controller)); }
};

class FakeRunningCounter : public IRunningInstanceCounter {
   public:
    void addRunningInstance() override {}
};

}  // namespace

class LaunchUiHintsTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase();
    void cleanupTestCase();

    void startedDoesNotOpenWindow();
    void blockedDoesNotOpenWindow();
    void rejectedDoesNotOpenWindow();
    void runningOpensConsole();
    void editableOpensInstance();
    void launchAcceptedOutcomes();
    void handleLaunchOutcomeStartedNoWindow();
    void handleLaunchOutcomeBlockedNoWindow();
    void handleLaunchOutcomeOpensConsole();
    void handleLaunchOutcomeOpensInstance();
    void killNullOrNotRunningFails();
    void killRunningWithoutControllerSucceeds();
    void killWithControllerUsesAbort();

   private:
    QTemporaryDir m_temp;
    std::unique_ptr<INISettingsObject> m_globalSettings;
    std::shared_ptr<FakeMinecraftInstance> m_instance;
};

void LaunchUiHintsTest::initTestCase()
{
    QVERIFY(m_temp.isValid());
    m_globalSettings = std::make_unique<INISettingsObject>(m_temp.filePath(QStringLiteral("global.ini")));
    // BaseInstance registers overrides against these global settings.
    m_globalSettings->registerSetting("ShowGameTime", true);
    m_globalSettings->registerSetting("RecordGameTime", true);
    m_globalSettings->registerSetting("PreLoadCommand", "");
    m_globalSettings->registerSetting("PreLaunchCommand", "");
    m_globalSettings->registerSetting("WrapperCommand", "");
    m_globalSettings->registerSetting("PostExitCommand", "");
    m_globalSettings->registerSetting("ShowConsole", true);
    m_globalSettings->registerSetting("AutoCloseConsole", false);
    m_globalSettings->registerSetting("ShowConsoleOnError", true);
    m_globalSettings->registerSetting("LogPrePostOutput", true);
    m_globalSettings->registerSetting("ConsoleMaxLines", 100000);
    m_globalSettings->registerSetting("ConsoleOverflowStop", false);

    auto instSettings = std::make_unique<INISettingsObject>(m_temp.filePath(QStringLiteral("instance.ini")));
    m_instance = std::make_shared<FakeMinecraftInstance>(m_globalSettings.get(), std::move(instSettings),
                                                        m_temp.filePath(QStringLiteral("instance")));
}

void LaunchUiHintsTest::cleanupTestCase()
{
    m_instance.reset();
    m_globalSettings.reset();
}

void LaunchUiHintsTest::startedDoesNotOpenWindow()
{
    const auto hint = LaunchUiHints::instanceWindowHintFor(LaunchCoordinator::LaunchOutcome::Started);
    QVERIFY(!hint.open);
    QVERIFY(hint.page.isEmpty());
}

void LaunchUiHintsTest::blockedDoesNotOpenWindow()
{
    const auto hint = LaunchUiHints::instanceWindowHintFor(LaunchCoordinator::LaunchOutcome::BlockedByUpdate);
    QVERIFY(!hint.open);
}

void LaunchUiHintsTest::rejectedDoesNotOpenWindow()
{
    const auto hint = LaunchUiHints::instanceWindowHintFor(LaunchCoordinator::LaunchOutcome::Rejected);
    QVERIFY(!hint.open);
}

void LaunchUiHintsTest::runningOpensConsole()
{
    const auto hint = LaunchUiHints::instanceWindowHintFor(LaunchCoordinator::LaunchOutcome::ShowConsoleBecauseRunning);
    QVERIFY(hint.open);
    QCOMPARE(hint.page, QStringLiteral("console"));
}

void LaunchUiHintsTest::editableOpensInstance()
{
    const auto hint = LaunchUiHints::instanceWindowHintFor(LaunchCoordinator::LaunchOutcome::ShowInstanceBecauseEditable);
    QVERIFY(hint.open);
    QVERIFY(hint.page.isEmpty());
}

void LaunchUiHintsTest::launchAcceptedOutcomes()
{
    QVERIFY(LaunchUiHints::launchAccepted(LaunchCoordinator::LaunchOutcome::Started));
    QVERIFY(LaunchUiHints::launchAccepted(LaunchCoordinator::LaunchOutcome::ShowConsoleBecauseRunning));
    QVERIFY(LaunchUiHints::launchAccepted(LaunchCoordinator::LaunchOutcome::ShowInstanceBecauseEditable));
    QVERIFY(!LaunchUiHints::launchAccepted(LaunchCoordinator::LaunchOutcome::BlockedByUpdate));
    QVERIFY(!LaunchUiHints::launchAccepted(LaunchCoordinator::LaunchOutcome::Rejected));
}

void LaunchUiHintsTest::handleLaunchOutcomeStartedNoWindow()
{
    int showCalls = 0;
    const bool accepted = LaunchUiHints::handleLaunchOutcome(LaunchCoordinator::LaunchOutcome::Started,
                                                             [&](const LaunchUiHints::InstanceWindowHint&) { ++showCalls; });
    QVERIFY(accepted);
    QCOMPARE(showCalls, 0);
}

void LaunchUiHintsTest::handleLaunchOutcomeBlockedNoWindow()
{
    int showCalls = 0;
    const bool accepted = LaunchUiHints::handleLaunchOutcome(LaunchCoordinator::LaunchOutcome::BlockedByUpdate,
                                                             [&](const LaunchUiHints::InstanceWindowHint&) { ++showCalls; });
    QVERIFY(!accepted);
    QCOMPARE(showCalls, 0);
}

void LaunchUiHintsTest::handleLaunchOutcomeOpensConsole()
{
    int showCalls = 0;
    QString page;
    const bool accepted =
        LaunchUiHints::handleLaunchOutcome(LaunchCoordinator::LaunchOutcome::ShowConsoleBecauseRunning,
                                           [&](const LaunchUiHints::InstanceWindowHint& hint) {
                                               ++showCalls;
                                               page = hint.page;
                                           });
    QVERIFY(accepted);
    QCOMPARE(showCalls, 1);
    QCOMPARE(page, QStringLiteral("console"));
}

void LaunchUiHintsTest::handleLaunchOutcomeOpensInstance()
{
    int showCalls = 0;
    bool open = false;
    const bool accepted =
        LaunchUiHints::handleLaunchOutcome(LaunchCoordinator::LaunchOutcome::ShowInstanceBecauseEditable,
                                           [&](const LaunchUiHints::InstanceWindowHint& hint) {
                                               ++showCalls;
                                               open = hint.open;
                                           });
    QVERIFY(accepted);
    QCOMPARE(showCalls, 1);
    QVERIFY(open);
}

void LaunchUiHintsTest::killNullOrNotRunningFails()
{
    FakeLaunchExtrasStore extras;
    FakeRunningCounter running;
    LaunchCoordinator coordinator(&extras, &running, false);

    QVERIFY(!LaunchUiHints::killInstance(&coordinator, &extras, nullptr));

    m_instance->setRunning(false);
    QVERIFY(!LaunchUiHints::killInstance(&coordinator, &extras, m_instance.get()));
}

void LaunchUiHintsTest::killRunningWithoutControllerSucceeds()
{
    FakeLaunchExtrasStore extras;
    FakeRunningCounter running;
    LaunchCoordinator coordinator(&extras, &running, false);

    m_instance->setRunning(true);
    QVERIFY(LaunchUiHints::killInstance(&coordinator, &extras, m_instance.get()));
}

void LaunchUiHintsTest::killWithControllerUsesAbort()
{
    FakeLaunchExtrasStore extras;
    FakeRunningCounter running;
    LaunchCoordinator coordinator(&extras, &running, false);

    auto recording = std::make_unique<RecordingLaunchController>();
    auto* raw = recording.get();
    raw->abortResult = true;
    extras.seedController(m_instance->id(), std::move(recording));

    m_instance->setRunning(true);
    QVERIFY(LaunchUiHints::killInstance(&coordinator, &extras, m_instance.get()));
    QCOMPARE(raw->abortCalls, 1);

    raw->abortResult = false;
    QVERIFY(!LaunchUiHints::killInstance(&coordinator, &extras, m_instance.get()));
    QCOMPARE(raw->abortCalls, 2);
}

QTEST_GUILESS_MAIN(LaunchUiHintsTest)

#include "LaunchUiHints_test.moc"
