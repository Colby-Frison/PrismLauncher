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

class InstanceCatalog;
class InstanceTask;
class LauncherSettings;
class Task;

/**
 * Runs a staged create/import Task (typically a modal progress dialog).
 * Keeps UI presentation out of CreateInstanceCoordinator.
 */
class IInstanceTaskRunner {
   public:
    virtual ~IInstanceTaskRunner() = default;
    virtual bool run(Task* stagedTask) = 0;
};

/**
 * Owns create/import InstanceTask staging+commit sequencing.
 *
 * UI (NewInstanceDialog / MainWindow) collects options and triggers this
 * coordinator; the coordinator wraps via InstanceCatalog and runs the staged
 * task through an injected runner.
 */
class CreateInstanceCoordinator {
   public:
    enum class Outcome {
        Rejected,
        Succeeded,
        Failed,
    };

    struct CreateRequest {
        InstanceTask* task = nullptr;
        QString groupToRemember;
        QString instDirToRemember;
        bool rememberPrefs = false;
    };

    CreateInstanceCoordinator(InstanceCatalog* catalog, LauncherSettings* settings, IInstanceTaskRunner* runner);

    /** Preferred group wins; otherwise LastUsedGroupForNewInstance from settings. */
    QString resolveInitialGroup(const QString& preferredGroup) const;

    /**
     * Optionally persist last-used create prefs, wrap the InstanceTask for
     * staging/commit, and run it. Takes ownership of request.task (deletes on
     * reject if wrap fails; otherwise ownership moves into the staged Task).
     */
    Outcome createFromTask(const CreateRequest& request);

   private:
    InstanceCatalog* m_catalog = nullptr;
    LauncherSettings* m_settings = nullptr;
    IInstanceTaskRunner* m_runner = nullptr;
};
