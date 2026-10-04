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

#include "services/InstanceListCatalog.h"

InstanceListCatalog::InstanceListCatalog(InstanceList* list) : m_list(list) {}

int InstanceListCatalog::count() const
{
    return m_list->count();
}

MinecraftInstance* InstanceListCatalog::at(int index) const
{
    return m_list->at(index);
}

MinecraftInstance* InstanceListCatalog::getById(const QString& id) const
{
    return m_list->getInstanceById(id);
}

MinecraftInstance* InstanceListCatalog::getByManagedName(const QString& managedName) const
{
    return m_list->getInstanceByManagedName(managedName);
}

InstanceList::InstListError InstanceListCatalog::loadList()
{
    return m_list->loadList();
}

void InstanceListCatalog::saveNow()
{
    m_list->saveNow();
}

QStringList InstanceListCatalog::getGroups()
{
    return m_list->getGroups();
}

GroupId InstanceListCatalog::getInstanceGroup(const InstanceId& id) const
{
    return m_list->getInstanceGroup(id);
}

void InstanceListCatalog::setInstanceGroup(const InstanceId& id, GroupId name)
{
    m_list->setInstanceGroup(id, name);
}

bool InstanceListCatalog::trashInstance(const InstanceId& id)
{
    return m_list->trashInstance(id);
}

void InstanceListCatalog::deleteInstance(const InstanceId& id)
{
    m_list->deleteInstance(id);
}

Task* InstanceListCatalog::wrapInstanceTask(InstanceTask* task)
{
    return m_list->wrapInstanceTask(task);
}

QString InstanceListCatalog::getStagedInstancePath(const QString& targetDir)
{
    return m_list->getStagedInstancePath(targetDir);
}

bool InstanceListCatalog::commitStagedInstance(const QString& keyPath, const InstanceTask& instanceTask, QString groupName)
{
    return m_list->commitStagedInstance(keyPath, instanceTask, groupName);
}

QString InstanceListCatalog::primaryDir() const
{
    return m_list->primaryDir();
}

InstanceList* InstanceListCatalog::model()
{
    return m_list;
}
