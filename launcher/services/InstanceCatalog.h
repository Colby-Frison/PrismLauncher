// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2026 Prism Launcher Contributors
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

#include <QString>
#include <QStringList>

#include "InstanceList.h"

class InstanceTask;
class MinecraftInstance;
class Task;

/**
 * Repository-style view over the instance catalog.
 *
 * Backed by InstanceList today. Lookup and lifecycle operations belong here;
 * Qt model binding can still use model() during the migration.
 */
class InstanceCatalog {
   public:
    virtual ~InstanceCatalog() = default;

    virtual int count() const = 0;
    virtual MinecraftInstance* at(int index) const = 0;
    virtual MinecraftInstance* getById(const QString& id) const = 0;
    virtual MinecraftInstance* getByManagedName(const QString& managedName) const = 0;

    virtual InstanceList::InstListError loadList() = 0;
    virtual void saveNow() = 0;

    virtual QStringList getGroups() = 0;
    virtual GroupId getInstanceGroup(const InstanceId& id) const = 0;
    virtual void setInstanceGroup(const InstanceId& id, GroupId name) = 0;

    virtual bool trashInstance(const InstanceId& id) = 0;
    virtual void deleteInstance(const InstanceId& id) = 0;

    virtual Task* wrapInstanceTask(InstanceTask* task) = 0;
    virtual QString getStagedInstancePath(const QString& targetDir) = 0;
    virtual bool commitStagedInstance(const QString& keyPath, const InstanceTask& instanceTask, QString groupName) = 0;

    virtual QString primaryDir() const = 0;

    /**
     * Transitional: Qt views and signals still attach to InstanceList.
     * New non-UI code should use the catalog methods above.
     */
    virtual InstanceList* model() = 0;
};
