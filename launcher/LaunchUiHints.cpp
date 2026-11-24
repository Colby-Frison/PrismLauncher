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

#include "LaunchUiHints.h"

#include <QDebug>

#include "BaseInstance.h"

namespace LaunchUiHints {

InstanceWindowHint instanceWindowHintFor(LaunchCoordinator::LaunchOutcome outcome)
{
    InstanceWindowHint hint;
    switch (outcome) {
        case LaunchCoordinator::LaunchOutcome::ShowConsoleBecauseRunning:
            hint.open = true;
            hint.page = QStringLiteral("console");
            break;
        case LaunchCoordinator::LaunchOutcome::ShowInstanceBecauseEditable:
            hint.open = true;
            break;
        case LaunchCoordinator::LaunchOutcome::Started:
        case LaunchCoordinator::LaunchOutcome::BlockedByUpdate:
        case LaunchCoordinator::LaunchOutcome::Rejected:
            break;
    }
    return hint;
}

bool launchAccepted(LaunchCoordinator::LaunchOutcome outcome)
{
    switch (outcome) {
        case LaunchCoordinator::LaunchOutcome::Started:
        case LaunchCoordinator::LaunchOutcome::ShowConsoleBecauseRunning:
        case LaunchCoordinator::LaunchOutcome::ShowInstanceBecauseEditable:
            return true;
        case LaunchCoordinator::LaunchOutcome::BlockedByUpdate:
        case LaunchCoordinator::LaunchOutcome::Rejected:
            return false;
    }
    return false;
}

bool handleLaunchOutcome(LaunchCoordinator::LaunchOutcome outcome,
                         const std::function<void(const InstanceWindowHint&)>& onShowWindow)
{
    if (outcome == LaunchCoordinator::LaunchOutcome::BlockedByUpdate) {
        qDebug() << "Cannot launch instances while an update is running. Please try again when updates are completed.";
    }
    const auto windowHint = instanceWindowHintFor(outcome);
    if (windowHint.open && onShowWindow) {
        onShowWindow(windowHint);
    }
    return launchAccepted(outcome);
}

bool killInstance(LaunchCoordinator* coordinator, ILaunchExtrasStore* extras, BaseInstance* instance)
{
    if (!instance || !instance->isRunning()) {
        if (instance) {
            qWarning() << "Attempted to kill instance" << instance->id() << ", which isn't running.";
        }
        return false;
    }
    if (!coordinator) {
        return false;
    }
    // Preserve prior UX: running with no controller still reports success.
    if (extras && extras->controller(instance->id())) {
        return coordinator->requestKill(instance);
    }
    return true;
}

}  // namespace LaunchUiHints
