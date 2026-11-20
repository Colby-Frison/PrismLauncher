#include <QSignalSpy>
#include <QTest>

#include <net/WorkScheduler.h>

// Assumptions (the header leaves these open):
// - request() takes a host key as produced by hostKey(), so tests pass lowercase hosts.
// - hostOverrides keys are host keys (lowercase hosts).
// - "Under the new cap" means a queued ticket is granted only when runningCount() < cap.
// - hostOverrides values below 1 are also treated as 1 (Req 10 only names globalMax/perHostMax).

using Ticket = WorkScheduler::Ticket;

namespace {
const QString kModrinth = QStringLiteral("api.modrinth.com");
const QString kForge = QStringLiteral("edge.forgecdn.net");
const QString kCdn = QStringLiteral("cdn.modrinth.com");
const QString kGithub = QStringLiteral("github.com");

SchedulerLimits makeLimits(int globalMax, int perHostMax, QHash<QString, int> overrides = {})
{
    SchedulerLimits limits;
    limits.globalMax = globalMax;
    limits.perHostMax = perHostMax;
    limits.hostOverrides = overrides;
    return limits;
}

QList<Ticket> grantedTickets(const QSignalSpy& spy)
{
    QList<Ticket> tickets;
    for (const auto& args : spy)
        tickets.append(args.at(0).value<Ticket>());
    return tickets;
}
}  // namespace

class WorkSchedulerTest : public QObject {
    Q_OBJECT

   private slots:
    // Req 1
    void test_globalCap_neverExceeded_acrossHosts()
    {
        WorkScheduler scheduler(makeLimits(4, 3));
        QSignalSpy spy(&scheduler, &WorkScheduler::granted);
        const QStringList hosts{ kModrinth, kForge, kCdn, kGithub };

        QList<Ticket> running;
        for (int round = 0; round < 3; ++round) {
            for (const auto& host : hosts) {
                const Ticket t = scheduler.request(host, WorkPriority::Foreground);
                if (scheduler.isGranted(t))
                    running.append(t);
                QVERIFY(scheduler.runningCount() <= 4);
            }
        }
        QCOMPARE(scheduler.runningCount(), 4);
        QCOMPARE(scheduler.queuedCount(), 8);

        qsizetype seen = 0;
        while (!running.isEmpty()) {
            QVERIFY(scheduler.release(running.takeFirst()));
            for (; seen < spy.count(); ++seen)
                running.append(spy.at(seen).at(0).value<Ticket>());
            QVERIFY(scheduler.runningCount() <= 4);
            QCOMPARE(scheduler.runningCount(), int(running.size()));
        }
        // Every queued ticket passed through the cap check above.
        QCOMPARE(scheduler.queuedCount(), 0);
    }

    // Req 2
    void test_perHostCap_neverExceeded_usingDefaultCap()
    {
        WorkScheduler scheduler(makeLimits(10, 2));
        QSignalSpy spy(&scheduler, &WorkScheduler::granted);

        QList<Ticket> running;
        for (int i = 0; i < 4; ++i) {
            for (const auto& host : { kModrinth, kForge }) {
                const Ticket t = scheduler.request(host, WorkPriority::Foreground);
                if (scheduler.isGranted(t))
                    running.append(t);
            }
        }
        QCOMPARE(scheduler.runningCount(kModrinth), 2);
        QCOMPARE(scheduler.runningCount(kForge), 2);
        QCOMPARE(scheduler.queuedCount(), 4);

        qsizetype seen = 0;
        while (!running.isEmpty()) {
            QVERIFY(scheduler.release(running.takeFirst()));
            for (; seen < spy.count(); ++seen)
                running.append(spy.at(seen).at(0).value<Ticket>());
            QVERIFY(scheduler.runningCount(kModrinth) <= 2);
            QVERIFY(scheduler.runningCount(kForge) <= 2);
        }
        QCOMPARE(scheduler.queuedCount(), 0);
    }

