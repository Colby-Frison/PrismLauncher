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

#include "InstanceCatalog.h"

#include <QDebug>
#include <QFileInfo>
#include <QJsonArray>
#include <algorithm>

#include "BaseInstance.h"

namespace {
constexpr int g_CATALOG_FORMAT_VERSION = 1;

void insertKey(QHash<QString, QSet<QString>>& index, const QString& key, const QString& id)
{
    if (key.isEmpty()) {
        return;
    }
    index[key].insert(id);
}

void removeKey(QHash<QString, QSet<QString>>& index, const QString& key, const QString& id)
{
    if (key.isEmpty()) {
        return;
    }
    auto it = index.find(key);
    if (it == index.end()) {
        return;
    }
    it->remove(id);
    if (it->isEmpty()) {
        index.erase(it);
    }
}
}  // namespace

// ---------------------------------------------------------------------------------------------
// InstanceRecord
// ---------------------------------------------------------------------------------------------

QJsonObject InstanceRecord::toJson() const
{
    QJsonObject obj;
    obj.insert("id", id);
    obj.insert("uuid", uuid);
    obj.insert("name", name);
    obj.insert("type", type);
    obj.insert("rootDir", rootDir);
    obj.insert("instanceRoot", instanceRoot);
    obj.insert("iconKey", iconKey);
    obj.insert("managedPack", managedPack);
    obj.insert("managedPackType", managedPackType);
    obj.insert("managedPackId", managedPackId);
    obj.insert("managedPackName", managedPackName);
    obj.insert("managedPackVersionId", managedPackVersionId);
    obj.insert("managedPackVersionName", managedPackVersionName);
    // Stored as double: exact for any value below 2^53 (ms timestamps / seconds played are far below).
    obj.insert("lastLaunch", static_cast<double>(lastLaunch));
    obj.insert("totalTimePlayed", static_cast<double>(totalTimePlayed));
    return obj;
}

std::optional<InstanceRecord> InstanceRecord::fromJson(const QJsonObject& obj)
{
    if (!obj.value("id").isString()) {
        return std::nullopt;
    }
    InstanceRecord record;
    record.id = obj.value("id").toString();
    if (!record.isValid()) {
        return std::nullopt;
    }
    record.uuid = obj.value("uuid").toString();
    record.name = obj.value("name").toString();
    record.type = obj.value("type").toString();
    record.rootDir = obj.value("rootDir").toString();
    record.instanceRoot = obj.value("instanceRoot").toString();
    record.iconKey = obj.value("iconKey").toString();
    record.managedPack = obj.value("managedPack").toBool(false);
    record.managedPackType = obj.value("managedPackType").toString();
    record.managedPackId = obj.value("managedPackId").toString();
    record.managedPackName = obj.value("managedPackName").toString();
    record.managedPackVersionId = obj.value("managedPackVersionId").toString();
    record.managedPackVersionName = obj.value("managedPackVersionName").toString();
    record.lastLaunch = static_cast<qint64>(obj.value("lastLaunch").toDouble(0));
    record.totalTimePlayed = static_cast<qint64>(obj.value("totalTimePlayed").toDouble(0));
    return record;
}

InstanceRecord InstanceRecord::fromInstance(const BaseInstance& inst)
{
    InstanceRecord record;
    record.id = inst.id();
    record.uuid = inst.uuid();
    record.name = inst.name();
    record.type = inst.instanceType();
    record.instanceRoot = inst.instanceRoot();
    record.rootDir = QFileInfo(record.instanceRoot).absolutePath();
    record.iconKey = inst.iconKey();
    record.managedPack = inst.isManagedPack();
    record.managedPackType = inst.getManagedPackType();
    record.managedPackId = inst.getManagedPackID();
    record.managedPackName = inst.getManagedPackName();
    record.managedPackVersionId = inst.getManagedPackVersionID();
    record.managedPackVersionName = inst.getManagedPackVersionName();
    record.lastLaunch = inst.lastLaunch();
    record.totalTimePlayed = inst.totalTimePlayed();
    return record;
}

// ---------------------------------------------------------------------------------------------
// InstanceCatalog
// ---------------------------------------------------------------------------------------------

InstanceCatalog::AddResult InstanceCatalog::add(const InstanceRecord& record)
{
    if (!record.isValid()) {
        return AddResult::InvalidRecord;
    }
    if (m_records.contains(record.id)) {
        return AddResult::DuplicateId;
    }
    m_records.insert(record.id, record);
    indexRecord(record);
    return AddResult::Added;
}

bool InstanceCatalog::update(const InstanceRecord& record)
{
    auto it = m_records.find(record.id);
    if (it == m_records.end()) {
        return false;
    }
    unindexRecord(*it);
    *it = record;
    indexRecord(*it);
    return true;
}

bool InstanceCatalog::remove(const QString& id)
{
    auto it = m_records.find(id);
    if (it == m_records.end()) {
        return false;
    }
    unindexRecord(*it);
    m_records.erase(it);
    return true;
}

