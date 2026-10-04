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

#include "services/LauncherSettings.h"

/**
 * LauncherSettings facade over existing SettingsObject instances.
 * Does not take ownership of either settings object.
 */
class SettingsObjectLauncherSettings : public LauncherSettings {
   public:
    SettingsObjectLauncherSettings(SettingsObject* globalSettings, SettingsObject* playtimeSettings);
    ~SettingsObjectLauncherSettings() override = default;

    QVariant get(const QString& id) override;
    bool set(const QString& id, QVariant value) override;
    void reset(const QString& id) override;
    bool contains(const QString& id) override;
    std::shared_ptr<Setting> getSetting(const QString& id) const override;
    std::shared_ptr<Setting> getOrRegisterSetting(const QString& id, QVariant defVal = QVariant()) override;

    QVariant getPlaytime(const QString& id) override;
    bool setPlaytime(const QString& id, QVariant value) override;

    SettingsObject* settingsObject() override;
    SettingsObject* playtimeSettingsObject() override;

   private:
    SettingsObject* m_global = nullptr;
    SettingsObject* m_playtime = nullptr;
};
