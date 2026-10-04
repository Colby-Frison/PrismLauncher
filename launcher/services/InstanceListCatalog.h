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

#include "services/InstanceCatalog.h"

/**
 * InstanceCatalog wrapper over InstanceList. Does not take ownership.
 */
class InstanceListCatalog : public InstanceCatalog {
   public:
    explicit InstanceListCatalog(InstanceList* list);
    ~InstanceListCatalog() override = default;

    int count() const override;
    MinecraftInstance* at(int index) const override;
    MinecraftInstance* getById(const QString& id) const override;
    MinecraftInstance* getByManagedName(const QString& managedName) const override;

    InstanceList::InstListError loadList() override;
    void saveNow() override;

    QStringList getGroups() override;
    GroupId getInstanceGroup(const InstanceId& id) const override;
    void setInstanceGroup(const InstanceId& id, GroupId name) override;

    bool trashInstance(const InstanceId& id) override;
    void deleteInstance(const InstanceId& id) override;

    Task* wrapInstanceTask(InstanceTask* task) override;
    QString getStagedInstancePath(const QString& targetDir) override;
    bool commitStagedInstance(const QString& keyPath, const InstanceTask& instanceTask, QString groupName) override;

    QString primaryDir() const override;
    InstanceList* model() override;

   private:
    InstanceList* m_list = nullptr;
};
