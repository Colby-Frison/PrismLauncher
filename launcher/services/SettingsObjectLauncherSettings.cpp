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

#include "services/SettingsObjectLauncherSettings.h"

#include "settings/SettingsObject.h"

SettingsObjectLauncherSettings::SettingsObjectLauncherSettings(SettingsObject* globalSettings, SettingsObject* playtimeSettings)
    : m_global(globalSettings), m_playtime(playtimeSettings)
{}

QVariant SettingsObjectLauncherSettings::get(const QString& id)
{
    return m_global->get(id);
}

bool SettingsObjectLauncherSettings::set(const QString& id, QVariant value)
{
    return m_global->set(id, value);
}

void SettingsObjectLauncherSettings::reset(const QString& id)
{
    m_global->reset(id);
}

bool SettingsObjectLauncherSettings::contains(const QString& id)
{
    return m_global->contains(id);
}

std::shared_ptr<Setting> SettingsObjectLauncherSettings::getSetting(const QString& id) const
{
    return m_global->getSetting(id);
}

std::shared_ptr<Setting> SettingsObjectLauncherSettings::getOrRegisterSetting(const QString& id, QVariant defVal)
{
    return m_global->getOrRegisterSetting(id, defVal);
}

QVariant SettingsObjectLauncherSettings::getPlaytime(const QString& id)
{
    return m_playtime->get(id);
}

bool SettingsObjectLauncherSettings::setPlaytime(const QString& id, QVariant value)
{
    return m_playtime->set(id, value);
}

SettingsObject* SettingsObjectLauncherSettings::settingsObject()
{
    return m_global;
}

SettingsObject* SettingsObjectLauncherSettings::playtimeSettingsObject()
{
    return m_playtime;
}
