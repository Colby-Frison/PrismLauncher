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

#include "LaunchUiHints.h"
#include "services/LauncherWindows.h"
#include "tasks/Task.h"

LaunchCoordinator::LaunchCoordinator(ILaunchExtrasStore* extras,
                                     IRunningInstanceCounter* running,
                                     bool updateRunning,
                                     AccountCoordinator* accounts,
                                     LauncherWindows* windows,
                                     const QMap<QString, std::shared_ptr<BaseProfilerFactory>>* profilers)
    : m_extras(extras)
    , m_running(running)
    , m_accounts(accounts)
    , m_windows(windows)
    , m_profilers(profilers)
    , m_updateRunning(updateRunning)
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

void LaunchCoordinator::setAccountCoordinator(AccountCoordinator* accounts)
{
    m_accounts = accounts;
}

bool LaunchCoordinator::launch(MinecraftInstance* instance,
                               LaunchMode mode,
                               MinecraftTarget::Ptr targetToJoin,
                               MinecraftAccountPtr accountToUse,
                               const QString& offlineName)
{
    if (!instance) {
        return false;
    }

    LaunchRequest request;
    request.instance = instance;
    request.mode = mode;
    request.targetToJoin = std::move(targetToJoin);
    request.accountToUse = std::move(accountToUse);
    request.offlineName = offlineName;
    if (m_profilers) {
        request.profiler = m_profilers->value(instance->settings()->get("Profiler").toString(), nullptr).get();
    }

    const auto outcome = requestLaunch(request);
    return LaunchUiHints::handleLaunchOutcome(outcome, [this, instance](const LaunchUiHints::InstanceWindowHint& hint) {
        if (m_windows) {
            m_windows->showInstanceWindow(instance, hint.page);
        }
    });
}

bool LaunchCoordinator::kill(BaseInstance* instance)
{
    return LaunchUiHints::killInstance(this, m_extras, instance);
}

const QMap<QString, std::shared_ptr<BaseProfilerFactory>>& LaunchCoordinator::profilers() const
{
    static const QMap<QString, std::shared_ptr<BaseProfilerFactory>> empty;
    return m_profilers ? *m_profilers : empty;
}