    // Req 2
    void test_perHostCap_overrideReplacesDefaultForThatHost()
    {
        // forge override is below the default, cdn override is above it.
        WorkScheduler scheduler(makeLimits(20, 2, { { kForge, 1 }, { kCdn, 4 } }));

        for (int i = 0; i < 5; ++i) {
            scheduler.request(kForge, WorkPriority::Foreground);
            scheduler.request(kCdn, WorkPriority::Foreground);
            scheduler.request(kModrinth, WorkPriority::Foreground);
        }

        QCOMPARE(scheduler.runningCount(kForge), 1);
        QCOMPARE(scheduler.runningCount(kCdn), 4);
        QCOMPARE(scheduler.runningCount(kModrinth), 2);
    }

    // Req 3
    void test_foreground_grantedBeforeBackground_regardlessOfArrivalOrder()
    {
        WorkScheduler scheduler(makeLimits(1, 10));
        const Ticket blocker = scheduler.request(kModrinth, WorkPriority::Background);
        QVERIFY(scheduler.isGranted(blocker));
        QSignalSpy spy(&scheduler, &WorkScheduler::granted);

        const Ticket bg1 = scheduler.request(kModrinth, WorkPriority::Background);
        const Ticket fg1 = scheduler.request(kModrinth, WorkPriority::Foreground);
        const Ticket bg2 = scheduler.request(kModrinth, WorkPriority::Background);
        const Ticket fg2 = scheduler.request(kModrinth, WorkPriority::Foreground);

        QVERIFY(scheduler.release(blocker));
        QVERIFY(scheduler.release(fg1));
        QVERIFY(scheduler.release(fg2));
        QVERIFY(scheduler.release(bg1));

        QCOMPARE(grantedTickets(spy), (QList<Ticket>{ fg1, fg2, bg1, bg2 }));
    }

    // Req 3
    void test_samePriority_grantedOldestFirst_data()
    {
        QTest::addColumn<bool>("foreground");
        QTest::newRow("foreground") << true;
        QTest::newRow("background") << false;
    }
    void test_samePriority_grantedOldestFirst()
    {
        QFETCH(bool, foreground);
        const WorkPriority priority = foreground ? WorkPriority::Foreground : WorkPriority::Background;

        WorkScheduler scheduler(makeLimits(1, 10));
        const Ticket blocker = scheduler.request(kModrinth, priority);
        QSignalSpy spy(&scheduler, &WorkScheduler::granted);

        const Ticket t1 = scheduler.request(kModrinth, priority);
        const Ticket t2 = scheduler.request(kForge, priority);
        const Ticket t3 = scheduler.request(kCdn, priority);

        QVERIFY(scheduler.release(blocker));
        QVERIFY(scheduler.release(t1));
        QVERIFY(scheduler.release(t2));

        QCOMPARE(grantedTickets(spy), (QList<Ticket>{ t1, t2, t3 }));
    }

    // Req 4
    void test_hostAtCap_doesNotBlockOtherHostsQueuedTickets()
    {
        WorkScheduler scheduler(makeLimits(2, 1));
        const Ticket a1 = scheduler.request(kModrinth, WorkPriority::Foreground);
        const Ticket b1 = scheduler.request(kForge, WorkPriority::Foreground);
        const Ticket a2 = scheduler.request(kModrinth, WorkPriority::Foreground);  // head of queue, host at cap
        const Ticket c1 = scheduler.request(kCdn, WorkPriority::Foreground);
        QSignalSpy spy(&scheduler, &WorkScheduler::granted);

        QVERIFY(scheduler.release(b1));

        QCOMPARE(grantedTickets(spy), (QList<Ticket>{ c1 }));
        QVERIFY(!scheduler.isGranted(a2));
        QCOMPARE(scheduler.runningCount(kModrinth), 1);
        QCOMPARE(scheduler.queuedCount(), 1);

        QVERIFY(scheduler.release(a1));
        QCOMPARE(grantedTickets(spy), (QList<Ticket>{ c1, a2 }));
    }

    // Req 4 (with Req 3 ordering: the scan grants the first ticket that fits, so a
    // skipped Foreground ticket does not hold back a Background ticket for another host)
    void test_hostAtCap_skippedForegroundDoesNotBlockBackgroundForOtherHost()
    {
        WorkScheduler scheduler(makeLimits(2, 1));
        scheduler.request(kModrinth, WorkPriority::Foreground);
        const Ticket b1 = scheduler.request(kForge, WorkPriority::Foreground);
        const Ticket a2 = scheduler.request(kModrinth, WorkPriority::Foreground);
        const Ticket c1 = scheduler.request(kCdn, WorkPriority::Background);
        QSignalSpy spy(&scheduler, &WorkScheduler::granted);

        QVERIFY(scheduler.release(b1));

        QCOMPARE(grantedTickets(spy), (QList<Ticket>{ c1 }));
        QVERIFY(!scheduler.isGranted(a2));
    }

