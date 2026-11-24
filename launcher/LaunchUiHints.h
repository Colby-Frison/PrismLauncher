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

#include <functional>

#include <QString>

#include "LaunchCoordinator.h"

class BaseInstance;

/**
 * Pure UI hints derived from LaunchCoordinator outcomes, plus shared kill
 * semantics. Application and MainWindow both use these so show-window UX is
 * not buried inside Application::launch.
 */
namespace LaunchUiHints {

struct InstanceWindowHint {
    bool open = false;
    QString page;
};

/** Map a coordinator launch outcome to whether an instance window should open. */
InstanceWindowHint instanceWindowHintFor(LaunchCoordinator::LaunchOutcome outcome);

/** True when the launch request was accepted (started or redirected to a window). */
bool launchAccepted(LaunchCoordinator::LaunchOutcome outcome);

/**
 * Single facade-layer ownership for launch outcome handling: log update blocks,
 * invoke onShowWindow when a window is required, return launchAccepted.
 * Used by both Application::launch and MainWindow::requestLaunch.
 */
bool handleLaunchOutcome(LaunchCoordinator::LaunchOutcome outcome,
                         const std::function<void(const InstanceWindowHint&)>& onShowWindow);

/**
 * Shared kill semantics previously in Application::kill:
 * - null / not-running → false (caller may log)
 * - running with no controller → true (legacy success)
 * - otherwise → coordinator abort result
 */
bool killInstance(LaunchCoordinator* coordinator, ILaunchExtrasStore* extras, BaseInstance* instance);

}  // namespace LaunchUiHints