void InstanceCatalog::clear()
{
    m_records.clear();
    m_byUuid.clear();
    m_byName.clear();
    m_byType.clear();
    m_byManagedPackName.clear();
}

const InstanceRecord* InstanceCatalog::findById(const QString& id) const
{
    auto it = m_records.constFind(id);
    if (it == m_records.constEnd()) {
        return nullptr;
    }
    return &it.value();
}

std::optional<InstanceRecord> InstanceCatalog::getById(const QString& id) const
{
    if (const auto* record = findById(id)) {
        return *record;
    }
    return std::nullopt;
}

QList<InstanceRecord> InstanceCatalog::findByUuid(const QString& uuid) const
{
    return resolve(m_byUuid, uuid);
}

QList<InstanceRecord> InstanceCatalog::findByName(const QString& name) const
{
    return resolve(m_byName, name);
}

QList<InstanceRecord> InstanceCatalog::findByType(const QString& type) const
{
    return resolve(m_byType, type);
}

QList<InstanceRecord> InstanceCatalog::findByManagedPackName(const QString& managedPackName) const
{
    return resolve(m_byManagedPackName, managedPackName);
}

QList<InstanceRecord> InstanceCatalog::list() const
{
    ++m_fullScans;
    return m_records.values();  // QMap: ascending id order
}

QStringList InstanceCatalog::ids() const
{
    ++m_fullScans;
    return m_records.keys();
}

MigrationReport InstanceCatalog::migrateFrom(LegacyInstanceSource& source)
{
    MigrationReport report;
    const QStringList discovered = source.discoverIds();
    report.discovered = static_cast<int>(discovered.size());

    QSet<QString> seen;
    for (const auto& id : discovered) {
        if (seen.contains(id)) {
            qWarning() << "Instance catalog migration: duplicate legacy id" << id << "- skipping";
            ++report.skipped;
            report.skippedIds << id;
            continue;
        }
        seen.insert(id);

        if (m_records.contains(id)) {
            ++report.alreadyPresent;
            continue;
        }

        auto record = source.loadRecord(id);
        if (!record || !record->isValid() || record->id != id) {
            qWarning() << "Instance catalog migration: legacy instance" << id << "is corrupt or invalid - skipping";
            ++report.skipped;
            report.skippedIds << id;
            continue;
        }

        add(*record);
        ++report.migrated;
    }
    return report;
}

QJsonObject InstanceCatalog::toJson() const
{
    ++m_fullScans;
    QJsonArray instances;
    for (const auto& record : m_records) {
        instances.append(record.toJson());
    }
    QJsonObject root;
    root.insert("formatVersion", g_CATALOG_FORMAT_VERSION);
    root.insert("instances", instances);
    return root;
}

std::optional<InstanceCatalog> InstanceCatalog::fromJson(const QJsonObject& root, QString* error)
{
    auto fail = [error](const QString& message) -> std::optional<InstanceCatalog> {
        if (error) {
            *error = message;
        }
        return std::nullopt;
    };

    if (root.value("formatVersion").toInt(-1) != g_CATALOG_FORMAT_VERSION) {
        return fail(QStringLiteral("Unsupported instance catalog format version"));
    }
    if (!root.value("instances").isArray()) {
        return fail(QStringLiteral("Instance catalog has no 'instances' array"));
    }

    InstanceCatalog catalog;
    const QJsonArray instances = root.value("instances").toArray();
    for (const auto& value : instances) {
        auto record = value.isObject() ? InstanceRecord::fromJson(value.toObject()) : std::nullopt;
        if (!record) {
            qWarning() << "Instance catalog: skipping malformed entry";
            continue;
        }
        if (catalog.add(*record) != AddResult::Added) {
            qWarning() << "Instance catalog: skipping duplicate entry" << record->id;
        }
    }
    return catalog;
}

void InstanceCatalog::indexRecord(const InstanceRecord& record)
{
    insertKey(m_byUuid, record.uuid, record.id);
    insertKey(m_byName, record.name, record.id);
    insertKey(m_byType, record.type, record.id);
    insertKey(m_byManagedPackName, record.managedPackName, record.id);
}

void InstanceCatalog::unindexRecord(const InstanceRecord& record)
{
    removeKey(m_byUuid, record.uuid, record.id);
    removeKey(m_byName, record.name, record.id);
    removeKey(m_byType, record.type, record.id);
    removeKey(m_byManagedPackName, record.managedPackName, record.id);
}

QList<InstanceRecord> InstanceCatalog::resolve(const SecondaryIndex& index, const QString& key) const
{
    QList<InstanceRecord> out;
    if (key.isEmpty()) {
        return out;
    }
    auto it = index.constFind(key);
    if (it == index.constEnd()) {
        return out;
    }
    QStringList matchIds(it->begin(), it->end());
    std::sort(matchIds.begin(), matchIds.end());
    out.reserve(matchIds.size());
    for (const auto& id : matchIds) {
        out.append(m_records.value(id));
    }
    return out;
}
