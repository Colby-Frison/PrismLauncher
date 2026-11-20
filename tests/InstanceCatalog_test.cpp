// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

// Unit tests for R5 - Indexed instance catalog.
//
// No real filesystem access: the legacy storage is replaced by FakeLegacySource (in-memory), and
// persistence is exercised through in-memory JSON round trips. Every test builds its own catalog
// and source, so there is no shared mutable state between tests.

#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>
#include <QTest>

#include <algorithm>
#include <optional>

#include "InstanceCatalog.h"

namespace {

// Builds a record with every field populated with deterministic, id-derived values so that
// field-by-field comparisons are meaningful.
InstanceRecord makeRecord(const QString& id, const QString& name = {}, const QString& type = QStringLiteral("OneSix"))
{
    InstanceRecord r;
    r.id = id;
    r.uuid = QStringLiteral("uuid-") + id;
    r.name = name.isNull() ? QStringLiteral("Name ") + id : name;
    r.type = type;
    r.rootDir = QStringLiteral("/fake/instances");
    r.instanceRoot = QStringLiteral("/fake/instances/") + id;
    r.iconKey = QStringLiteral("icon-") + id;
    r.managedPack = true;
    r.managedPackType = QStringLiteral("modrinth");
    r.managedPackId = QStringLiteral("pack-") + id;
    r.managedPackName = QStringLiteral("Pack ") + id;
    r.managedPackVersionId = QStringLiteral("ver-") + id;
    r.managedPackVersionName = QStringLiteral("1.0.") + id;
    r.lastLaunch = 1700000000000LL + id.size();
    r.totalTimePlayed = 3600 + 7 * id.size();
    return r;
}

// Minimal record: only the id is set; every optional field is left at its default.
InstanceRecord makeMinimalRecord(const QString& id)
{
    InstanceRecord r;
    r.id = id;
    return r;
}

// In-memory stand-in for the legacy directory scan. Counts how often discovery and per-instance
// loads happen so tests can assert lookups never go back to "the filesystem".
class FakeLegacySource : public LegacyInstanceSource {
   public:
    // Adds a well-formed entry.
    void addEntry(const InstanceRecord& record)
    {
        m_order << record.id;
        m_entries.insert(record.id, record);
    }
    // Adds a discovered id whose data cannot be loaded (e.g. corrupt instance.cfg).
    void addCorruptEntry(const QString& id)
    {
        m_order << id;
        m_entries.insert(id, std::nullopt);
    }
    // Adds a discovered id that loads as an arbitrary (possibly inconsistent) record.
    void addRawEntry(const QString& discoveredId, const InstanceRecord& loaded)
    {
        m_order << discoveredId;
        m_entries.insert(discoveredId, loaded);
    }

    QStringList discoverIds() override
    {
        ++discoverCalls;
        return m_order;
    }

    std::optional<InstanceRecord> loadRecord(const QString& id) override
    {
        ++loadCalls;
        return m_entries.value(id, std::nullopt);
    }

    void resetCounters()
    {
        discoverCalls = 0;
        loadCalls = 0;
    }

    int discoverCalls = 0;
    int loadCalls = 0;

   private:
    QStringList m_order;
    QHash<QString, std::optional<InstanceRecord>> m_entries;
};

FakeLegacySource makeSource(int count)
{
    FakeLegacySource source;
    for (int i = 0; i < count; ++i) {
        source.addEntry(makeRecord(QStringLiteral("inst-%1").arg(i, 5, 10, QChar('0'))));
    }
    return source;
}

QStringList idsOf(const QList<InstanceRecord>& records)
{
    QStringList out;
    for (const auto& r : records) {
        out << r.id;
    }
    return out;
}

}  // namespace

