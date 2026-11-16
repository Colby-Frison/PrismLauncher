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

#include <memory>

#include <QObject>
#include <QString>

class QWidget;

class AccountCoordinator;

#include "BaseInstance.h"
#include "LaunchController.h"
#include "LaunchMode.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/auth/MinecraftAccount.h"
#include "minecraft/launch/MinecraftTarget.h"
#include "tools/BaseProfiler.h"

/**
 * Persists per-instance launch extras (open window save + active LaunchController).
 * Application owns the concrete store; tests use fakes.
 */
class ILaunchExtrasStore {
   public:
    virtual ~ILaunchExtrasStore() = default;

    /** Persist the open instance window if any. false aborts launch start. */
    virtual bool saveOpenWindow(const QString& instanceId) = 0;

    virtual void setController(const QString& instanceId, std::unique_ptr<LaunchController> controller) = 0;
    virtual LaunchController* controller(const QString& instanceId) const = 0;
};

/**
 * Tracks how many instances are currently launching/running for update gating.
 */
class IRunningInstanceCounter {
   public:
    virtual ~IRunningInstanceCounter() = default;
    virtual void addRunningInstance() = 0;
};

/**
 * Owns launch orchestration without UI: decide whether to start a LaunchController,
 * configure and store it, track running, and abort on kill.
 */
class LaunchCoordinator : public QObject {
    Q_OBJECT
   public:
    struct LaunchRequest {
        MinecraftInstance* instance = nullptr;
        LaunchMode mode = LaunchMode::Normal;
        MinecraftTarget::Ptr targetToJoin;
        MinecraftAccountPtr accountToUse;
        QString offlineName;
        BaseProfilerFactory* profiler = nullptr;
        QWidget* parentWidget = nullptr;
    };

    enum class LaunchOutcome {
        Started,
        BlockedByUpdate,
        ShowConsoleBecauseRunning,
        ShowInstanceBecauseEditable,
        Rejected
    };

    explicit LaunchCoordinator(ILaunchExtrasStore* extras,
                               IRunningInstanceCounter* running,
                               bool updateRunning,
                               AccountCoordinator* accounts = nullptr);

    LaunchOutcome requestLaunch(const LaunchRequest& request);
    bool requestKill(BaseInstance* instance);
    void setUpdateRunning(bool running);

   private:
    ILaunchExtrasStore* m_extras = nullptr;
    IRunningInstanceCounter* m_running = nullptr;
    AccountCoordinator* m_accounts = nullptr;
    bool m_updateRunning = false;
};