    // Req 5 (immediate grants emit nothing, so every granted() comes from the queue)
    void test_request_withRoom_grantedImmediatelyWithoutSignal()
    {
        WorkScheduler scheduler(makeLimits(2, 2));
        QSignalSpy spy(&scheduler, &WorkScheduler::granted);

        const Ticket t1 = scheduler.request(kModrinth, WorkPriority::Foreground);
        const Ticket t2 = scheduler.request(kForge, WorkPriority::Background);

        QVERIFY(scheduler.isGranted(t1));
        QVERIFY(scheduler.isGranted(t2));
        QCOMPARE(spy.count(), 0);
    }

    // Req 5
    void test_release_grantsNextEligibleTicketExactlyOnce()
    {
        WorkScheduler scheduler(makeLimits(1, 1));
        const Ticket t1 = scheduler.request(kModrinth, WorkPriority::Foreground);
        const Ticket t2 = scheduler.request(kModrinth, WorkPriority::Foreground);
        const Ticket t3 = scheduler.request(kModrinth, WorkPriority::Foreground);
        QSignalSpy spy(&scheduler, &WorkScheduler::granted);

        QVERIFY(scheduler.release(t1));
        QCOMPARE(grantedTickets(spy), (QList<Ticket>{ t2 }));
        QVERIFY(scheduler.isGranted(t2));
        QVERIFY(!scheduler.isGranted(t3));

        QVERIFY(scheduler.release(t2));
        QCOMPARE(grantedTickets(spy), (QList<Ticket>{ t2, t3 }));

        QVERIFY(scheduler.release(t3));
        QCOMPARE(spy.count(), 2);
    }

    // Req 6
    void test_release_unknownTicket_returnsFalseAndChangesNothing()
    {
        WorkScheduler scheduler(makeLimits(1, 1));
        const Ticket running = scheduler.request(kModrinth, WorkPriority::Foreground);
        const Ticket queued = scheduler.request(kModrinth, WorkPriority::Foreground);
        const Ticket neverIssued = qMax(running, queued) + 1000;
        QSignalSpy spy(&scheduler, &WorkScheduler::granted);

        QVERIFY(!scheduler.release(0));
        QVERIFY(!scheduler.release(neverIssued));

        QCOMPARE(scheduler.runningCount(), 1);
        QCOMPARE(scheduler.runningCount(kModrinth), 1);
        QCOMPARE(scheduler.queuedCount(), 1);
        QCOMPARE(spy.count(), 0);
    }

    // Req 6
    void test_release_queuedTicket_returnsFalseAndChangesNothing()
    {
        WorkScheduler scheduler(makeLimits(1, 1));
        scheduler.request(kModrinth, WorkPriority::Foreground);
        const Ticket queued = scheduler.request(kModrinth, WorkPriority::Foreground);
        QSignalSpy spy(&scheduler, &WorkScheduler::granted);

        QVERIFY(!scheduler.release(queued));

        QVERIFY(!scheduler.isGranted(queued));
        QCOMPARE(scheduler.runningCount(), 1);
        QCOMPARE(scheduler.runningCount(kModrinth), 1);
        QCOMPARE(scheduler.queuedCount(), 1);
        QCOMPARE(spy.count(), 0);
    }

    // Req 6
    void test_release_twice_freesOnlyOneSlot()
    {
        WorkScheduler scheduler(makeLimits(2, 2));
        const Ticket t1 = scheduler.request(kModrinth, WorkPriority::Foreground);
        scheduler.request(kModrinth, WorkPriority::Foreground);
        const Ticket t3 = scheduler.request(kModrinth, WorkPriority::Foreground);
        const Ticket t4 = scheduler.request(kModrinth, WorkPriority::Foreground);
        QSignalSpy spy(&scheduler, &WorkScheduler::granted);

        QVERIFY(scheduler.release(t1));
        QCOMPARE(grantedTickets(spy), (QList<Ticket>{ t3 }));

        QVERIFY(!scheduler.release(t1));

        QCOMPARE(scheduler.runningCount(), 2);
        QCOMPARE(scheduler.runningCount(kModrinth), 2);
        QCOMPARE(scheduler.queuedCount(), 1);
        QCOMPARE(spy.count(), 1);
        QVERIFY(!scheduler.isGranted(t4));
    }