// Field-by-field comparison (not just ids) so a test failure names the exact field that was lost.
#define VERIFY_RECORD_FIELDS(actual, expected)                                   \
    do {                                                                         \
        const InstanceRecord& a_ = (actual);                                     \
        const InstanceRecord& e_ = (expected);                                   \
        QCOMPARE(a_.id, e_.id);                                                  \
        QCOMPARE(a_.uuid, e_.uuid);                                              \
        QCOMPARE(a_.name, e_.name);                                              \
        QCOMPARE(a_.type, e_.type);                                              \
        QCOMPARE(a_.rootDir, e_.rootDir);                                        \
        QCOMPARE(a_.instanceRoot, e_.instanceRoot);                              \
        QCOMPARE(a_.iconKey, e_.iconKey);                                        \
        QCOMPARE(a_.managedPack, e_.managedPack);                                \
        QCOMPARE(a_.managedPackType, e_.managedPackType);                        \
        QCOMPARE(a_.managedPackId, e_.managedPackId);                            \
        QCOMPARE(a_.managedPackName, e_.managedPackName);                        \
        QCOMPARE(a_.managedPackVersionId, e_.managedPackVersionId);              \
        QCOMPARE(a_.managedPackVersionName, e_.managedPackVersionName);          \
        QCOMPARE(a_.lastLaunch, e_.lastLaunch);                                  \
        QCOMPARE(a_.totalTimePlayed, e_.totalTimePlayed);                        \
    } while (false)

class InstanceCatalogTest : public QObject {
    Q_OBJECT

   private slots:
    // ---------------------------------------------------------------------------------------
    // 1. Indexed lookup
    // ---------------------------------------------------------------------------------------

    void test_getById_returnsCorrectInstanceForExistingId()
    {
        InstanceCatalog catalog;
        const auto expected = makeRecord("beta");
        catalog.add(makeRecord("alpha"));
        catalog.add(expected);
        catalog.add(makeRecord("gamma"));

        const auto found = catalog.getById("beta");

        QVERIFY(found.has_value());
        VERIFY_RECORD_FIELDS(*found, expected);
    }

    // The code specifies: getById -> std::nullopt, findById -> nullptr for a missing id (no throw).
    void test_getById_returnsNulloptForUnknownId()
    {
        InstanceCatalog catalog;
        catalog.add(makeRecord("alpha"));

        const auto found = catalog.getById("does-not-exist");
        const auto* foundPtr = catalog.findById("does-not-exist");

        QVERIFY(!found.has_value());
        QVERIFY(foundPtr == nullptr);
    }

    void test_getById_returnsNulloptForEmptyId()
    {
        InstanceCatalog catalog;
        catalog.add(makeRecord("alpha"));

        QVERIFY(!catalog.getById(QString()).has_value());
    }

    // Lookups after the initial load must not call discovery/loading again, nor visit every record.
    void test_lookups_doNotTriggerScanOrIterateAfterInitialLoad()
    {
        InstanceCatalog catalog;
        auto source = makeSource(50);
        catalog.migrateFrom(source);
        source.resetCounters();
        const auto scansBefore = catalog.fullScanCount();

        for (int i = 0; i < 50; ++i) {
            QVERIFY(catalog.findById(QStringLiteral("inst-%1").arg(i, 5, 10, QChar('0'))) != nullptr);
        }
        QCOMPARE(catalog.findByName("Name inst-00007").size(), qsizetype(1));
        QCOMPARE(catalog.findByUuid("uuid-inst-00008").size(), qsizetype(1));
        QCOMPARE(catalog.findByType("OneSix").size(), qsizetype(50));
        QCOMPARE(catalog.findByManagedPackName("Pack inst-00009").size(), qsizetype(1));
        QVERIFY(!catalog.getById("missing").has_value());

        QCOMPARE(source.discoverCalls, 0);
        QCOMPARE(source.loadCalls, 0);
        QCOMPARE(catalog.fullScanCount(), scansBefore);
    }

    void test_findByName_returnsExactlyMatchingInstances()
    {
        InstanceCatalog catalog;
        catalog.add(makeRecord("a", "Survival"));
        catalog.add(makeRecord("b", "Creative"));
        catalog.add(makeRecord("c", "Survival"));
        catalog.add(makeRecord("d", "survival"));  // name index is case-sensitive

        const auto matches = catalog.findByName("Survival");

        QCOMPARE(idsOf(matches), QStringList({ "a", "c" }));
    }

