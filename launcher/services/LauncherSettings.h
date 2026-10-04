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
#include <QVariant>
#include <memory>

class Setting;
class SettingsObject;

/**
 * Narrow settings contract for launcher-wide and playtime configuration.
 *
 * Application remains the composition root and owns the backing SettingsObject
 * instances. Consumers should depend on this interface instead of APPLICATION.
 */
class LauncherSettings {
   public:
    virtual ~LauncherSettings() = default;

    virtual QVariant get(const QString& id) = 0;
    virtual bool set(const QString& id, QVariant value) = 0;
    virtual void reset(const QString& id) = 0;
    virtual bool contains(const QString& id) = 0;
    virtual std::shared_ptr<Setting> getSetting(const QString& id) const = 0;
    virtual std::shared_ptr<Setting> getOrRegisterSetting(const QString& id, QVariant defVal = QVariant()) = 0;

    virtual QVariant getPlaytime(const QString& id) = 0;
    virtual bool setPlaytime(const QString& id, QVariant value) = 0;

    /**
     * Transitional access for call sites that still need SettingsObject*
     * (signals, registration helpers). Prefer the methods above in new code.
     */
    virtual SettingsObject* settingsObject() = 0;
    virtual SettingsObject* playtimeSettingsObject() = 0;
};