    // Req 7
    void test_cancel_queuedTicket_isRemovedAndNeverGranted()
    {
        WorkScheduler scheduler(makeLimits(1, 1));
        const Ticket t1 = scheduler.request(kModrinth, WorkPriority::Foreground);
        const Ticket t2 = scheduler.request(kModrinth, WorkPriority::Foreground);
        const Ticket t3 = scheduler.request(kModrinth, WorkPriority::Foreground);
        QSignalSpy spy(&scheduler, &WorkScheduler::granted);

        QVERIFY(scheduler.cancel(t2));
        QCOMPARE(scheduler.queuedCount(), 1);

        QVERIFY(scheduler.release(t1));
        QCOMPARE(grantedTickets(spy), (QList<Ticket>{ t3 }));

        QVERIFY(scheduler.release(t3));
        QCOMPARE(grantedTickets(spy), (QList<Ticket>{ t3 }));
        QVERIFY(!scheduler.isGranted(t2));
        QCOMPARE(scheduler.queuedCount(), 0);
        QCOMPARE(scheduler.runningCount(), 0);
    }

    // Req 7
    void test_cancel_grantedTicket_returnsFalseAndKeepsRunning()
    {
        WorkScheduler scheduler(makeLimits(1, 1));
        const Ticket t1 = scheduler.request(kModrinth, WorkPriority::Foreground);

        QVERIFY(!scheduler.cancel(t1));

        QVERIFY(scheduler.isGranted(t1));
        QCOMPARE(scheduler.runningCount(), 1);
        QCOMPARE(scheduler.runningCount(kModrinth), 1);
    }

    // Req 7 (header: cancel() returns false for unknown or already-cancelled tickets)
    void test_cancel_unknownOrAlreadyCancelled_returnsFalse()
    {
        WorkScheduler scheduler(makeLimits(1, 1));
        scheduler.request(kModrinth, WorkPriority::Foreground);
        const Ticket t2 = scheduler.request(kModrinth, WorkPriority::Foreground);
        const Ticket t3 = scheduler.request(kModrinth, WorkPriority::Foreground);

        QVERIFY(!scheduler.cancel(0));
        QVERIFY(scheduler.cancel(t2));
        QVERIFY(!scheduler.cancel(t2));
        QCOMPARE(scheduler.queuedCount(), 1);
        QVERIFY(!scheduler.isGranted(t3));
    }

    // Req 8
    void test_setLimits_loweringCaps_keepsRunningTickets()
    {
        WorkScheduler scheduler(makeLimits(4, 2));
        const QList<Ticket> running{
            scheduler.request(kModrinth, WorkPriority::Foreground),
            scheduler.request(kModrinth, WorkPriority::Foreground),
            scheduler.request(kForge, WorkPriority::Foreground),
            scheduler.request(kForge, WorkPriority::Foreground),
        };
        QSignalSpy spy(&scheduler, &WorkScheduler::granted);

        scheduler.setLimits(makeLimits(1, 1));

        QCOMPARE(scheduler.runningCount(), 4);
        QCOMPARE(scheduler.runningCount(kModrinth), 2);
        QCOMPARE(scheduler.runningCount(kForge), 2);
        for (const Ticket t : running)
            QVERIFY(scheduler.isGranted(t));
        QCOMPARE(spy.count(), 0);
    }

    // Req 8
    void test_setLimits_loweringGlobalCap_newGrantsWaitUntilUnderNewCap()
    {
        WorkScheduler scheduler(makeLimits(4, 10));
        const Ticket r1 = scheduler.request(kModrinth, WorkPriority::Foreground);
        const Ticket r2 = scheduler.request(kModrinth, WorkPriority::Foreground);
        const Ticket r3 = scheduler.request(kForge, WorkPriority::Foreground);
        scheduler.request(kForge, WorkPriority::Foreground);
        const Ticket queued = scheduler.request(kCdn, WorkPriority::Foreground);
        QSignalSpy spy(&scheduler, &WorkScheduler::granted);

        scheduler.setLimits(makeLimits(2, 10));

        QVERIFY(scheduler.release(r1));
        QCOMPARE(scheduler.runningCount(), 3);
        QVERIFY(!scheduler.isGranted(queued));

        QVERIFY(scheduler.release(r2));
        QCOMPARE(scheduler.runningCount(), 2);
        QVERIFY(!scheduler.isGranted(queued));
        QCOMPARE(spy.count(), 0);

        QVERIFY(scheduler.release(r3));
        QCOMPARE(grantedTickets(spy), (QList<Ticket>{ queued }));
        QCOMPARE(scheduler.runningCount(), 2);
    }