    void test_findByType_returnsExactlyMatchingInstances()
    {
        InstanceCatalog catalog;
        catalog.add(makeRecord("a", {}, "OneSix"));
        catalog.add(makeRecord("b", {}, "Legacy"));
        catalog.add(makeRecord("c", {}, "OneSix"));

        QCOMPARE(idsOf(catalog.findByType("OneSix")), QStringList({ "a", "c" }));
        QCOMPARE(idsOf(catalog.findByType("Legacy")), QStringList({ "b" }));
        QVERIFY(catalog.findByType("Unknown").isEmpty());
    }

    void test_findByUuid_returnsMatchingInstance()
    {
        InstanceCatalog catalog;
        catalog.add(makeRecord("a"));
        catalog.add(makeRecord("b"));

        QCOMPARE(idsOf(catalog.findByUuid("uuid-b")), QStringList({ "b" }));
        QVERIFY(catalog.findByUuid("uuid-zzz").isEmpty());
    }

    void test_findByManagedPackName_returnsExactlyMatchingInstances()
    {
        InstanceCatalog catalog;
        auto a = makeRecord("a");
        auto b = makeRecord("b");
        auto c = makeRecord("c");
        a.managedPackName = "Fabulously Optimized";
        b.managedPackName = "Better MC";
        c.managedPackName = "Fabulously Optimized";
        catalog.add(a);
        catalog.add(b);
        catalog.add(c);

        QCOMPARE(idsOf(catalog.findByManagedPackName("Fabulously Optimized")), QStringList({ "a", "c" }));
    }

    // Assumption: empty keys are never indexed, so querying with an empty key matches nothing
    // (otherwise every instance without e.g. a managed pack would "match" the empty name).
    void test_secondaryQueries_withEmptyKey_returnNothing()
    {
        InstanceCatalog catalog;
        catalog.add(makeMinimalRecord("bare"));

        QVERIFY(catalog.findByName(QString()).isEmpty());
        QVERIFY(catalog.findByUuid(QString()).isEmpty());
        QVERIFY(catalog.findByType(QString()).isEmpty());
        QVERIFY(catalog.findByManagedPackName(QString()).isEmpty());
    }

    // ---------------------------------------------------------------------------------------
    // 2. Listing
    // ---------------------------------------------------------------------------------------

    void test_list_returnsEveryInstanceExactlyOnce()
    {
        InstanceCatalog catalog;
        const QStringList ids{ "one", "two", "three", "four" };
        for (const auto& id : ids) {
            catalog.add(makeRecord(id));
        }

        const auto listed = idsOf(catalog.list());

        QCOMPARE(listed.size(), ids.size());
        QCOMPARE(QSet<QString>(listed.begin(), listed.end()).size(), ids.size());  // no duplicates
        for (const auto& id : ids) {
            QVERIFY2(listed.contains(id), qPrintable(id));  // none missing
        }
    }

    // The code defines list()/ids() order as ascending by id, independent of insertion order.
    void test_list_orderIsAscendingByIdRegardlessOfInsertionOrder()
    {
        InstanceCatalog catalog;
        for (const auto& id : { "delta", "alpha", "charlie", "bravo" }) {
            catalog.add(makeRecord(id));
        }

        const QStringList expected{ "alpha", "bravo", "charlie", "delta" };
        QCOMPARE(idsOf(catalog.list()), expected);
        QCOMPARE(catalog.ids(), expected);
    }

    void test_list_onEmptyCatalog_returnsEmptyCollection()
    {
        InstanceCatalog catalog;

        QVERIFY(catalog.list().isEmpty());
        QVERIFY(catalog.ids().isEmpty());
        QVERIFY(catalog.isEmpty());
        QCOMPARE(catalog.size(), qsizetype(0));
    }

    // ---------------------------------------------------------------------------------------
    // 3. Index consistency on mutation
    // ---------------------------------------------------------------------------------------

    void test_add_makesInstanceFindableByIdAndEverySecondaryIndex()
    {
        InstanceCatalog catalog;
        const auto record = makeRecord("new-one", "Fresh", "OneSix");

        const auto result = catalog.add(record);

        QCOMPARE(result, InstanceCatalog::AddResult::Added);
        QVERIFY(catalog.contains("new-one"));
        QCOMPARE(idsOf(catalog.findByName("Fresh")), QStringList({ "new-one" }));
        QCOMPARE(idsOf(catalog.findByType("OneSix")), QStringList({ "new-one" }));
        QCOMPARE(idsOf(catalog.findByUuid(record.uuid)), QStringList({ "new-one" }));
        QCOMPARE(idsOf(catalog.findByManagedPackName(record.managedPackName)), QStringList({ "new-one" }));
    }

