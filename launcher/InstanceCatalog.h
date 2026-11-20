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

#pragma once

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <optional>

class BaseInstance;

/**
 * Plain-data snapshot of the fields of an instance that the catalog indexes and persists.
 * Optional fields are represented by empty strings / zero.
 */
struct InstanceRecord {
    QString id;            ///< Folder name of the instance; primary key.
    QString uuid;          ///< Instance UUID (secondary index).
    QString name;          ///< Display name (secondary index).
    QString type;          ///< Instance type, e.g. "OneSix" (secondary index).
    QString rootDir;       ///< Instance root directory that contains the instance folder.
    QString instanceRoot;  ///< Full path of the instance folder.
    QString iconKey;

    bool managedPack = false;
    QString managedPackType;
    QString managedPackId;
    QString managedPackName;  ///< Secondary index.
    QString managedPackVersionId;
    QString managedPackVersionName;

    qint64 lastLaunch = 0;       ///< Milliseconds since epoch.
    qint64 totalTimePlayed = 0;  ///< Seconds.

    /// A record is valid when it has a non-blank id.
    bool isValid() const { return !id.trimmed().isEmpty(); }

    bool operator==(const InstanceRecord& other) const = default;

    QJsonObject toJson() const;
    /// Returns std::nullopt if @p obj has no string "id" or the id is blank.
    static std::optional<InstanceRecord> fromJson(const QJsonObject& obj);
    /// Snapshot the indexed fields of a live instance.
    static InstanceRecord fromInstance(const BaseInstance& inst);
};

/**
 * The pre-catalog way of finding instances (directory discovery + per-instance load).
 * Used as the migration source when the catalog is populated for the first time.
 */
class LegacyInstanceSource {
   public:
    virtual ~LegacyInstanceSource() = default;
    /// Full discovery pass over the legacy storage. Returns the ids found, in discovery order.
    virtual QStringList discoverIds() = 0;
    /// Load a single legacy instance. Returns std::nullopt if the entry is corrupt/unreadable.
    virtual std::optional<InstanceRecord> loadRecord(const QString& id) = 0;
};

struct MigrationReport {
    int discovered = 0;      ///< Ids returned by the legacy discovery pass.
    int migrated = 0;        ///< New entries added to the catalog.
    int alreadyPresent = 0;  ///< Ids that were already in the catalog (left untouched).
    int skipped = 0;         ///< Corrupt, invalid, mismatched or duplicate legacy entries.
    QStringList skippedIds;
};

/**
 * Indexed, in-memory instance catalog.
 *
 * Primary index: instance id (ordered map, O(log n) lookup).
 * Secondary indexes (hash -> id set): uuid, name, type, managed pack name.
 *
 * Lookups never scan all records. Operations that do visit every record (list(), ids(), toJson())
 * increment fullScanCount() so callers/tests can verify that lookups stay off the O(n) path.
 *
 * Phase 3 (incremental filesystem reconciliation) is not implemented here.
 */
class InstanceCatalog {
   public:
    enum class AddResult { Added, InvalidRecord, DuplicateId };

    /// Adds a new record. Duplicate ids are rejected and the existing record is left unchanged.
    AddResult add(const InstanceRecord& record);
    /// Replaces the record with the same id, re-indexing changed fields. Returns false if the id is unknown.
    bool update(const InstanceRecord& record);
    /// Removes the record from the primary and every secondary index. Returns false if the id is unknown.
    bool remove(const QString& id);
    void clear();

    bool contains(const QString& id) const { return m_records.contains(id); }
    qsizetype size() const { return m_records.size(); }
    bool isEmpty() const { return m_records.isEmpty(); }

    /// Returns nullptr if missing. The pointer is invalidated by the next mutation of the catalog.
    const InstanceRecord* findById(const QString& id) const;
    /// Returns std::nullopt if missing.
    std::optional<InstanceRecord> getById(const QString& id) const;

    /// Secondary-index queries. Results are sorted by id. An empty key never matches.
    QList<InstanceRecord> findByUuid(const QString& uuid) const;
    QList<InstanceRecord> findByName(const QString& name) const;
    QList<InstanceRecord> findByType(const QString& type) const;
    QList<InstanceRecord> findByManagedPackName(const QString& managedPackName) const;

    /// Every record exactly once, ordered by id.
    QList<InstanceRecord> list() const;
    /// Every id exactly once, in ascending order.
    QStringList ids() const;

    /**
     * Imports instances from the legacy storage. Ids already in the catalog are not reloaded or
     * modified, so running the migration again is a no-op. Corrupt entries are logged and skipped.
     */
    MigrationReport migrateFrom(LegacyInstanceSource& source);

    QJsonObject toJson() const;
    /// Returns std::nullopt if the document root is unusable; malformed entries are skipped.
    static std::optional<InstanceCatalog> fromJson(const QJsonObject& root, QString* error = nullptr);

    /// Diagnostic: number of operations that visited every record.
    quint64 fullScanCount() const { return m_fullScans; }

   private:
    using SecondaryIndex = QHash<QString, QSet<QString>>;

    void indexRecord(const InstanceRecord& record);
    void unindexRecord(const InstanceRecord& record);
    QList<InstanceRecord> resolve(const SecondaryIndex& index, const QString& key) const;

    QMap<QString, InstanceRecord> m_records;
    SecondaryIndex m_byUuid;
    SecondaryIndex m_byName;
    SecondaryIndex m_byType;
    SecondaryIndex m_byManagedPackName;
    mutable quint64 m_fullScans = 0;
};
