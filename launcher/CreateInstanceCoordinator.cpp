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

#include "CreateInstanceCoordinator.h"

#include "InstanceTask.h"
#include "QObjectPtr.h"
#include "services/InstanceCatalog.h"
#include "services/LauncherSettings.h"
#include "tasks/Task.h"

CreateInstanceCoordinator::CreateInstanceCoordinator(InstanceCatalog* catalog, LauncherSettings* settings, IInstanceTaskRunner* runner)
    : m_catalog(catalog), m_settings(settings), m_runner(runner)
{
}

QString CreateInstanceCoordinator::resolveInitialGroup(const QString& preferredGroup) const
{
    if (!preferredGroup.isEmpty()) {
        return preferredGroup;
    }
    if (!m_settings) {
        return {};
    }
    return m_settings->get(QStringLiteral("LastUsedGroupForNewInstance")).toString();
}

CreateInstanceCoordinator::Outcome CreateInstanceCoordinator::createFromTask(const CreateRequest& request)
{
    if (!request.task || !m_catalog || !m_runner) {
        delete request.task;
        return Outcome::Rejected;
    }

    if (request.rememberPrefs && m_settings) {
        m_settings->set(QStringLiteral("LastUsedGroupForNewInstance"), request.groupToRemember);
        m_settings->set(QStringLiteral("LastUsedInstDirForNewInstance"), request.instDirToRemember);
    }

    // wrapInstanceTask takes ownership of request.task (see InstanceStaging).
    unique_qobject_ptr<Task> staged(m_catalog->wrapInstanceTask(request.task));
    if (!staged) {
        return Outcome::Rejected;
    }

    if (!m_runner->run(staged.get())) {
        return Outcome::Failed;
    }
    return Outcome::Succeeded;
}