    void test_update_changingIndexedFields_removesOldEntriesAndAddsNewOnes()
    {
        InstanceCatalog catalog;
        catalog.add(makeRecord("x", "Old Name", "OneSix"));
        auto changed = makeRecord("x", "New Name", "Legacy");
        changed.uuid = "uuid-new";
        changed.managedPackName = "New Pack";

        const bool ok = catalog.update(changed);

        QVERIFY(ok);
        QVERIFY(catalog.findByName("Old Name").isEmpty());
        QVERIFY(catalog.findByType("OneSix").isEmpty());
        QVERIFY(catalog.findByUuid("uuid-x").isEmpty());
        QVERIFY(catalog.findByManagedPackName("Pack x").isEmpty());
        QCOMPARE(idsOf(catalog.findByName("New Name")), QStringList({ "x" }));
        QCOMPARE(idsOf(catalog.findByType("Legacy")), QStringList({ "x" }));
        QCOMPARE(idsOf(catalog.findByUuid("uuid-new")), QStringList({ "x" }));
        QCOMPARE(idsOf(catalog.findByManagedPackName("New Pack")), QStringList({ "x" }));
        VERIFY_RECORD_FIELDS(*catalog.getById("x"), changed);
        QCOMPARE(catalog.size(), qsizetype(1));
    }

    void test_update_keepsOtherInstancesSharingTheOldKey()
    {
        InstanceCatalog catalog;
        catalog.add(makeRecord("a", "Shared"));
        catalog.add(makeRecord("b", "Shared"));

        catalog.update(makeRecord("a", "Renamed"));

        QCOMPARE(idsOf(catalog.findByName("Shared")), QStringList({ "b" }));
        QCOMPARE(idsOf(catalog.findByName("Renamed")), QStringList({ "a" }));
    }

    // The code specifies: update of an unknown id returns false and does not insert it.
    void test_update_unknownId_returnsFalseAndDoesNotInsert()
    {
        InstanceCatalog catalog;

        const bool ok = catalog.update(makeRecord("ghost"));

        QVERIFY(!ok);
        QVERIFY(catalog.isEmpty());
        QVERIFY(catalog.findByName("Name ghost").isEmpty());
    }

    void test_remove_dropsInstanceFromEveryIndexAndFromList()
    {
        InstanceCatalog catalog;
        const auto doomed = makeRecord("doomed", "Bye");
        catalog.add(makeRecord("keeper"));
        catalog.add(doomed);

        const bool ok = catalog.remove("doomed");

        QVERIFY(ok);
        QVERIFY(!catalog.getById("doomed").has_value());
        QVERIFY(catalog.findByName("Bye").isEmpty());
        QVERIFY(catalog.findByUuid(doomed.uuid).isEmpty());
        QVERIFY(catalog.findByManagedPackName(doomed.managedPackName).isEmpty());
        QCOMPARE(idsOf(catalog.findByType("OneSix")), QStringList({ "keeper" }));
        QCOMPARE(idsOf(catalog.list()), QStringList({ "keeper" }));
    }

    void test_remove_unknownId_returnsFalse()
    {
        InstanceCatalog catalog;
        catalog.add(makeRecord("a"));

        QVERIFY(!catalog.remove("nope"));
        QCOMPARE(catalog.size(), qsizetype(1));
    }

    // The code specifies: duplicate ids are rejected (DuplicateId) and the original is kept unchanged.
    void test_add_duplicateId_isRejectedAndOriginalIsUnchanged()
    {
        InstanceCatalog catalog;
        const auto original = makeRecord("dup", "Original");
        catalog.add(original);

        const auto result = catalog.add(makeRecord("dup", "Impostor"));

        QCOMPARE(result, InstanceCatalog::AddResult::DuplicateId);
        QCOMPARE(catalog.size(), qsizetype(1));
        VERIFY_RECORD_FIELDS(*catalog.getById("dup"), original);
        QVERIFY(catalog.findByName("Impostor").isEmpty());
        QCOMPARE(idsOf(catalog.findByName("Original")), QStringList({ "dup" }));
    }

