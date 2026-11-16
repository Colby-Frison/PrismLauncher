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

#include "LaunchCoordinator.h"

#include <QMetaObject>

LaunchCoordinator::LaunchCoordinator(ILaunchExtrasStore* extras,
                                     IRunningInstanceCounter* running,
                                     bool updateRunning,
                                     AccountCoordinator* accounts)
    : m_extras(extras), m_running(running), m_accounts(accounts), m_updateRunning(updateRunning)
{
}

LaunchCoordinator::LaunchOutcome LaunchCoordinator::requestLaunch(const LaunchRequest& request)
{
    if (!request.instance) {
        return LaunchOutcome::Rejected;
    }
    if (m_updateRunning) {
        return LaunchOutcome::BlockedByUpdate;
    }
    if (request.instance->canLaunch()) {
        if (!m_extras->saveOpenWindow(request.instance->id())) {
            return LaunchOutcome::Rejected;
        }

        auto controller = std::make_unique<LaunchController>();
        controller->setInstance(request.instance);
        controller->setLaunchMode(request.mode);
        controller->setProfiler(request.profiler);
        controller->setTargetToJoin(request.targetToJoin);
        controller->setAccountToUse(request.accountToUse);
        controller->setOfflineName(request.offlineName);
        controller->setParentWidget(request.parentWidget);
        controller->setAccountCoordinator(m_accounts);

        auto* raw = controller.get();
        m_extras->setController(request.instance->id(), std::move(controller));
        m_running->addRunningInstance();
        QMetaObject::invokeMethod(raw, &Task::start, Qt::QueuedConnection);
        return LaunchOutcome::Started;
    }
    if (request.instance->isRunning()) {
        return LaunchOutcome::ShowConsoleBecauseRunning;
    }
    if (request.instance->canEdit()) {
        return LaunchOutcome::ShowInstanceBecauseEditable;
    }
    return LaunchOutcome::Rejected;
}

bool LaunchCoordinator::requestKill(BaseInstance* instance)
{
    if (!instance) {
        return false;
    }
    auto* controller = m_extras->controller(instance->id());
    if (!controller) {
        return false;
    }
    return controller->abort();
}

void LaunchCoordinator::setUpdateRunning(bool running)
{
    m_updateRunning = running;
}