    // Req 8
    void test_setLimits_loweringPerHostCap_newGrantsWaitUntilUnderNewCap()
    {
        WorkScheduler scheduler(makeLimits(10, 3));
        const Ticket r1 = scheduler.request(kModrinth, WorkPriority::Foreground);
        const Ticket r2 = scheduler.request(kModrinth, WorkPriority::Foreground);
        const Ticket r3 = scheduler.request(kModrinth, WorkPriority::Foreground);
        const Ticket queued = scheduler.request(kModrinth, WorkPriority::Foreground);
        QSignalSpy spy(&scheduler, &WorkScheduler::granted);

        scheduler.setLimits(makeLimits(10, 1));
        QCOMPARE(scheduler.runningCount(kModrinth), 3);

        QVERIFY(scheduler.release(r1));
        QCOMPARE(scheduler.runningCount(kModrinth), 2);
        QVERIFY(scheduler.release(r2));
        QCOMPARE(scheduler.runningCount(kModrinth), 1);
        QVERIFY(!scheduler.isGranted(queued));
        QCOMPARE(spy.count(), 0);

        QVERIFY(scheduler.release(r3));
        QCOMPARE(grantedTickets(spy), (QList<Ticket>{ queued }));
        QCOMPARE(scheduler.runningCount(kModrinth), 1);
    }

    // Req 8
    void test_setLimits_raisingGlobalCap_grantsQueuedAtOnce()
    {
        WorkScheduler scheduler(makeLimits(1, 10));
        scheduler.request(kModrinth, WorkPriority::Foreground);
        const Ticket q2 = scheduler.request(kForge, WorkPriority::Foreground);
        const Ticket q3 = scheduler.request(kCdn, WorkPriority::Foreground);
        const Ticket q4 = scheduler.request(kGithub, WorkPriority::Foreground);
        QSignalSpy spy(&scheduler, &WorkScheduler::granted);

        scheduler.setLimits(makeLimits(3, 10));

        QCOMPARE(grantedTickets(spy), (QList<Ticket>{ q2, q3 }));
        QCOMPARE(scheduler.runningCount(), 3);
        QCOMPARE(scheduler.queuedCount(), 1);
        QVERIFY(!scheduler.isGranted(q4));
    }

    // Req 8 (raising a cap through hostOverrides)
    void test_setLimits_raisingHostOverride_grantsQueuedAtOnce()
    {
        WorkScheduler scheduler(makeLimits(10, 1));
        scheduler.request(kForge, WorkPriority::Foreground);
        const Ticket q2 = scheduler.request(kForge, WorkPriority::Foreground);
        const Ticket q3 = scheduler.request(kForge, WorkPriority::Foreground);
        QSignalSpy spy(&scheduler, &WorkScheduler::granted);

        scheduler.setLimits(makeLimits(10, 1, { { kForge, 3 } }));

        QCOMPARE(grantedTickets(spy), (QList<Ticket>{ q2, q3 }));
        QCOMPARE(scheduler.runningCount(kForge), 3);
        QCOMPARE(scheduler.queuedCount(), 0);
    }

    // Req 9
    void test_hostKey_lowercasesHost_data()
    {
        QTest::addColumn<QUrl>("url");
        QTest::addColumn<QString>("expected");
        QTest::newRow("modrinth") << QUrl("https://API.Modrinth.com/v2/x") << QString("api.modrinth.com");
        QTest::newRow("forgecdn") << QUrl("https://EDGE.ForgeCDN.net/files/1/2/mod.jar") << QString("edge.forgecdn.net");
    }
    void test_hostKey_lowercasesHost()
    {
        QFETCH(QUrl, url);
        QFETCH(QString, expected);
        QCOMPARE(WorkScheduler::hostKey(url), expected);
    }