    void test_add_recordWithBlankId_isRejected()
    {
        InstanceCatalog catalog;

        QCOMPARE(catalog.add(makeMinimalRecord(QString())), InstanceCatalog::AddResult::InvalidRecord);
        QCOMPARE(catalog.add(makeMinimalRecord("   ")), InstanceCatalog::AddResult::InvalidRecord);
        QVERIFY(catalog.isEmpty());
    }

    void test_clear_emptiesPrimaryAndSecondaryIndexes()
    {
        InstanceCatalog catalog;
        catalog.add(makeRecord("a", "Name A"));

        catalog.clear();

        QVERIFY(catalog.isEmpty());
        QVERIFY(catalog.findByName("Name A").isEmpty());
        QVERIFY(catalog.findByType("OneSix").isEmpty());
    }

    // ---------------------------------------------------------------------------------------
    // 4. Migration / loading without data loss
    // ---------------------------------------------------------------------------------------

    void test_migrate_nLegacyInstances_producesExactlyNEntries()
    {
        InstanceCatalog catalog;
        auto source = makeSource(25);

        const auto report = catalog.migrateFrom(source);

        QCOMPARE(catalog.size(), qsizetype(25));
        QCOMPARE(report.discovered, 25);
        QCOMPARE(report.migrated, 25);
        QCOMPARE(report.skipped, 0);
        QCOMPARE(report.alreadyPresent, 0);
    }

    void test_migrate_preservesEveryFieldOfEveryInstance()
    {
        InstanceCatalog catalog;
        QList<InstanceRecord> legacy{ makeRecord("a"), makeRecord("b"), makeRecord("c") };
        legacy[1].managedPack = false;  // vary boolean / numeric fields too
        legacy[1].totalTimePlayed = 0;
        FakeLegacySource source;
        for (const auto& r : legacy) {
            source.addEntry(r);
        }

        catalog.migrateFrom(source);

        for (const auto& expected : legacy) {
            const auto* actual = catalog.findById(expected.id);
            QVERIFY2(actual != nullptr, qPrintable(expected.id));
            VERIFY_RECORD_FIELDS(*actual, expected);
        }
    }

    void test_migrate_isIdempotent_noDuplicatesAndNoDataChanges()
    {
        InstanceCatalog catalog;
        auto source = makeSource(10);
        catalog.migrateFrom(source);
        const auto afterFirst = catalog.list();

        const auto second = catalog.migrateFrom(source);

        QCOMPARE(catalog.size(), qsizetype(10));
        QCOMPARE(second.migrated, 0);
        QCOMPARE(second.alreadyPresent, 10);
        QVERIFY(catalog.list() == afterFirst);  // full-record equality, element by element
    }

    // Assumption (as implemented): migration never overwrites an entry already in the catalog,
    // so an existing catalog entry wins over a differing legacy entry with the same id.
    void test_migrate_doesNotOverwriteExistingCatalogEntry()
    {
        InstanceCatalog catalog;
        const auto existing = makeRecord("shared", "Catalog Version");
        catalog.add(existing);
        FakeLegacySource source;
        source.addEntry(makeRecord("shared", "Legacy Version"));

        const auto report = catalog.migrateFrom(source);

        QCOMPARE(report.alreadyPresent, 1);
        QCOMPARE(source.loadCalls, 0);  // not even reloaded
        VERIFY_RECORD_FIELDS(*catalog.getById("shared"), existing);
    }

    void test_migrate_emptyLegacyStore_producesEmptyCatalog()
    {
        InstanceCatalog catalog;
        FakeLegacySource source;

        const auto report = catalog.migrateFrom(source);

        QVERIFY(catalog.isEmpty());
        QCOMPARE(report.discovered, 0);
        QCOMPARE(report.migrated, 0);
        QCOMPARE(report.skipped, 0);
    }

