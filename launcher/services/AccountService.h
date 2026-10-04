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

#include <QModelIndex>
#include <QString>
#include <QStringList>

#include "minecraft/auth/MinecraftAccount.h"

class AccountList;

/**
 * Account list and auth entry-point contract.
 *
 * Backed by AccountList. Covers lookup, default account, refresh queueing, and
 * basic mutation. Full AuthFlow construction stays on MinecraftAccount for now.
 */
class AccountService {
   public:
    virtual ~AccountService() = default;

    virtual int count() const = 0;
    virtual MinecraftAccountPtr at(int index) const = 0;

    virtual void addAccount(MinecraftAccountPtr account) = 0;
    virtual void removeAccount(QModelIndex index) = 0;

    virtual int findAccountByProfileId(const QString& profileId) const = 0;
    virtual MinecraftAccountPtr getAccountByProfileName(const QString& profileName) const = 0;
    virtual QStringList profileNames() const = 0;

    virtual MinecraftAccountPtr defaultAccount() const = 0;
    virtual void setDefaultAccount(MinecraftAccountPtr account) = 0;
    virtual bool anyAccountIsValid() = 0;

    virtual void requestRefresh(QString accountId) = 0;
    virtual void queueRefresh(QString accountId) = 0;

    /**
     * Transitional: Qt views and account-list signals still attach to AccountList.
     */
    virtual AccountList* model() = 0;
};