    // Req 9
    void test_hostKey_invalidOrHostlessUrl_returnsEmpty_data()
    {
        QTest::addColumn<QUrl>("url");
        QTest::newRow("default-constructed") << QUrl();
        QTest::newRow("unterminated IPv6 host") << QUrl("http://[::1");
        QTest::newRow("file url") << QUrl("file:///C:/mods/sodium.jar");
        QTest::newRow("relative path") << QUrl("mods/sodium.jar");
    }
    void test_hostKey_invalidOrHostlessUrl_returnsEmpty()
    {
        QFETCH(QUrl, url);
        QCOMPARE(WorkScheduler::hostKey(url), QString(""));
    }

    // Req 10
    void test_limitsBelowOne_globalMaxTreatedAsOne_data()
    {
        QTest::addColumn<int>("globalMax");
        QTest::newRow("zero") << 0;
        QTest::newRow("negative") << -3;
    }
    void test_limitsBelowOne_globalMaxTreatedAsOne()
    {
        QFETCH(int, globalMax);
        WorkScheduler scheduler(makeLimits(globalMax, 5));
        const Ticket t1 = scheduler.request(kModrinth, WorkPriority::Foreground);
        const Ticket t2 = scheduler.request(kForge, WorkPriority::Foreground);
        scheduler.request(kCdn, WorkPriority::Foreground);
        QSignalSpy spy(&scheduler, &WorkScheduler::granted);

        QVERIFY(scheduler.isGranted(t1));
        QCOMPARE(scheduler.runningCount(), 1);
        QCOMPARE(scheduler.queuedCount(), 2);

        QVERIFY(scheduler.release(t1));
        QCOMPARE(grantedTickets(spy), (QList<Ticket>{ t2 }));
        QCOMPARE(scheduler.runningCount(), 1);
    }

    // Req 10
    void test_limitsBelowOne_perHostMaxTreatedAsOne_data()
    {
        QTest::addColumn<int>("perHostMax");
        QTest::newRow("zero") << 0;
        QTest::newRow("negative") << -3;
    }
    void test_limitsBelowOne_perHostMaxTreatedAsOne()
    {
        QFETCH(int, perHostMax);
        WorkScheduler scheduler(makeLimits(10, perHostMax));
        for (int i = 0; i < 2; ++i) {
            scheduler.request(kModrinth, WorkPriority::Foreground);
            scheduler.request(kForge, WorkPriority::Foreground);
        }

        QCOMPARE(scheduler.runningCount(kModrinth), 1);
        QCOMPARE(scheduler.runningCount(kForge), 1);
        QCOMPARE(scheduler.runningCount(), 2);
        QCOMPARE(scheduler.queuedCount(), 2);
    }

    // Req 10 (assumes override values below 1 are clamped too, not ignored)
    void test_limitsBelowOne_hostOverrideTreatedAsOne()
    {
        WorkScheduler scheduler(makeLimits(10, 3, { { kForge, 0 } }));
        for (int i = 0; i < 3; ++i)
            scheduler.request(kForge, WorkPriority::Foreground);

        QCOMPARE(scheduler.runningCount(kForge), 1);
        QCOMPARE(scheduler.queuedCount(), 2);
    }

    // Req 10 (limits applied through setLimits() are clamped the same way)
    void test_setLimits_belowOne_treatedAsOne()
    {
        WorkScheduler scheduler(makeLimits(1, 10));
        const Ticket t1 = scheduler.request(kModrinth, WorkPriority::Foreground);
        const Ticket q2 = scheduler.request(kForge, WorkPriority::Foreground);
        scheduler.request(kCdn, WorkPriority::Foreground);
        QSignalSpy spy(&scheduler, &WorkScheduler::granted);

        scheduler.setLimits(makeLimits(0, 10));
        QCOMPARE(spy.count(), 0);

        QVERIFY(scheduler.release(t1));
        QCOMPARE(grantedTickets(spy), (QList<Ticket>{ q2 }));
        QCOMPARE(scheduler.runningCount(), 1);
        QCOMPARE(scheduler.queuedCount(), 1);
    }
};

QTEST_GUILESS_MAIN(WorkSchedulerTest)

#include "WorkScheduler_test.moc"