    void test_migrate_instanceWithOptionalFieldsMissing_loadsWithDefaults()
    {
        InstanceCatalog catalog;
        FakeLegacySource source;
        const auto bare = makeMinimalRecord("bare-bones");
        source.addEntry(bare);

        catalog.migrateFrom(source);

        const auto* actual = catalog.findById("bare-bones");
        QVERIFY(actual != nullptr);
        VERIFY_RECORD_FIELDS(*actual, bare);
        QVERIFY(actual->name.isEmpty());
        QVERIFY(!actual->managedPack);
        QCOMPARE(actual->lastLaunch, qint64(0));
    }

    void test_migrate_unicodeAndSpecialCharacters_arePreserved()
    {
        InstanceCatalog catalog;
        FakeLegacySource source;
        auto r = makeRecord(QStringLiteral("Wörld ✨ 世界 (1)"), QStringLiteral("Créatif — «test» 🚀"));
        r.instanceRoot = QStringLiteral("/fake/ïnstances/Wörld ✨ 世界 (1)");
        r.rootDir = QStringLiteral("/fake/ïnstances");
        r.managedPackName = QStringLiteral("Пак & <Co> \"quoted\" 'single'");
        source.addEntry(r);

        catalog.migrateFrom(source);

        QVERIFY(catalog.findById(r.id) != nullptr);
        VERIFY_RECORD_FIELDS(*catalog.findById(r.id), r);
        QCOMPARE(idsOf(catalog.findByName(r.name)), QStringList({ r.id }));
        QCOMPARE(idsOf(catalog.findByManagedPackName(r.managedPackName)), QStringList({ r.id }));
    }

    // The code specifies: corrupt/invalid legacy entries are skipped and logged (qWarning),
    // reported in MigrationReport, and the valid entries still load.
    void test_migrate_corruptLegacyEntry_isSkippedAndValidEntriesStillLoad()
    {
        InstanceCatalog catalog;
        FakeLegacySource source;
        source.addEntry(makeRecord("good-1"));
        source.addCorruptEntry("corrupt");                                  // unreadable
        source.addRawEntry("blank-id", makeMinimalRecord(QString()));      // loads without an id
        source.addRawEntry("mismatch", makeRecord("something-else"));      // id disagrees with folder
        source.addEntry(makeRecord("good-2"));

        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("corrupt or invalid"));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("corrupt or invalid"));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("corrupt or invalid"));
        const auto report = catalog.migrateFrom(source);

        QCOMPARE(catalog.ids(), QStringList({ "good-1", "good-2" }));
        QCOMPARE(report.migrated, 2);
        QCOMPARE(report.skipped, 3);
        QCOMPARE(report.skippedIds, QStringList({ "corrupt", "blank-id", "mismatch" }));
        QVERIFY(!catalog.contains("something-else"));
    }

    void test_migrate_duplicateIdInLegacySource_isLoadedOnce()
    {
        InstanceCatalog catalog;
        FakeLegacySource source;
        source.addEntry(makeRecord("twice"));
        source.addEntry(makeRecord("twice"));

        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("duplicate legacy id"));
        const auto report = catalog.migrateFrom(source);

        QCOMPARE(catalog.size(), qsizetype(1));
        QCOMPARE(report.migrated, 1);
        QCOMPARE(report.skipped, 1);
    }

    // Optional on-disk form (tested in memory): JSON round trip keeps every field.
    void test_jsonRoundTrip_preservesAllRecordsAndFields()
    {
        InstanceCatalog catalog;
        catalog.add(makeRecord("a"));
        catalog.add(makeMinimalRecord("b"));
        catalog.add(makeRecord(QStringLiteral("ü-ñ-✓"), QStringLiteral("Ünïcödé")));

        const auto restored = InstanceCatalog::fromJson(catalog.toJson());

        QVERIFY(restored.has_value());
        QCOMPARE(restored->ids(), catalog.ids());
        for (const auto& expected : catalog.list()) {
            VERIFY_RECORD_FIELDS(*restored->findById(expected.id), expected);
        }
        QCOMPARE(idsOf(restored->findByName(QStringLiteral("Ünïcödé"))), QStringList({ QStringLiteral("ü-ñ-✓") }));
    }

    void test_fromJson_malformedEntriesAreSkippedValidOnesLoad()
    {
        InstanceCatalog source;
        source.add(makeRecord("valid"));
        auto root = source.toJson();
        auto instances = root.value("instances").toArray();
        instances.append(QJsonValue(42));                                          // not an object
        instances.append(QJsonObject{ { "name", "no id" } });                      // missing id
        instances.append(QJsonObject{ { "id", 7 } });                              // id wrong type
        instances.append(makeRecord("valid", "dup").toJson());                     // duplicate id
        root.insert("instances", instances);

        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("malformed entry"));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("malformed entry"));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("malformed entry"));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("duplicate entry"));
        const auto restored = InstanceCatalog::fromJson(root);

        QVERIFY(restored.has_value());
        QCOMPARE(restored->ids(), QStringList({ "valid" }));
        VERIFY_RECORD_FIELDS(*restored->findById("valid"), makeRecord("valid"));
    }

    void test_fromJson_unsupportedFormatVersion_returnsNulloptWithError()
    {
        QJsonObject root{ { "formatVersion", 999 }, { "instances", QJsonArray() } };
        QString error;

        const auto restored = InstanceCatalog::fromJson(root, &error);

        QVERIFY(!restored.has_value());
        QVERIFY(!error.isEmpty());
    }

    // ---------------------------------------------------------------------------------------
    // 5. Scalability sanity check (call counts, not wall-clock time)
    // ---------------------------------------------------------------------------------------

    void test_largeCatalog_getById_doesNotScanOrIterate()
    {
        constexpr int count = 10000;
        InstanceCatalog catalog;
        auto source = makeSource(count);
        catalog.migrateFrom(source);
        QCOMPARE(catalog.size(), qsizetype(count));
        source.resetCounters();
        const auto scansBefore = catalog.fullScanCount();

        int found = 0;
        for (int i = 0; i < count; ++i) {
            if (catalog.findById(QStringLiteral("inst-%1").arg(i, 5, 10, QChar('0')))) {
                ++found;
            }
        }
        const bool missingFound = catalog.getById("inst-99999").has_value();

        QCOMPARE(found, count);
        QVERIFY(!missingFound);
        QCOMPARE(source.discoverCalls, 0);
        QCOMPARE(source.loadCalls, 0);
        QCOMPARE(catalog.fullScanCount(), scansBefore);
    }

    void test_largeCatalog_secondaryQueries_doNotIterate()
    {
        constexpr int count = 10000;
        InstanceCatalog catalog;
        auto source = makeSource(count);
        catalog.migrateFrom(source);
        const auto scansBefore = catalog.fullScanCount();

        const auto byName = catalog.findByName("Name inst-04242");
        const auto byUuid = catalog.findByUuid("uuid-inst-09999");

        QCOMPARE(idsOf(byName), QStringList({ "inst-04242" }));
        QCOMPARE(idsOf(byUuid), QStringList({ "inst-09999" }));
        QCOMPARE(catalog.fullScanCount(), scansBefore);
    }

    // Sanity check of the instrumentation itself: full listings DO count as scans.
    void test_fullScanCount_incrementsForListingOperations()
    {
        InstanceCatalog catalog;
        catalog.add(makeRecord("a"));
        const auto before = catalog.fullScanCount();

        catalog.list();
        catalog.ids();
        catalog.toJson();

        QCOMPARE(catalog.fullScanCount(), before + 3);
    }

    // ---------------------------------------------------------------------------------------
    // 6. Phase 3 placeholders (incremental filesystem reconciliation - not implemented)
    // ---------------------------------------------------------------------------------------

    // Expected: when a new instance folder appears, the catalog adds only that entry
    // (one loadRecord call for the new id, zero discoverIds calls, existing entries untouched).
    void test_phase3_reconcilesAddedFolderIncrementally()
    {
        QSKIP("Phase 3 (incremental FS reconciliation) is not implemented yet");
    }

    // Expected: when an instance folder is removed or its instance.cfg changes, only that entry
    // is removed/updated in every index, without a full rediscovery pass.
    void test_phase3_reconcilesRemovedOrChangedFolderIncrementally()
    {
        QSKIP("Phase 3 (incremental FS reconciliation) is not implemented yet");
    }
};

QTEST_GUILESS_MAIN(InstanceCatalogTest)

#include "InstanceCatalog_test.moc"
